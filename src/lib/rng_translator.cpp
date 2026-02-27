#include <xb/rng_translator.hpp>

#include <stdexcept>
#include <unordered_map>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
    const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

    struct translator {
      schema result;
      std::unordered_map<std::string, const rng::define*> define_map;
      // Track which defines have been translated to avoid duplicates
      std::unordered_map<std::string, qname> translated_types;

      void
      build_define_map(const rng::grammar_pattern& g) {
        for (const auto& d : g.defines) {
          define_map[d.name] = &d;
        }
      }

      // Determine the target namespace from element names in the grammar
      std::string
      infer_namespace(const rng::grammar_pattern& g) {
        for (const auto& d : g.defines) {
          if (d.body && d.body->holds<rng::element_pattern>()) {
            auto& elem = d.body->get<rng::element_pattern>();
            if (elem.name.holds<rng::specific_name>()) {
              auto& ns = elem.name.get<rng::specific_name>().ns;
              if (!ns.empty()) return ns;
            }
          }
        }
        return "";
      }

      // Map a RELAX NG data type to an XSD qname
      qname
      data_type_qname(const rng::data_pattern& dp) {
        if (dp.datatype_library == xsd_dt || dp.datatype_library == xs_ns) {
          return qname(xs_ns, dp.type);
        }
        // Built-in RELAX NG types
        if (dp.datatype_library.empty()) {
          if (dp.type == "string") return qname(xs_ns, "string");
          if (dp.type == "token") return qname(xs_ns, "token");
        }
        return qname(xs_ns, "string"); // fallback
      }

      // Determine the XSD type for a content pattern
      // Returns the type qname, or empty qname if it needs a complex type
      qname
      content_type_name(const rng::pattern& p, const std::string& ns) {
        if (p.holds<rng::text_pattern>()) { return qname(xs_ns, "string"); }
        if (p.holds<rng::data_pattern>()) {
          return data_type_qname(p.get<rng::data_pattern>());
        }
        if (p.holds<rng::empty_pattern>()) {
          return qname(); // empty content
        }
        if (p.holds<rng::ref_pattern>()) {
          auto& ref = p.get<rng::ref_pattern>();
          auto it = define_map.find(ref.name);
          if (it != define_map.end() && it->second->body) {
            // If the referenced define is an element, translate it
            translate_define(*it->second, ns);
            auto tt = translated_types.find(ref.name);
            if (tt != translated_types.end()) return tt->second;
          }
        }
        return qname(); // needs inline complex type
      }

      // Helper to make inner particles optional (min=0)
      void
      make_optional(std::vector<particle>& inner_particles,
                    std::vector<attribute_use>& inner_attrs,
                    std::vector<particle>& particles,
                    std::vector<attribute_use>& attrs) {
        for (auto& ip : inner_particles) {
          if (ip.occurs.is_unbounded()) {
            ip.occurs.min_occurs = 0;
          } else {
            ip.occurs = occurrence{0, ip.occurs.max_occurs};
          }
          particles.push_back(std::move(ip));
        }
        for (auto& ia : inner_attrs) {
          ia.required = false;
          attrs.push_back(std::move(ia));
        }
      }

      // Translate a content pattern into particles for a model_group
      void
      translate_content_particles(const rng::pattern& p, const std::string& ns,
                                  std::vector<particle>& particles,
                                  std::vector<attribute_use>& attrs) {
        if (p.holds<rng::element_pattern>()) {
          auto& elem = p.get<rng::element_pattern>();
          if (elem.name.holds<rng::specific_name>()) {
            auto& sn = elem.name.get<rng::specific_name>();
            qname elem_name(sn.ns, sn.local_name);
            qname type_name;
            if (elem.content) {
              type_name = content_type_name(*elem.content, ns);
              if (type_name == qname()) {
                type_name = qname(ns.empty() ? sn.ns : ns, sn.local_name);
                translate_element_body(elem_name, *elem.content, ns);
              }
            } else {
              type_name = qname(xs_ns, "string");
            }
            particles.emplace_back(element_decl(elem_name, type_name));
          }
        } else if (p.holds<rng::attribute_pattern>()) {
          auto& attr = p.get<rng::attribute_pattern>();
          if (attr.name.holds<rng::specific_name>()) {
            auto& sn = attr.name.get<rng::specific_name>();
            qname attr_name(sn.ns, sn.local_name);
            qname type_name(xs_ns, "string");
            if (attr.content) {
              auto tn = content_type_name(*attr.content, ns);
              if (tn != qname()) type_name = tn;
            }
            attrs.push_back(attribute_use{attr_name, type_name, true,
                                          std::nullopt, std::nullopt});
          }
        } else if (p.holds<rng::group_pattern>()) {
          auto& g = p.get<rng::group_pattern>();
          if (g.left)
            translate_content_particles(*g.left, ns, particles, attrs);
          if (g.right)
            translate_content_particles(*g.right, ns, particles, attrs);
        } else if (p.holds<rng::interleave_pattern>()) {
          auto& il = p.get<rng::interleave_pattern>();
          if (il.left)
            translate_content_particles(*il.left, ns, particles, attrs);
          if (il.right)
            translate_content_particles(*il.right, ns, particles, attrs);
        } else if (p.holds<rng::one_or_more_pattern>()) {
          auto& om = p.get<rng::one_or_more_pattern>();
          if (om.content) {
            std::vector<particle> inner;
            std::vector<attribute_use> inner_attrs;
            translate_content_particles(*om.content, ns, inner, inner_attrs);
            for (auto& ip : inner) {
              ip.occurs = occurrence{1, unbounded};
              particles.push_back(std::move(ip));
            }
            for (auto& ia : inner_attrs)
              attrs.push_back(std::move(ia));
          }
        } else if (p.holds<rng::choice_pattern>()) {
          auto& ch = p.get<rng::choice_pattern>();
          if (ch.right && ch.right->holds<rng::empty_pattern>() && ch.left) {
            std::vector<particle> inner;
            std::vector<attribute_use> inner_attrs;
            translate_content_particles(*ch.left, ns, inner, inner_attrs);
            make_optional(inner, inner_attrs, particles, attrs);
          } else if (ch.left && ch.left->holds<rng::empty_pattern>() &&
                     ch.right) {
            std::vector<particle> inner;
            std::vector<attribute_use> inner_attrs;
            translate_content_particles(*ch.right, ns, inner, inner_attrs);
            make_optional(inner, inner_attrs, particles, attrs);
          } else {
            if (ch.left)
              translate_content_particles(*ch.left, ns, particles, attrs);
            if (ch.right)
              translate_content_particles(*ch.right, ns, particles, attrs);
          }
        } else if (p.holds<rng::ref_pattern>()) {
          auto& ref = p.get<rng::ref_pattern>();
          auto it = define_map.find(ref.name);
          if (it != define_map.end() && it->second->body)
            translate_content_particles(*it->second->body, ns, particles,
                                        attrs);
        }
        // text, empty, data, value: no particles in complex content
      }

      // Determine compositor kind from a pattern
      compositor_kind
      pattern_compositor(const rng::pattern& p) {
        if (p.holds<rng::interleave_pattern>())
          return compositor_kind::interleave;
        if (p.holds<rng::choice_pattern>()) {
          // Check if this is a true choice (not optional/zeroOrMore)
          auto& ch = p.get<rng::choice_pattern>();
          if ((ch.right && ch.right->holds<rng::empty_pattern>()) ||
              (ch.left && ch.left->holds<rng::empty_pattern>())) {
            return compositor_kind::sequence; // optional, treat as sequence
          }
          return compositor_kind::choice;
        }
        return compositor_kind::sequence;
      }

      // Translate an element body into a complex type
      void
      translate_element_body(const qname& elem_name, const rng::pattern& body,
                             const std::string& ns) {
        // Check if already translated
        std::string key =
            elem_name.namespace_uri() + "#" + elem_name.local_name();
        if (translated_types.count(key)) return;
        translated_types[key] = elem_name;

        std::vector<particle> particles;
        std::vector<attribute_use> attrs;
        translate_content_particles(body, ns, particles, attrs);

        compositor_kind compositor = pattern_compositor(body);

        if (particles.empty() && attrs.empty()) {
          // Empty content type
          content_type ct;
          result.add_complex_type(
              complex_type(elem_name, false, false, std::move(ct)));
          return;
        }

        if (particles.empty()) {
          // Attributes only → empty content with attributes
          content_type ct;
          result.add_complex_type(complex_type(
              elem_name, false, false, std::move(ct), std::move(attrs)));
          return;
        }

        // Build model group
        model_group mg(compositor, std::move(particles));
        complex_content cc(qname{}, derivation_method::restriction,
                           std::move(mg));
        content_type ct(content_kind::element_only, std::move(cc));
        result.add_complex_type(complex_type(elem_name, false, false,
                                             std::move(ct), std::move(attrs)));
      }

      // Translate a single define
      void
      translate_define(const rng::define& d, const std::string& ns) {
        if (!d.body) return;

        if (d.body->holds<rng::element_pattern>()) {
          auto& elem = d.body->get<rng::element_pattern>();
          if (elem.name.holds<rng::specific_name>()) {
            auto& sn = elem.name.get<rng::specific_name>();
            qname elem_name(sn.ns, sn.local_name);

            std::string key =
                elem_name.namespace_uri() + "#" + elem_name.local_name();
            if (translated_types.count(key)) return;

            // Determine element type
            qname type_name;
            if (elem.content) {
              type_name = content_type_name(*elem.content, ns);
              if (type_name == qname()) {
                type_name = elem_name;
                translate_element_body(elem_name, *elem.content, ns);
              }
            } else {
              type_name = qname(xs_ns, "string");
            }

            result.add_element(element_decl(elem_name, type_name));
            translated_types[key] = type_name;
          }
        }
      }

      void
      translate_grammar(const rng::grammar_pattern& g) {
        auto ns = infer_namespace(g);
        result.set_target_namespace(ns);

        build_define_map(g);

        // Translate all defines that are reachable from start
        for (const auto& d : g.defines) {
          translate_define(d, ns);
        }
      }
    };

  } // namespace

  schema_set
  rng_translate(const rng::pattern& simplified) {
    if (!simplified.holds<rng::grammar_pattern>()) {
      throw std::runtime_error(
          "rng_translate: expected a grammar pattern (run simplification "
          "first)");
    }

    translator tr;
    tr.translate_grammar(simplified.get<rng::grammar_pattern>());

    schema_set ss;
    ss.add(std::move(tr.result));
    ss.resolve();
    return ss;
  }

} // namespace xb
