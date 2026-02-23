#include <xb/schema_parser.hpp>

#include <xb/open_content.hpp>
#include <xb/type_alternative.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    bool
    is_whitespace_only(std::string_view sv) {
      return !sv.empty() && std::all_of(sv.begin(), sv.end(), [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
      });
    }

    bool
    read_skip_ws(xml_reader& reader) {
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::characters &&
            is_whitespace_only(reader.text()))
          continue;
        return true;
      }
      return false;
    }

    bool
    is_xs(const qname& name, const std::string& local) {
      return name.namespace_uri() == xs_ns && name.local_name() == local;
    }

    std::optional<std::string>
    opt_attr(xml_reader& reader, const std::string& local) {
      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        if (reader.attribute_name(i).local_name() == local &&
            reader.attribute_name(i).namespace_uri().empty()) {
          return std::string(reader.attribute_value(i));
        }
      }
      return std::nullopt;
    }

    std::string
    req_attr(xml_reader& reader, const std::string& local) {
      auto val = opt_attr(reader, local);
      if (!val.has_value()) {
        throw std::runtime_error("schema_parser: missing required attribute '" +
                                 local + "' on <" + reader.name().local_name() +
                                 ">");
      }
      return val.value();
    }

    // Resolve a prefixed QName string (e.g. "xs:string") using the reader's
    // namespace prefix bindings.
    qname
    resolve_qname(xml_reader& reader, const std::string& prefixed_name) {
      auto colon = prefixed_name.find(':');
      if (colon == std::string::npos) {
        // No prefix — unqualified name
        return qname("", prefixed_name);
      }
      std::string prefix = prefixed_name.substr(0, colon);
      std::string local = prefixed_name.substr(colon + 1);
      auto uri = reader.namespace_uri_for_prefix(prefix);
      if (uri.empty()) {
        throw std::runtime_error("schema_parser: unknown namespace prefix '" +
                                 prefix + "'");
      }
      return qname(std::string(uri), local);
    }

    // Skip to the end of the current element
    void
    skip_element(xml_reader& reader) {
      std::size_t depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return;
        }
      }
    }

    // Parse occurrence attributes from a particle-bearing element
    occurrence
    parse_occurrence(xml_reader& reader) {
      occurrence o;
      auto min_str = opt_attr(reader, "minOccurs");
      if (min_str.has_value()) { o.min_occurs = std::stoull(min_str.value()); }
      auto max_str = opt_attr(reader, "maxOccurs");
      if (max_str.has_value()) {
        if (max_str.value() == "unbounded") {
          o.max_occurs = unbounded;
        } else {
          o.max_occurs = std::stoull(max_str.value());
        }
      }
      return o;
    }

    wildcard
    parse_wildcard_attrs(xml_reader& reader) {
      wildcard w;
      auto ns = opt_attr(reader, "namespace");
      if (ns.has_value()) {
        if (ns.value() == "##any") {
          w.ns_constraint = wildcard_ns_constraint::any;
        } else if (ns.value() == "##other") {
          w.ns_constraint = wildcard_ns_constraint::other;
        } else {
          w.ns_constraint = wildcard_ns_constraint::enumerated;
          // Space-separated list of URIs
          std::istringstream iss(ns.value());
          std::string uri;
          while (iss >> uri) {
            w.namespaces.push_back(uri);
          }
        }
      }
      auto pc = opt_attr(reader, "processContents");
      if (pc.has_value()) {
        if (pc.value() == "strict") {
          w.process = process_contents::strict;
        } else if (pc.value() == "lax") {
          w.process = process_contents::lax;
        } else if (pc.value() == "skip") {
          w.process = process_contents::skip;
        }
      }
      return w;
    }

    // Parse a facet element inside xs:restriction
    void
    parse_facet(xml_reader& reader, facet_set& facets) {
      const auto& local = reader.name().local_name();
      auto value = opt_attr(reader, "value");

      if (local == "enumeration") {
        facets.enumeration.push_back(value.value_or(""));
      } else if (local == "pattern") {
        facets.pattern = value;
      } else if (local == "minInclusive") {
        facets.min_inclusive = value;
      } else if (local == "maxInclusive") {
        facets.max_inclusive = value;
      } else if (local == "minExclusive") {
        facets.min_exclusive = value;
      } else if (local == "maxExclusive") {
        facets.max_exclusive = value;
      } else if (local == "length") {
        facets.length = std::stoull(value.value_or("0"));
      } else if (local == "minLength") {
        facets.min_length = std::stoull(value.value_or("0"));
      } else if (local == "maxLength") {
        facets.max_length = std::stoull(value.value_or("0"));
      } else if (local == "totalDigits") {
        facets.total_digits = std::stoull(value.value_or("0"));
      } else if (local == "fractionDigits") {
        facets.fraction_digits = std::stoull(value.value_or("0"));
      }
    }

    // Parse the children of an xs:restriction element for a simple type
    // Returns {base_type_name, facets}
    std::pair<qname, facet_set>
    parse_restriction(xml_reader& reader) {
      auto base_str = req_attr(reader, "base");
      qname base_type = resolve_qname(reader, base_str);
      facet_set facets;

      std::size_t depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          break;
        }
        if (reader.node_type() == xml_node_type::start_element &&
            reader.name().namespace_uri() == xs_ns) {
          parse_facet(reader, facets);
          skip_element(reader);
        }
      }

      return {base_type, facets};
    }

    // Parse xs:simpleType
    simple_type
    parse_simple_type(xml_reader& reader, const std::string& tns,
                      const std::string& name) {
      std::size_t depth = reader.depth();

      simple_type_variety variety = simple_type_variety::atomic;
      qname base_type;
      facet_set facets;
      std::optional<qname> item_type;
      std::vector<qname> member_types;

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          break;
        }
        if (reader.node_type() != xml_node_type::start_element) continue;
        if (reader.name().namespace_uri() != xs_ns) {
          skip_element(reader);
          continue;
        }

        const auto& local = reader.name().local_name();

        if (local == "restriction") {
          variety = simple_type_variety::atomic;
          auto [bt, fs] = parse_restriction(reader);
          base_type = std::move(bt);
          facets = std::move(fs);
        } else if (local == "list") {
          variety = simple_type_variety::list;
          auto item_str = opt_attr(reader, "itemType");
          if (item_str.has_value()) {
            item_type = resolve_qname(reader, item_str.value());
          }
          skip_element(reader);
        } else if (local == "union") {
          variety = simple_type_variety::union_type;
          auto members_str = opt_attr(reader, "memberTypes");
          if (members_str.has_value()) {
            std::istringstream iss(members_str.value());
            std::string token;
            while (iss >> token) {
              member_types.push_back(resolve_qname(reader, token));
            }
          }
          skip_element(reader);
        } else {
          skip_element(reader);
        }
      }

      return simple_type(qname(tns, name), variety, std::move(base_type),
                         std::move(facets), std::move(item_type),
                         std::move(member_types));
    }

    // Parse xs:alternative children of an element declaration.
    // The reader must be positioned on the xs:element start tag.
    // Reads children and returns any xs:alternative elements found.
    std::vector<type_alternative>
    parse_alternatives(xml_reader& reader, std::size_t elem_depth) {
      std::vector<type_alternative> alts;
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == elem_depth) {
          break;
        }
        if (reader.node_type() != xml_node_type::start_element) continue;
        if (reader.name().namespace_uri() != xs_ns) {
          skip_element(reader);
          continue;
        }
        if (reader.name().local_name() == "alternative") {
          auto test = opt_attr(reader, "test");
          auto type_str = opt_attr(reader, "type");
          qname type_name;
          if (type_str.has_value()) {
            type_name = resolve_qname(reader, type_str.value());
          }
          skip_element(reader);
          alts.push_back({test, type_name});
        } else {
          skip_element(reader);
        }
      }
      return alts;
    }

    // Parse a particle's term (element, element ref, group ref, nested
    // compositor, or wildcard) from inside a compositor
    particle
    parse_particle(xml_reader& reader, const std::string& tns,
                   std::vector<simple_type>& anon_simple_types,
                   std::vector<complex_type>& anon_complex_types);

    // Parse a model group (sequence, choice, all) and its particle children
    model_group
    parse_compositor(xml_reader& reader, compositor_kind kind,
                     const std::string& tns,
                     std::vector<simple_type>& anon_simple_types,
                     std::vector<complex_type>& anon_complex_types) {
      model_group mg(kind);
      std::size_t depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          break;
        }
        if (reader.node_type() != xml_node_type::start_element) continue;
        if (reader.name().namespace_uri() != xs_ns) {
          skip_element(reader);
          continue;
        }

        mg.add_particle(
            parse_particle(reader, tns, anon_simple_types, anon_complex_types));
      }

      return mg;
    }

    // Forward declare for mutual recursion with parse_complex_type_body
    void
    parse_complex_type_body(xml_reader& reader, const std::string& tns,
                            content_type& content,
                            std::vector<attribute_use>& attributes,
                            std::vector<attribute_group_ref>& attr_group_refs,
                            std::optional<wildcard>& attr_wildcard,
                            bool& is_mixed,
                            std::vector<simple_type>& anon_simple_types,
                            std::vector<complex_type>& anon_complex_types,
                            std::optional<open_content>& oc);

    particle
    parse_particle(xml_reader& reader, const std::string& tns,
                   std::vector<simple_type>& anon_simple_types,
                   std::vector<complex_type>& anon_complex_types) {
      occurrence occurs = parse_occurrence(reader);
      const auto& local = reader.name().local_name();

      if (local == "element") {
        auto ref_str = opt_attr(reader, "ref");
        if (ref_str.has_value()) {
          auto ref_qname = resolve_qname(reader, ref_str.value());
          skip_element(reader);
          return particle(element_ref{ref_qname}, occurs);
        }

        // Inline element declaration in compositor
        auto elem_name = req_attr(reader, "name");
        auto type_str = opt_attr(reader, "type");
        qname type_name;

        std::vector<type_alternative> alts;

        if (type_str.has_value()) {
          type_name = resolve_qname(reader, type_str.value());
          alts = parse_alternatives(reader, reader.depth());
        } else {
          // May have anonymous type child
          std::string synth_name = elem_name + "_type";
          type_name = qname(tns, synth_name);
          std::size_t elem_depth = reader.depth();
          bool found_anon = false;

          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == elem_depth) {
              break;
            }
            if (reader.node_type() != xml_node_type::start_element) continue;
            if (reader.name().namespace_uri() != xs_ns) {
              skip_element(reader);
              continue;
            }

            if (reader.name().local_name() == "simpleType") {
              auto st = parse_simple_type(reader, tns, synth_name);
              anon_simple_types.push_back(std::move(st));
              found_anon = true;
            } else if (reader.name().local_name() == "complexType") {
              // Parse inline complex type
              bool mixed = opt_attr(reader, "mixed").value_or("") == "true";
              content_type ct_content;
              std::vector<attribute_use> ct_attrs;
              std::vector<attribute_group_ref> ct_agrefs;
              std::optional<wildcard> ct_awild;
              std::optional<open_content> ct_oc;

              parse_complex_type_body(
                  reader, tns, ct_content, ct_attrs, ct_agrefs, ct_awild, mixed,
                  anon_simple_types, anon_complex_types, ct_oc);

              anon_complex_types.push_back(complex_type(
                  qname(tns, synth_name), false, mixed, std::move(ct_content),
                  std::move(ct_attrs), std::move(ct_agrefs),
                  std::move(ct_awild), std::move(ct_oc)));
              found_anon = true;
            } else if (reader.name().local_name() == "alternative") {
              auto test = opt_attr(reader, "test");
              auto alt_type_str = opt_attr(reader, "type");
              qname alt_type_name;
              if (alt_type_str.has_value()) {
                alt_type_name = resolve_qname(reader, alt_type_str.value());
              }
              skip_element(reader);
              alts.push_back({test, alt_type_name});
            } else {
              skip_element(reader);
            }
          }

          if (!found_anon) {
            // No type attribute and no anonymous type — use xs:anyType
            type_name = qname(xs_ns, "anyType");
          }
        }

        auto nillable = opt_attr(reader, "nillable").value_or("") == "true";
        auto abstract = opt_attr(reader, "abstract").value_or("") == "true";

        element_decl ed(qname(tns, elem_name), type_name, nillable, abstract,
                        std::nullopt, std::nullopt, std::nullopt,
                        std::move(alts));
        return particle(std::move(ed), occurs);
      }

      if (local == "group") {
        auto ref_str = req_attr(reader, "ref");
        auto ref_qname = resolve_qname(reader, ref_str);
        skip_element(reader);
        return particle(group_ref{ref_qname}, occurs);
      }

      if (local == "any") {
        auto w = parse_wildcard_attrs(reader);
        skip_element(reader);
        return particle(std::move(w), occurs);
      }

      if (local == "sequence" || local == "choice" || local == "all") {
        compositor_kind ck = compositor_kind::sequence;
        if (local == "choice") ck = compositor_kind::choice;
        if (local == "all") ck = compositor_kind::all;

        auto mg = parse_compositor(reader, ck, tns, anon_simple_types,
                                   anon_complex_types);
        return particle(std::make_unique<model_group>(std::move(mg)), occurs);
      }

      // Unknown element in compositor — skip
      skip_element(reader);
      return particle(element_ref{qname()}, occurs);
    }

    // Parse the body (children) of an xs:complexType
    void
    parse_complex_type_body(xml_reader& reader, const std::string& tns,
                            content_type& content,
                            std::vector<attribute_use>& attributes,
                            std::vector<attribute_group_ref>& attr_group_refs,
                            std::optional<wildcard>& attr_wildcard,
                            bool& is_mixed,
                            std::vector<simple_type>& anon_simple_types,
                            std::vector<complex_type>& anon_complex_types,
                            std::optional<open_content>& oc) {
      std::size_t depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          break;
        }
        if (reader.node_type() != xml_node_type::start_element) continue;
        if (reader.name().namespace_uri() != xs_ns) {
          skip_element(reader);
          continue;
        }

        const auto& local = reader.name().local_name();

        if (local == "sequence" || local == "choice" || local == "all") {
          compositor_kind ck = compositor_kind::sequence;
          if (local == "choice") ck = compositor_kind::choice;
          if (local == "all") ck = compositor_kind::all;

          auto mg = parse_compositor(reader, ck, tns, anon_simple_types,
                                     anon_complex_types);
          content_kind ckind =
              is_mixed ? content_kind::mixed : content_kind::element_only;
          content = content_type(
              ckind, complex_content(qname(), derivation_method::restriction,
                                     std::move(mg)));
        } else if (local == "simpleContent") {
          std::size_t sc_depth = reader.depth();
          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == sc_depth) {
              break;
            }
            if (reader.node_type() != xml_node_type::start_element) continue;
            if (reader.name().namespace_uri() != xs_ns) {
              skip_element(reader);
              continue;
            }

            derivation_method dm = derivation_method::restriction;
            if (reader.name().local_name() == "extension") {
              dm = derivation_method::extension;
            }
            auto base_str = req_attr(reader, "base");
            auto base_name = resolve_qname(reader, base_str);
            facet_set facets;

            // Parse children for attributes and facets
            std::size_t der_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == der_depth) {
                break;
              }
              if (reader.node_type() != xml_node_type::start_element) continue;
              if (reader.name().namespace_uri() != xs_ns) {
                skip_element(reader);
                continue;
              }
              if (reader.name().local_name() == "attribute") {
                auto aname = req_attr(reader, "name");
                auto atype_str = opt_attr(reader, "type");
                qname atype;
                if (atype_str.has_value()) {
                  atype = resolve_qname(reader, atype_str.value());
                }
                auto use = opt_attr(reader, "use");
                bool req = use.has_value() && use.value() == "required";
                auto def = opt_attr(reader, "default");
                auto fix = opt_attr(reader, "fixed");

                attributes.push_back(
                    attribute_use{qname("", aname), atype, req, def, fix});
                skip_element(reader);
              } else if (reader.name().local_name() == "attributeGroup") {
                auto ref_str = req_attr(reader, "ref");
                attr_group_refs.push_back(
                    attribute_group_ref{resolve_qname(reader, ref_str)});
                skip_element(reader);
              } else {
                // Facets inside simpleContent/restriction
                parse_facet(reader, facets);
                skip_element(reader);
              }
            }

            simple_content sc;
            sc.base_type_name = base_name;
            sc.derivation = dm;
            sc.facets = std::move(facets);
            content = content_type(content_kind::simple, std::move(sc));
          }
        } else if (local == "complexContent") {
          auto cc_mixed = opt_attr(reader, "mixed");
          if (cc_mixed.has_value() && cc_mixed.value() == "true") {
            is_mixed = true;
          }

          std::size_t cc_depth = reader.depth();
          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == cc_depth) {
              break;
            }
            if (reader.node_type() != xml_node_type::start_element) continue;
            if (reader.name().namespace_uri() != xs_ns) {
              skip_element(reader);
              continue;
            }

            derivation_method dm = derivation_method::restriction;
            if (reader.name().local_name() == "extension") {
              dm = derivation_method::extension;
            }
            auto base_str = req_attr(reader, "base");
            auto base_name = resolve_qname(reader, base_str);

            // Parse the derivation children
            std::optional<model_group> mg;
            std::size_t der_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == der_depth) {
                break;
              }
              if (reader.node_type() != xml_node_type::start_element) continue;
              if (reader.name().namespace_uri() != xs_ns) {
                skip_element(reader);
                continue;
              }
              const auto& dl = reader.name().local_name();
              if (dl == "sequence" || dl == "choice" || dl == "all") {
                compositor_kind ck = compositor_kind::sequence;
                if (dl == "choice") ck = compositor_kind::choice;
                if (dl == "all") ck = compositor_kind::all;
                mg = parse_compositor(reader, ck, tns, anon_simple_types,
                                      anon_complex_types);
              } else if (dl == "attribute") {
                auto aname = req_attr(reader, "name");
                auto atype_str = opt_attr(reader, "type");
                qname atype;
                if (atype_str.has_value()) {
                  atype = resolve_qname(reader, atype_str.value());
                }
                auto use = opt_attr(reader, "use");
                bool req = use.has_value() && use.value() == "required";
                auto def = opt_attr(reader, "default");
                auto fix = opt_attr(reader, "fixed");
                attributes.push_back(
                    attribute_use{qname("", aname), atype, req, def, fix});
                skip_element(reader);
              } else if (dl == "attributeGroup") {
                auto ref_str = req_attr(reader, "ref");
                attr_group_refs.push_back(
                    attribute_group_ref{resolve_qname(reader, ref_str)});
                skip_element(reader);
              } else if (dl == "anyAttribute") {
                attr_wildcard = parse_wildcard_attrs(reader);
                skip_element(reader);
              } else {
                skip_element(reader);
              }
            }

            content_kind ckind =
                is_mixed ? content_kind::mixed : content_kind::element_only;
            content = content_type(
                ckind, complex_content(base_name, dm, std::move(mg)));
          }
        } else if (local == "attribute") {
          auto aname = req_attr(reader, "name");
          auto atype_str = opt_attr(reader, "type");
          qname atype;
          if (atype_str.has_value()) {
            atype = resolve_qname(reader, atype_str.value());
          }
          auto use = opt_attr(reader, "use");
          bool req = use.has_value() && use.value() == "required";
          auto def = opt_attr(reader, "default");
          auto fix = opt_attr(reader, "fixed");
          attributes.push_back(
              attribute_use{qname("", aname), atype, req, def, fix});
          skip_element(reader);
        } else if (local == "attributeGroup") {
          auto ref_str = req_attr(reader, "ref");
          attr_group_refs.push_back(
              attribute_group_ref{resolve_qname(reader, ref_str)});
          skip_element(reader);
        } else if (local == "anyAttribute") {
          attr_wildcard = parse_wildcard_attrs(reader);
          skip_element(reader);
        } else if (local == "openContent") {
          auto mode_str = opt_attr(reader, "mode").value_or("interleave");
          open_content_mode mode = open_content_mode::interleave;
          if (mode_str == "suffix")
            mode = open_content_mode::suffix;
          else if (mode_str == "none")
            mode = open_content_mode::none;

          wildcard w;
          std::size_t oc_depth = reader.depth();
          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == oc_depth)
              break;
            if (reader.node_type() != xml_node_type::start_element) continue;
            if (reader.name().namespace_uri() == xs_ns &&
                reader.name().local_name() == "any") {
              w = parse_wildcard_attrs(reader);
              skip_element(reader);
            } else {
              skip_element(reader);
            }
          }
          oc = open_content{mode, w};
        } else {
          skip_element(reader);
        }
      }
    }

  } // namespace

  schema
  schema_parser::parse(xml_reader& reader) {
    // Advance to the root xs:schema element
    if (!read_skip_ws(reader) ||
        reader.node_type() != xml_node_type::start_element ||
        !is_xs(reader.name(), "schema")) {
      throw std::runtime_error(
          "schema_parser: expected <xs:schema> root element");
    }

    schema result;

    // Read targetNamespace
    auto tns = opt_attr(reader, "targetNamespace");
    if (tns.has_value()) { result.set_target_namespace(tns.value()); }
    std::string target_ns = result.target_namespace();

    // Build namespace context from the schema element
    // Vectors for collecting anonymous types
    std::vector<simple_type> anon_simple_types;
    std::vector<complex_type> anon_complex_types;

    std::size_t schema_depth = reader.depth();

    while (read_skip_ws(reader)) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == schema_depth) {
        break;
      }
      if (reader.node_type() != xml_node_type::start_element) continue;
      if (reader.name().namespace_uri() != xs_ns) {
        skip_element(reader);
        continue;
      }

      const auto& local = reader.name().local_name();

      if (local == "element") {
        auto name = req_attr(reader, "name");
        auto type_str = opt_attr(reader, "type");
        qname type_name;
        bool has_anon_type = false;

        // Check for anonymous type children before consuming the element
        auto nillable = opt_attr(reader, "nillable").value_or("") == "true";
        auto abstract = opt_attr(reader, "abstract").value_or("") == "true";
        auto default_val = opt_attr(reader, "default");
        auto fixed_val = opt_attr(reader, "fixed");
        auto subst_str = opt_attr(reader, "substitutionGroup");
        std::optional<qname> subst_group;
        if (subst_str.has_value()) {
          subst_group = resolve_qname(reader, subst_str.value());
        }

        std::vector<type_alternative> alts;

        if (type_str.has_value()) {
          type_name = resolve_qname(reader, type_str.value());
          alts = parse_alternatives(reader, reader.depth());
        } else {
          // Look for anonymous type child
          std::string synth_name = name + "_type";
          type_name = qname(target_ns, synth_name);

          std::size_t elem_depth = reader.depth();
          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == elem_depth) {
              break;
            }
            if (reader.node_type() != xml_node_type::start_element) continue;
            if (reader.name().namespace_uri() != xs_ns) {
              skip_element(reader);
              continue;
            }

            if (reader.name().local_name() == "simpleType") {
              auto st = parse_simple_type(reader, target_ns, synth_name);
              anon_simple_types.push_back(std::move(st));
              has_anon_type = true;
            } else if (reader.name().local_name() == "complexType") {
              bool mixed = opt_attr(reader, "mixed").value_or("") == "true";
              content_type ct_content;
              std::vector<attribute_use> ct_attrs;
              std::vector<attribute_group_ref> ct_agrefs;
              std::optional<wildcard> ct_awild;
              std::optional<open_content> ct_oc;

              parse_complex_type_body(
                  reader, target_ns, ct_content, ct_attrs, ct_agrefs, ct_awild,
                  mixed, anon_simple_types, anon_complex_types, ct_oc);

              anon_complex_types.push_back(complex_type(
                  qname(target_ns, synth_name), false, mixed,
                  std::move(ct_content), std::move(ct_attrs),
                  std::move(ct_agrefs), std::move(ct_awild), std::move(ct_oc)));
              has_anon_type = true;
            } else if (reader.name().local_name() == "alternative") {
              auto test = opt_attr(reader, "test");
              auto alt_type_str = opt_attr(reader, "type");
              qname alt_type_name;
              if (alt_type_str.has_value()) {
                alt_type_name = resolve_qname(reader, alt_type_str.value());
              }
              skip_element(reader);
              alts.push_back({test, alt_type_name});
            } else {
              skip_element(reader);
            }
          }

          if (!has_anon_type) { type_name = qname(xs_ns, "anyType"); }
        }

        result.add_element(
            element_decl(qname(target_ns, name), type_name, nillable, abstract,
                         default_val, fixed_val, subst_group, std::move(alts)));
      } else if (local == "attribute") {
        auto name = req_attr(reader, "name");
        auto type_str = opt_attr(reader, "type");
        qname type_name;
        if (type_str.has_value()) {
          type_name = resolve_qname(reader, type_str.value());
        }
        auto default_val = opt_attr(reader, "default");
        auto fixed_val = opt_attr(reader, "fixed");

        result.add_attribute(
            attribute_decl(qname("", name), type_name, default_val, fixed_val));
        skip_element(reader);
      } else if (local == "simpleType") {
        auto name = req_attr(reader, "name");
        auto st = parse_simple_type(reader, target_ns, name);
        result.add_simple_type(std::move(st));
      } else if (local == "complexType") {
        auto name = req_attr(reader, "name");
        bool abstract = opt_attr(reader, "abstract").value_or("") == "true";
        bool mixed = opt_attr(reader, "mixed").value_or("") == "true";

        content_type ct_content;
        std::vector<attribute_use> ct_attrs;
        std::vector<attribute_group_ref> ct_agrefs;
        std::optional<wildcard> ct_awild;
        std::optional<open_content> ct_oc;

        parse_complex_type_body(reader, target_ns, ct_content, ct_attrs,
                                ct_agrefs, ct_awild, mixed, anon_simple_types,
                                anon_complex_types, ct_oc);

        result.add_complex_type(complex_type(
            qname(target_ns, name), abstract, mixed, std::move(ct_content),
            std::move(ct_attrs), std::move(ct_agrefs), std::move(ct_awild),
            std::move(ct_oc)));
      } else if (local == "group") {
        auto name = req_attr(reader, "name");
        // Parse the compositor child
        std::size_t grp_depth = reader.depth();
        std::optional<model_group> mg;

        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == grp_depth) {
            break;
          }
          if (reader.node_type() != xml_node_type::start_element) continue;
          if (reader.name().namespace_uri() != xs_ns) {
            skip_element(reader);
            continue;
          }
          const auto& gl = reader.name().local_name();
          if (gl == "sequence" || gl == "choice" || gl == "all") {
            compositor_kind ck = compositor_kind::sequence;
            if (gl == "choice") ck = compositor_kind::choice;
            if (gl == "all") ck = compositor_kind::all;
            mg = parse_compositor(reader, ck, target_ns, anon_simple_types,
                                  anon_complex_types);
          } else {
            skip_element(reader);
          }
        }

        if (mg.has_value()) {
          result.add_model_group_def(
              model_group_def(qname(target_ns, name), std::move(mg.value())));
        }
      } else if (local == "attributeGroup") {
        auto name = req_attr(reader, "name");
        std::vector<attribute_use> attrs;
        std::vector<attribute_group_ref> agrefs;
        std::optional<wildcard> awild;

        std::size_t ag_depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == ag_depth) {
            break;
          }
          if (reader.node_type() != xml_node_type::start_element) continue;
          if (reader.name().namespace_uri() != xs_ns) {
            skip_element(reader);
            continue;
          }
          if (reader.name().local_name() == "attribute") {
            auto aname = req_attr(reader, "name");
            auto atype_str = opt_attr(reader, "type");
            qname atype;
            if (atype_str.has_value()) {
              atype = resolve_qname(reader, atype_str.value());
            }
            auto use = opt_attr(reader, "use");
            bool req = use.has_value() && use.value() == "required";
            auto def = opt_attr(reader, "default");
            auto fix = opt_attr(reader, "fixed");
            attrs.push_back(
                attribute_use{qname("", aname), atype, req, def, fix});
            skip_element(reader);
          } else if (reader.name().local_name() == "attributeGroup") {
            auto ref_str = req_attr(reader, "ref");
            agrefs.push_back(
                attribute_group_ref{resolve_qname(reader, ref_str)});
            skip_element(reader);
          } else if (reader.name().local_name() == "anyAttribute") {
            awild = parse_wildcard_attrs(reader);
            skip_element(reader);
          } else {
            skip_element(reader);
          }
        }

        result.add_attribute_group_def(
            attribute_group_def(qname(target_ns, name), std::move(attrs),
                                std::move(agrefs), std::move(awild)));
      } else if (local == "import") {
        auto ns = opt_attr(reader, "namespace").value_or("");
        auto loc = opt_attr(reader, "schemaLocation").value_or("");
        result.add_import(schema_import{ns, loc});
        skip_element(reader);
      } else if (local == "include") {
        auto loc = opt_attr(reader, "schemaLocation").value_or("");
        result.add_include(schema_include{loc});
        skip_element(reader);
      } else if (local == "defaultOpenContent") {
        auto mode_str = opt_attr(reader, "mode").value_or("interleave");
        open_content_mode mode = open_content_mode::interleave;
        if (mode_str == "suffix") mode = open_content_mode::suffix;

        auto ate = opt_attr(reader, "appliesToEmpty").value_or("false");
        bool applies_to_empty = (ate == "true" || ate == "1");

        wildcard w;
        std::size_t doc_depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == doc_depth)
            break;
          if (reader.node_type() != xml_node_type::start_element) continue;
          if (reader.name().namespace_uri() == xs_ns &&
              reader.name().local_name() == "any") {
            w = parse_wildcard_attrs(reader);
            skip_element(reader);
          } else {
            skip_element(reader);
          }
        }
        result.set_default_open_content(open_content{mode, w},
                                        applies_to_empty);
      } else {
        // Skip annotation, notation, redefine, etc.
        skip_element(reader);
      }
    }

    // Add anonymous types to the schema
    for (auto& st : anon_simple_types) {
      result.add_simple_type(std::move(st));
    }
    for (auto& ct : anon_complex_types) {
      result.add_complex_type(std::move(ct));
    }

    return result;
  }

} // namespace xb
