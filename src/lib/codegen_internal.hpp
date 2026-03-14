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
    const std::set<std::string>* all_namespaces = nullptr;
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

    bool
    is_wrapped() const {
      return options.encapsulation == encapsulation_mode::wrapped;
    }

    // Build a field access expression: "value.field" for raw structs,
    // "value.field()" for wrapped classes.
    std::string
    field_access(const std::string& var, const std::string& field) const {
      if (is_wrapped()) return var + "." + field + "()";
      return var + "." + field;
    }

    // Generate a for-loop header for iterating over a sequence field.
    // Raw mode:    "for (const auto& VAR : value.field) {\n"
    // Wrapped:     "for (auto VAR_it = value.field_begin(); ...
    //               VAR_it != value.field_end(); ++VAR_it) {\n"
    //              "  const auto& VAR = *VAR_it;\n"
    std::string
    sequence_for_begin(const std::string& var, const std::string& field,
                       const std::string& item_var) const {
      if (is_wrapped()) {
        std::string it = item_var + "_it";
        return "for (auto " + it + " = " + var + "." + field + "_begin(); " +
               it + " != " + var + "." + field + "_end(); ++" + it + ") {\n" +
               "    const auto& " + item_var + " = *" + it + ";\n";
      }
      return "for (const auto& " + item_var + " : " + var + "." + field +
             ") {\n";
    }

    // Generate field_size expression.
    // Raw mode:    "value.field.size()"
    // Wrapped:     "value.field_size()"
    std::string
    field_size(const std::string& var, const std::string& field) const {
      if (is_wrapped()) return var + "." + field + "_size()";
      return var + "." + field + ".size()";
    }

    std::string
    qualify_fn(const std::string& prefix, const qname& qn) const {
      std::string fn = prefix + type_name(qn.local_name());
      return qualify_call(fn, qn);
    }

    // Qualify an arbitrary function name with the namespace from a qname.
    // Unlike qualify_fn, this does not embed the type name into the function
    // name — use it for functions like to_string and format whose names are
    // independent of the type.
    std::string
    qualify_call(const std::string& fn_name, const qname& qn) const {
      if (!qn.namespace_uri().empty() && qn.namespace_uri() != current_ns) {
        std::string ns = resolve_namespace(qn.namespace_uri());
        if (!ns.empty()) return ns + "::" + fn_name;
      }
      return fn_name;
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
        std::string ns = resolve_namespace(qn.namespace_uri());
        if (!ns.empty()) return ns + "::" + name;
      }

      return name;
    }

  private:
    std::string
    resolve_namespace(const std::string& ns_uri) const {
      if (options.ns_style == namespace_style::short_name && all_namespaces)
        return default_namespace_for(ns_uri, *all_namespaces, options);
      return cpp_namespace_for(ns_uri, options);
    }
  };

} // namespace xb
