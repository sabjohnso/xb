#include <xb/dtd_translator.hpp>

#include <unordered_map>
#include <unordered_set>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    struct translator {
      schema result;
      const dtd::document& doc;
      std::unordered_set<std::string> translated_types;

      // Merged attributes: element_name -> vector of attribute_def
      std::unordered_map<std::string, std::vector<const dtd::attribute_def*>>
          attr_map;

      // Element index: name -> element_decl
      std::unordered_map<std::string, const dtd::element_decl*> elem_map;

      explicit translator(const dtd::document& d) : doc(d) {
        // Build attribute map (multiple ATTLISTs for same element are merged)
        for (const auto& al : doc.attlists) {
          for (const auto& ad : al.attributes) {
            attr_map[al.element_name].push_back(&ad);
          }
        }
        // Build element index
        for (const auto& ed : doc.elements) {
          elem_map[ed.name] = &ed;
        }
      }

      // Map DTD attribute type to XSD type qname
      qname
      attr_type_qname(const dtd::attribute_def& ad) {
        switch (ad.type) {
          case dtd::attribute_type::cdata:
            return qname(xs_ns, "string");
          case dtd::attribute_type::id:
            return qname(xs_ns, "ID");
          case dtd::attribute_type::idref:
            return qname(xs_ns, "IDREF");
          case dtd::attribute_type::idrefs:
            return qname(xs_ns, "IDREFS");
          case dtd::attribute_type::nmtoken:
            return qname(xs_ns, "NMTOKEN");
          case dtd::attribute_type::nmtokens:
            return qname(xs_ns, "NMTOKENS");
          case dtd::attribute_type::entity:
            return qname(xs_ns, "ENTITY");
          case dtd::attribute_type::entities:
            return qname(xs_ns, "ENTITIES");
          case dtd::attribute_type::notation:
            return qname(xs_ns, "NOTATION");
          case dtd::attribute_type::enumeration: {
            // Create a simple type with enumeration facets
            std::string type_name = ad.name + "Type";
            qname tn("", type_name);
            facet_set facets;
            facets.enumeration = ad.enum_values;
            result.add_simple_type(simple_type(tn, simple_type_variety::atomic,
                                               qname(xs_ns, "string"),
                                               std::move(facets)));
            return tn;
          }
        }
        return qname(xs_ns, "string");
      }

      // Build attribute_use list for an element
      std::vector<attribute_use>
      build_attributes(const std::string& element_name) {
        std::vector<attribute_use> attrs;
        auto it = attr_map.find(element_name);
        if (it == attr_map.end()) return attrs;

        for (const auto* ad : it->second) {
          attribute_use au;
          au.name = qname("", ad->name);
          au.type_name = attr_type_qname(*ad);
          au.required = (ad->default_kind == dtd::default_kind::required);

          if (ad->default_kind == dtd::default_kind::fixed) {
            au.fixed_value = ad->default_value;
          } else if (ad->default_kind == dtd::default_kind::value) {
            au.default_value = ad->default_value;
          }

          attrs.push_back(std::move(au));
        }
        return attrs;
      }

      // Map quantifier to occurrence
      occurrence
      quantifier_occurrence(dtd::quantifier q) {
        switch (q) {
          case dtd::quantifier::one:
            return {1, 1};
          case dtd::quantifier::optional:
            return {0, 1};
          case dtd::quantifier::zero_or_more:
            return {0, unbounded};
          case dtd::quantifier::one_or_more:
            return {1, unbounded};
        }
        return {1, 1};
      }

      // Resolve the type for a child element by name
      qname
      resolve_child_type(const std::string& name) {
        auto it = elem_map.find(name);
        if (it != elem_map.end()) {
          // Ensure the referenced element is translated
          translate_element(*it->second);
          // Pure text elements map to xs:string directly
          if (it->second->content.kind == dtd::content_kind::mixed &&
              it->second->content.mixed_names.empty() &&
              attr_map.find(name) == attr_map.end()) {
            return qname(xs_ns, "string");
          }
          return qname("", name + "Type");
        }
        // Unknown child element — default to xs:string
        return qname(xs_ns, "string");
      }

      // Translate a content particle into schema particles
      void
      translate_particle(const dtd::content_particle& cp,
                         std::vector<particle>& particles) {
        if (cp.kind == dtd::particle_kind::name) {
          qname elem_name("", cp.name);
          qname type_name = resolve_child_type(cp.name);
          particles.emplace_back(element_decl(elem_name, type_name),
                                 quantifier_occurrence(cp.quantifier));
        } else if (cp.kind == dtd::particle_kind::sequence ||
                   cp.kind == dtd::particle_kind::choice) {
          compositor_kind comp = (cp.kind == dtd::particle_kind::sequence)
                                     ? compositor_kind::sequence
                                     : compositor_kind::choice;

          std::vector<particle> inner;
          for (const auto& child : cp.children) {
            translate_particle(child, inner);
          }

          auto mg = std::make_unique<model_group>(comp, std::move(inner));
          particles.emplace_back(std::move(mg),
                                 quantifier_occurrence(cp.quantifier));
        }
      }

      void
      translate_element(const dtd::element_decl& ed) {
        std::string type_name_str = ed.name + "Type";
        qname elem_name("", ed.name);
        qname type_name("", type_name_str);

        if (translated_types.count(ed.name)) return;
        translated_types.insert(ed.name);

        auto attrs = build_attributes(ed.name);

        switch (ed.content.kind) {
          case dtd::content_kind::empty: {
            content_type ct;
            result.add_complex_type(complex_type(
                type_name, false, false, std::move(ct), std::move(attrs)));
            result.add_element(element_decl(elem_name, type_name));
            break;
          }

          case dtd::content_kind::any: {
            // ANY = complex type with empty content (best approximation)
            content_type ct;
            result.add_complex_type(complex_type(
                type_name, false, false, std::move(ct), std::move(attrs)));
            result.add_element(element_decl(elem_name, type_name));
            break;
          }

          case dtd::content_kind::mixed: {
            if (ed.content.mixed_names.empty() && attrs.empty()) {
              // Pure text: (#PCDATA) -> use xs:string directly
              result.add_element(
                  element_decl(elem_name, qname(xs_ns, "string")));
            } else if (ed.content.mixed_names.empty()) {
              // Text with attributes
              content_type ct;
              result.add_complex_type(complex_type(
                  type_name, false, true, std::move(ct), std::move(attrs)));
              result.add_element(element_decl(elem_name, type_name));
            } else {
              // Mixed content with child elements
              std::vector<particle> particles;
              for (const auto& name : ed.content.mixed_names) {
                qname child_name("", name);
                qname child_type = resolve_child_type(name);
                particles.emplace_back(element_decl(child_name, child_type),
                                       occurrence{0, unbounded});
              }
              model_group mg(compositor_kind::choice, std::move(particles));
              complex_content cc(qname{}, derivation_method::restriction,
                                 std::move(mg));
              content_type ct(content_kind::mixed, std::move(cc));
              result.add_complex_type(complex_type(
                  type_name, false, true, std::move(ct), std::move(attrs)));
              result.add_element(element_decl(elem_name, type_name));
            }
            break;
          }

          case dtd::content_kind::children: {
            if (!ed.content.particle.has_value()) {
              content_type ct;
              result.add_complex_type(complex_type(
                  type_name, false, false, std::move(ct), std::move(attrs)));
              result.add_element(element_decl(elem_name, type_name));
              break;
            }

            auto& root_cp = *ed.content.particle;
            compositor_kind comp = (root_cp.kind == dtd::particle_kind::choice)
                                       ? compositor_kind::choice
                                       : compositor_kind::sequence;

            std::vector<particle> particles;
            for (const auto& child : root_cp.children) {
              translate_particle(child, particles);
            }

            model_group mg(comp, std::move(particles));
            complex_content cc(qname{}, derivation_method::restriction,
                               std::move(mg));
            content_type ct(content_kind::element_only, std::move(cc));
            result.add_complex_type(complex_type(
                type_name, false, false, std::move(ct), std::move(attrs)));
            result.add_element(element_decl(elem_name, type_name));
            break;
          }
        }
      }

      void
      translate() {
        // Target namespace is always empty for DTDs
        result.set_target_namespace("");

        for (const auto& ed : doc.elements) {
          translate_element(ed);
        }
      }
    };

  } // namespace

  schema_set
  dtd_translate(const dtd::document& doc) {
    translator tr(doc);
    tr.translate();

    schema_set ss;
    ss.add(std::move(tr.result));
    ss.resolve();
    return ss;
  }

} // namespace xb
