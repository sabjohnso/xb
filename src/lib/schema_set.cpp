#include <xb/schema_set.hpp>

#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    // Built-in XSD type local names that don't need explicit definitions
    const std::set<std::string> builtin_types = {
        "anyType",
        "anySimpleType",
        "string",
        "normalizedString",
        "token",
        "boolean",
        "float",
        "double",
        "decimal",
        "integer",
        "nonPositiveInteger",
        "negativeInteger",
        "nonNegativeInteger",
        "positiveInteger",
        "long",
        "int",
        "short",
        "byte",
        "unsignedLong",
        "unsignedInt",
        "unsignedShort",
        "unsignedByte",
        "dateTime",
        "date",
        "time",
        "duration",
        "hexBinary",
        "base64Binary",
        "anyURI",
        "QName",
        "ID",
        "IDREF",
        "NMTOKEN",
        "language",
        "NOTATION",
        "ENTITY",
        "ENTITIES",
        "IDREFS",
        "NMTOKENS",
        "Name",
        "NCName",
        "gYear",
        "gYearMonth",
        "gMonth",
        "gMonthDay",
        "gDay",
    };

    bool
    is_builtin_type(const qname& name) {
      return name.namespace_uri() == xs_ns &&
             builtin_types.count(name.local_name()) > 0;
    }

    void
    check_type_ref(const qname& type_name,
                   const std::unordered_map<qname, bool>& known_types) {
      if (type_name.local_name().empty()) return;
      if (is_builtin_type(type_name)) return;
      if (known_types.count(type_name) > 0) return;
      throw std::runtime_error("schema_set: unresolved type reference '" +
                               type_name.namespace_uri() + ":" +
                               type_name.local_name() + "'");
    }

    void
    check_element_ref(const qname& ref,
                      const std::unordered_map<qname, bool>& known_elements) {
      if (ref.local_name().empty()) return;
      if (known_elements.count(ref) > 0) return;
      throw std::runtime_error("schema_set: unresolved element reference '" +
                               ref.namespace_uri() + ":" + ref.local_name() +
                               "'");
    }

    void
    check_group_ref(const qname& ref,
                    const std::unordered_map<qname, bool>& known_groups) {
      if (ref.local_name().empty()) return;
      if (known_groups.count(ref) > 0) return;
      throw std::runtime_error("schema_set: unresolved group reference '" +
                               ref.namespace_uri() + ":" + ref.local_name() +
                               "'");
    }

    void
    check_attr_group_ref(
        const qname& ref,
        const std::unordered_map<qname, bool>& known_attr_groups) {
      if (ref.local_name().empty()) return;
      if (known_attr_groups.count(ref) > 0) return;
      throw std::runtime_error(
          "schema_set: unresolved attribute group reference '" +
          ref.namespace_uri() + ":" + ref.local_name() + "'");
    }

    void
    validate_particles(const std::vector<particle>& particles,
                       const std::unordered_map<qname, bool>& known_types,
                       const std::unordered_map<qname, bool>& known_elements,
                       const std::unordered_map<qname, bool>& known_groups) {
      for (const auto& p : particles) {
        std::visit(
            [&](const auto& term) {
              using T = std::decay_t<decltype(term)>;
              if constexpr (std::is_same_v<T, element_decl>) {
                check_type_ref(term.type_name(), known_types);
              } else if constexpr (std::is_same_v<T, element_ref>) {
                check_element_ref(term.ref, known_elements);
              } else if constexpr (std::is_same_v<T, group_ref>) {
                check_group_ref(term.ref, known_groups);
              } else if constexpr (std::is_same_v<
                                       T, std::unique_ptr<model_group>>) {
                if (term) {
                  validate_particles(term->particles(), known_types,
                                     known_elements, known_groups);
                }
              }
              // wildcard — nothing to validate
            },
            p.term);
      }
    }

    template <typename T>
    void
    register_name(const qname& name, std::unordered_map<qname, bool>& map,
                  const char* kind) {
      if (map.count(name) > 0) {
        throw std::runtime_error(std::string("schema_set: duplicate ") + kind +
                                 " '" + name.namespace_uri() + ":" +
                                 name.local_name() + "'");
      }
      map[name] = true;
    }

  } // namespace

  void
  schema_set::add(schema s) {
    schemas_.push_back(std::move(s));
    resolved_ = false;
  }

  void
  schema_set::resolve() {
    // Phase 1: Build lookup tables, checking for duplicates
    std::unordered_map<qname, bool> known_types;
    std::unordered_map<qname, bool> known_elements;
    std::unordered_map<qname, bool> known_attributes;
    std::unordered_map<qname, bool> known_groups;
    std::unordered_map<qname, bool> known_attr_groups;

    for (const auto& s : schemas_) {
      for (const auto& st : s.simple_types()) {
        register_name<simple_type>(st.name(), known_types, "type");
      }
      for (const auto& ct : s.complex_types()) {
        register_name<complex_type>(ct.name(), known_types, "type");
      }
      for (const auto& e : s.elements()) {
        register_name<element_decl>(e.name(), known_elements, "element");
      }
      for (const auto& a : s.attributes()) {
        register_name<attribute_decl>(a.name(), known_attributes, "attribute");
      }
      for (const auto& g : s.model_group_defs()) {
        register_name<model_group_def>(g.name(), known_groups, "model group");
      }
      for (const auto& ag : s.attribute_group_defs()) {
        register_name<attribute_group_def>(ag.name(), known_attr_groups,
                                           "attribute group");
      }
    }

    // Phase 2: Validate all references
    for (const auto& s : schemas_) {
      // Check element type references
      for (const auto& e : s.elements()) {
        check_type_ref(e.type_name(), known_types);
      }

      // Check simple type base type references
      for (const auto& st : s.simple_types()) {
        if (!st.base_type_name().local_name().empty()) {
          check_type_ref(st.base_type_name(), known_types);
        }
        if (st.item_type_name().has_value()) {
          check_type_ref(st.item_type_name().value(), known_types);
        }
        for (const auto& mt : st.member_type_names()) {
          check_type_ref(mt, known_types);
        }
      }

      // Check complex type references
      for (const auto& ct : s.complex_types()) {
        // Check attribute type references
        for (const auto& au : ct.attributes()) {
          if (!au.type_name.local_name().empty()) {
            check_type_ref(au.type_name, known_types);
          }
        }

        // Check attribute group refs
        for (const auto& agr : ct.attribute_group_refs()) {
          check_attr_group_ref(agr.ref, known_attr_groups);
        }

        // Check content model references
        const auto& content = ct.content();
        if (std::holds_alternative<simple_content>(content.detail)) {
          const auto& sc = std::get<simple_content>(content.detail);
          check_type_ref(sc.base_type_name, known_types);
        } else if (std::holds_alternative<complex_content>(content.detail)) {
          const auto& cc = std::get<complex_content>(content.detail);
          if (!cc.base_type_name.local_name().empty()) {
            check_type_ref(cc.base_type_name, known_types);
          }
          if (cc.content_model.has_value()) {
            validate_particles(cc.content_model->particles(), known_types,
                               known_elements, known_groups);
          }
        }
      }

      // Check model group def particle references
      for (const auto& g : s.model_group_defs()) {
        validate_particles(g.group().particles(), known_types, known_elements,
                           known_groups);
      }

      // Check attribute group def references
      for (const auto& ag : s.attribute_group_defs()) {
        for (const auto& au : ag.attributes()) {
          if (!au.type_name.local_name().empty()) {
            check_type_ref(au.type_name, known_types);
          }
        }
        for (const auto& agr : ag.attribute_group_refs()) {
          check_attr_group_ref(agr.ref, known_attr_groups);
        }
      }
    }

    resolved_ = true;
  }

  const simple_type*
  schema_set::find_simple_type(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& st : s.simple_types()) {
        if (st.name() == name) return &st;
      }
    }
    return nullptr;
  }

  const complex_type*
  schema_set::find_complex_type(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& ct : s.complex_types()) {
        if (ct.name() == name) return &ct;
      }
    }
    return nullptr;
  }

  const element_decl*
  schema_set::find_element(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& e : s.elements()) {
        if (e.name() == name) return &e;
      }
    }
    return nullptr;
  }

  const attribute_decl*
  schema_set::find_attribute(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& a : s.attributes()) {
        if (a.name() == name) return &a;
      }
    }
    return nullptr;
  }

  const model_group_def*
  schema_set::find_model_group_def(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& g : s.model_group_defs()) {
        if (g.name() == name) return &g;
      }
    }
    return nullptr;
  }

  const attribute_group_def*
  schema_set::find_attribute_group_def(const qname& name) const {
    for (const auto& s : schemas_) {
      for (const auto& ag : s.attribute_group_defs()) {
        if (ag.name() == name) return &ag;
      }
    }
    return nullptr;
  }

} // namespace xb
