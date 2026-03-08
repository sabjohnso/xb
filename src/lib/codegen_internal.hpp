#pragma once

#include <xb/naming.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <map>
#include <set>
#include <string>

namespace xb {

  struct type_resolver {
    const schema_set& schemas;
    const type_map& types;
    const codegen_options& options;
    const std::string& current_ns;
    std::set<std::string>& referenced_namespaces;
    const std::map<std::string, std::string>* union_variant_map = nullptr;
    const std::set<qname>* cycle_types = nullptr;
    const std::set<std::string>* schema_type_names = nullptr;

    bool
    is_cycle_type(const qname& type_name) const {
      return cycle_types && cycle_types->count(type_name) > 0;
    }

    std::string
    type_name(const std::string& xml_local_name) const {
      return apply_naming(xml_local_name, naming_category::type_,
                          options.naming);
    }

    std::string
    field_name(const std::string& xml_local_name,
               const std::string& enclosing_type = {}) const {
      auto name =
          apply_naming(xml_local_name, naming_category::field, options.naming);
      if (schema_type_names && name != enclosing_type &&
          schema_type_names->count(name))
        name += '_';
      return name;
    }

    std::string
    enum_value_name(const std::string& xml_value) const {
      return apply_naming(xml_value, naming_category::enum_value,
                          options.naming);
    }

    std::string
    qualify_fn(const std::string& prefix, const qname& qn) const {
      std::string fn = prefix + to_cpp_identifier(qn.local_name());
      if (!qn.namespace_uri().empty() && qn.namespace_uri() != current_ns) {
        std::string ns = cpp_namespace_for(qn.namespace_uri(), options);
        if (!ns.empty()) return ns + "::" + fn;
      }
      return fn;
    }

    std::string
    resolve(const qname& type_name) const {
      if (type_name.namespace_uri().empty() && type_name.local_name().empty())
        return "void";

      if (type_name.namespace_uri() == "http://www.w3.org/2001/XMLSchema") {
        if (auto* mapping = types.find(type_name.local_name()))
          return mapping->cpp_type;
      }

      if (auto* st = schemas.find_simple_type(type_name)) {
        if (!type_name.namespace_uri().empty() &&
            type_name.namespace_uri() != current_ns)
          return qualify(type_name);

        if (!st->facets().enumeration.empty()) return qualify(type_name);

        if (st->variety() == simple_type_variety::list &&
            st->item_type_name().has_value())
          return "std::vector<" + resolve(st->item_type_name().value()) + ">";

        if (st->variety() == simple_type_variety::union_type) {
          std::string result = "std::variant<";
          bool first = true;
          for (const auto& member : st->member_type_names()) {
            if (!first) result += ", ";
            result += resolve(member);
            first = false;
          }
          return result + ">";
        }

        return resolve(st->base_type_name());
      }

      if (schemas.find_complex_type(type_name)) return qualify(type_name);

      if (auto* mapping = types.find(type_name.local_name()))
        return mapping->cpp_type;

      return this->type_name(type_name.local_name());
    }

    std::string
    qualify(const qname& qn) const {
      std::string name = type_name(qn.local_name());

      if (!qn.namespace_uri().empty() && qn.namespace_uri() != current_ns) {
        referenced_namespaces.insert(qn.namespace_uri());
        std::string ns = cpp_namespace_for(qn.namespace_uri(), options);
        if (!ns.empty()) return ns + "::" + name;
      }

      return name;
    }
  };

} // namespace xb
