#include <xb/codegen.hpp>
#include <xb/naming.hpp>

#include <set>

namespace xb {

  codegen::codegen(const schema_set& schemas, const type_map& types,
                   codegen_options options)
      : schemas_(schemas), types_(types), options_(std::move(options)) {}

  namespace {

    struct type_resolver {
      const schema_set& schemas;
      const type_map& types;
      const codegen_options& options;
      const std::string& current_ns;
      std::set<std::string>& referenced_namespaces;

      std::string
      resolve(const qname& type_name) const {
        if (type_name.namespace_uri().empty() && type_name.local_name().empty())
          return "void";

        // Check type_map for XSD built-in type
        if (type_name.namespace_uri() == "http://www.w3.org/2001/XMLSchema") {
          if (auto* mapping = types.find(type_name.local_name()))
            return mapping->cpp_type;
        }

        // Check if it's a simple type defined in the schema set
        if (auto* st = schemas.find_simple_type(type_name)) {
          // Cross-namespace reference: use qualified name
          if (!type_name.namespace_uri().empty() &&
              type_name.namespace_uri() != current_ns)
            return qualify(type_name);

          // Simple type with enumeration -> the enum name
          if (!st->facets().enumeration.empty()) return qualify(type_name);

          // List type -> vector
          if (st->variety() == simple_type_variety::list &&
              st->item_type_name().has_value())
            return "std::vector<" + resolve(st->item_type_name().value()) + ">";

          // Union type -> variant
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

          // Atomic restriction without enum -> alias to base
          return resolve(st->base_type_name());
        }

        // Check if it's a complex type
        if (schemas.find_complex_type(type_name)) return qualify(type_name);

        // Fallback: try type_map with just local name
        if (auto* mapping = types.find(type_name.local_name()))
          return mapping->cpp_type;

        return to_cpp_identifier(type_name.local_name());
      }

      std::string
      qualify(const qname& type_name) const {
        std::string name = to_cpp_identifier(type_name.local_name());

        if (!type_name.namespace_uri().empty() &&
            type_name.namespace_uri() != current_ns) {
          referenced_namespaces.insert(type_name.namespace_uri());
          std::string ns =
              cpp_namespace_for(type_name.namespace_uri(), options);
          if (!ns.empty()) return ns + "::" + name;
        }

        return name;
      }
    };

    // Find substitution group members for an abstract element
    std::vector<const element_decl*>
    find_substitution_members(const schema_set& schemas,
                              const qname& head_name) {
      std::vector<const element_decl*> members;
      for (const auto& s : schemas.schemas()) {
        for (const auto& e : s.elements()) {
          if (e.substitution_group().has_value() &&
              e.substitution_group().value() == head_name && !e.abstract())
            members.push_back(&e);
        }
      }
      return members;
    }

    std::string
    field_type_for_element(const element_decl& elem, const occurrence& occurs,
                           const type_resolver& resolver,
                           const qname& containing_type_name) {
      std::string base_type = resolver.resolve(elem.type_name());

      // Check for recursive self-reference
      bool is_recursive = (elem.type_name() == containing_type_name);

      // Nillable -> optional
      if (elem.nillable() && !is_recursive)
        base_type = "std::optional<" + base_type + ">";

      // Apply cardinality
      if (occurs.is_unbounded() || occurs.max_occurs > 1)
        return "std::vector<" + base_type + ">";

      // Self-reference with optional cardinality -> unique_ptr
      if (is_recursive && occurs.min_occurs == 0)
        return "std::unique_ptr<" + base_type + ">";

      if (occurs.min_occurs == 0) return "std::optional<" + base_type + ">";

      return base_type;
    }

    std::string
    default_value_for_element(const element_decl& elem) {
      if (elem.default_value().has_value()) return elem.default_value().value();
      if (elem.fixed_value().has_value()) return elem.fixed_value().value();
      return "";
    }

    void
    translate_particles(const std::vector<particle>& particles,
                        compositor_kind compositor,
                        std::vector<cpp_field>& fields,
                        const type_resolver& resolver,
                        const qname& containing_type_name);

    void
    translate_particle_term(const particle& p, std::vector<cpp_field>& fields,
                            const type_resolver& resolver,
                            const qname& containing_type_name) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              fields.push_back({field_type_for_element(term, p.occurs, resolver,
                                                       containing_type_name),
                                to_cpp_identifier(term.name().local_name()),
                                default_value_for_element(term)});
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto* elem = resolver.schemas.find_element(term.ref);
              if (!elem) return;

              // Check for abstract element -> substitution group
              if (elem->abstract()) {
                auto members =
                    find_substitution_members(resolver.schemas, term.ref);
                if (!members.empty()) {
                  std::string variant = "std::variant<";
                  bool first = true;
                  for (const auto* m : members) {
                    if (!first) variant += ", ";
                    variant += resolver.resolve(m->type_name());
                    first = false;
                  }
                  variant += ">";

                  std::string type = variant;
                  if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                    type = "std::vector<" + type + ">";
                  else if (p.occurs.min_occurs == 0)
                    type = "std::optional<" + type + ">";

                  fields.push_back(
                      {type, to_cpp_identifier(elem->name().local_name()), ""});
                  return;
                }
              }

              fields.push_back(
                  {field_type_for_element(*elem, p.occurs, resolver,
                                          containing_type_name),
                   to_cpp_identifier(elem->name().local_name()),
                   default_value_for_element(*elem)});
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def) {
                translate_particles(group_def->group().particles(),
                                    group_def->group().compositor(), fields,
                                    resolver, containing_type_name);
              }
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) {
                translate_particles(term->particles(), term->compositor(),
                                    fields, resolver, containing_type_name);
              }
            } else if constexpr (std::is_same_v<T, wildcard>) {
              fields.push_back({"std::vector<xb::any_element>", "any", ""});
            }
          },
          p.term);
    }

    void
    translate_particles(const std::vector<particle>& particles,
                        compositor_kind compositor,
                        std::vector<cpp_field>& fields,
                        const type_resolver& resolver,
                        const qname& containing_type_name) {
      if (compositor == compositor_kind::choice) {
        std::string variant_type = "std::variant<";
        bool first = true;

        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, element_decl>) {
                  if (!first) variant_type += ", ";
                  variant_type += resolver.resolve(term.type_name());
                  first = false;
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  if (!first) variant_type += ", ";
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (elem) variant_type += resolver.resolve(elem->type_name());
                  first = false;
                }
              },
              p.term);
        }

        variant_type += ">";
        fields.push_back({variant_type, "choice", ""});
        return;
      }

      for (const auto& p : particles)
        translate_particle_term(p, fields, resolver, containing_type_name);
    }

    std::string
    default_value_for_attr(const attribute_use& attr,
                           const type_resolver& resolver) {
      if (attr.fixed_value.has_value()) {
        std::string cpp_type = resolver.resolve(attr.type_name);
        // String types need quoting
        if (cpp_type == "std::string")
          return "\"" + attr.fixed_value.value() + "\"";
        return attr.fixed_value.value();
      }
      if (attr.default_value.has_value()) return attr.default_value.value();
      return "";
    }

    void
    translate_attributes(const std::vector<attribute_use>& attrs,
                         std::vector<cpp_field>& fields,
                         const type_resolver& resolver) {
      for (const auto& attr : attrs) {
        std::string base_type = resolver.resolve(attr.type_name);
        std::string name = to_cpp_identifier(attr.name.local_name());
        std::string default_val = default_value_for_attr(attr, resolver);

        if (attr.required)
          fields.push_back({base_type, name, default_val});
        else
          fields.push_back(
              {"std::optional<" + base_type + ">", name, default_val});
      }
    }

    void
    translate_attribute_group_refs(const std::vector<attribute_group_ref>& refs,
                                   std::vector<cpp_field>& fields,
                                   const type_resolver& resolver) {
      for (const auto& ref : refs) {
        auto* group_def = resolver.schemas.find_attribute_group_def(ref.ref);
        if (group_def) {
          translate_attributes(group_def->attributes(), fields, resolver);
          translate_attribute_group_refs(group_def->attribute_group_refs(),
                                         fields, resolver);
        }
      }
    }

    // Collect fields from a base complex type (for extension flattening)
    void
    collect_base_fields(const schema_set& schemas, const qname& base_name,
                        std::vector<cpp_field>& fields,
                        const type_resolver& resolver,
                        const qname& containing_type_name) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;

      // Recurse to collect grandparent fields first
      if (base_ct->content().kind == content_kind::element_only ||
          base_ct->content().kind == content_kind::mixed) {
        if (auto* cc =
                std::get_if<complex_content>(&base_ct->content().detail)) {
          if (!cc->base_type_name.namespace_uri().empty() ||
              !cc->base_type_name.local_name().empty()) {
            if (cc->derivation == derivation_method::extension)
              collect_base_fields(schemas, cc->base_type_name, fields, resolver,
                                  containing_type_name);
          }

          if (cc->content_model.has_value()) {
            translate_particles(cc->content_model->particles(),
                                cc->content_model->compositor(), fields,
                                resolver, containing_type_name);
          }
        }
      }

      // Collect base type attributes
      translate_attributes(base_ct->attributes(), fields, resolver);
      translate_attribute_group_refs(base_ct->attribute_group_refs(), fields,
                                     resolver);
    }

    cpp_decl
    translate_complex_type(const complex_type& ct,
                           const type_resolver& resolver) {
      cpp_struct s;
      s.name = to_cpp_identifier(ct.name().local_name());
      s.generate_equality = true;

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          std::string value_type = resolver.resolve(sc->base_type_name);
          s.fields.push_back({value_type, "value", ""});
        }

        translate_attributes(ct.attributes(), s.fields, resolver);
        translate_attribute_group_refs(ct.attribute_group_refs(), s.fields,
                                       resolver);
        if (ct.attribute_wildcard().has_value())
          s.fields.push_back(
              {"std::vector<xb::any_attribute>", "any_attribute", ""});
        return s;
      }

      // Handle mixed content
      if (ct.mixed() && (ct.content().kind == content_kind::mixed ||
                         ct.content().kind == content_kind::element_only)) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          if (cc->content_model.has_value()) {
            // Collect child element types for the variant
            std::string variant = "std::vector<std::variant<std::string";
            for (const auto& p : cc->content_model->particles()) {
              std::visit(
                  [&](const auto& term) {
                    using T = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<T, element_decl>) {
                      variant += ", " + resolver.resolve(term.type_name());
                    }
                  },
                  p.term);
            }
            variant += ">>";
            s.fields.push_back({variant, "content", ""});
          }
        }

        translate_attributes(ct.attributes(), s.fields, resolver);
        translate_attribute_group_refs(ct.attribute_group_refs(), s.fields,
                                       resolver);
        if (ct.attribute_wildcard().has_value())
          s.fields.push_back(
              {"std::vector<xb::any_attribute>", "any_attribute", ""});
        return s;
      }

      // Handle element_only content
      if (ct.content().kind == content_kind::element_only) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          // Extension: flatten base fields first
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            collect_base_fields(resolver.schemas, cc->base_type_name, s.fields,
                                resolver, ct.name());
          }

          if (cc->content_model.has_value()) {
            translate_particles(cc->content_model->particles(),
                                cc->content_model->compositor(), s.fields,
                                resolver, ct.name());
          }
        }
      }

      // Translate attributes
      translate_attributes(ct.attributes(), s.fields, resolver);
      translate_attribute_group_refs(ct.attribute_group_refs(), s.fields,
                                     resolver);

      if (ct.attribute_wildcard().has_value())
        s.fields.push_back(
            {"std::vector<xb::any_attribute>", "any_attribute", ""});

      return s;
    }

    cpp_decl
    translate_simple_type(const simple_type& st,
                          const type_resolver& resolver) {
      if (!st.facets().enumeration.empty()) {
        cpp_enum e;
        e.name = to_cpp_identifier(st.name().local_name());
        for (const auto& val : st.facets().enumeration)
          e.values.push_back({to_cpp_identifier(val), val});
        return e;
      }

      if (st.variety() == simple_type_variety::list) {
        std::string item_type = "void";
        if (st.item_type_name().has_value())
          item_type = resolver.resolve(st.item_type_name().value());
        return cpp_type_alias{to_cpp_identifier(st.name().local_name()),
                              "std::vector<" + item_type + ">"};
      }

      if (st.variety() == simple_type_variety::union_type) {
        std::string variant = "std::variant<";
        bool first = true;
        for (const auto& member : st.member_type_names()) {
          if (!first) variant += ", ";
          variant += resolver.resolve(member);
          first = false;
        }
        variant += ">";
        return cpp_type_alias{to_cpp_identifier(st.name().local_name()),
                              variant};
      }

      std::string base = resolver.resolve(st.base_type_name());
      return cpp_type_alias{to_cpp_identifier(st.name().local_name()), base};
    }

    std::vector<cpp_include>
    compute_includes(const std::set<std::string>& referenced_namespaces,
                     const std::vector<schema>& schemas,
                     const std::vector<cpp_decl>& declarations) {
      std::set<std::string> includes;

      auto check_type = [&](const std::string& type_expr) {
        if (type_expr.find("std::string") != std::string::npos)
          includes.insert("<string>");
        if (type_expr.find("std::optional") != std::string::npos)
          includes.insert("<optional>");
        if (type_expr.find("std::vector") != std::string::npos)
          includes.insert("<vector>");
        if (type_expr.find("std::variant") != std::string::npos)
          includes.insert("<variant>");
        if (type_expr.find("std::unique_ptr") != std::string::npos)
          includes.insert("<memory>");
        if (type_expr.find("int8_t") != std::string::npos ||
            type_expr.find("int16_t") != std::string::npos ||
            type_expr.find("int32_t") != std::string::npos ||
            type_expr.find("int64_t") != std::string::npos ||
            type_expr.find("uint8_t") != std::string::npos ||
            type_expr.find("uint16_t") != std::string::npos ||
            type_expr.find("uint32_t") != std::string::npos ||
            type_expr.find("uint64_t") != std::string::npos)
          includes.insert("<cstdint>");
        if (type_expr.find("xb::any_element") != std::string::npos)
          includes.insert("\"xb/any_element.hpp\"");
        if (type_expr.find("xb::any_attribute") != std::string::npos)
          includes.insert("\"xb/any_attribute.hpp\"");
        if (type_expr.find("xb::decimal") != std::string::npos)
          includes.insert("\"xb/decimal.hpp\"");
        if (type_expr.find("xb::integer") != std::string::npos)
          includes.insert("\"xb/integer.hpp\"");
        if (type_expr.find("xb::qname") != std::string::npos)
          includes.insert("\"xb/qname.hpp\"");
        if (type_expr.find("xb::date_time") != std::string::npos)
          includes.insert("\"xb/date_time.hpp\"");
        else if (type_expr.find("xb::date") != std::string::npos)
          includes.insert("\"xb/date.hpp\"");
        if (type_expr.find("xb::time") != std::string::npos &&
            type_expr.find("xb::date_time") == std::string::npos)
          includes.insert("\"xb/time.hpp\"");
        if (type_expr.find("xb::duration") != std::string::npos)
          includes.insert("\"xb/duration.hpp\"");
        if (type_expr.find("std::byte") != std::string::npos)
          includes.insert("<cstddef>");
      };

      for (const auto& decl : declarations) {
        std::visit(
            [&](const auto& d) {
              using T = std::decay_t<decltype(d)>;
              if constexpr (std::is_same_v<T, cpp_struct>) {
                for (const auto& f : d.fields)
                  check_type(f.type);
              } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
                check_type(d.target);
              }
            },
            decl);
      }

      for (const auto& decl : declarations) {
        if (std::holds_alternative<cpp_enum>(decl)) {
          includes.insert("<stdexcept>");
          includes.insert("<string>");
          includes.insert("<string_view>");
          break;
        }
      }

      for (const auto& decl : declarations) {
        if (std::holds_alternative<cpp_function>(decl)) {
          includes.insert("\"xb/xml_value.hpp\"");
          includes.insert("\"xb/xml_io.hpp\"");
          includes.insert("\"xb/xml_reader.hpp\"");
          includes.insert("\"xb/xml_writer.hpp\"");
          break;
        }
      }

      for (const auto& ref_ns : referenced_namespaces) {
        for (const auto& s : schemas) {
          if (s.target_namespace() == ref_ns) {
            std::string filename =
                to_snake_case(ref_ns.substr(ref_ns.rfind('/') + 1)) + ".hpp";
            includes.insert("\"" + filename + "\"");
          }
        }
      }

      std::vector<cpp_include> result;
      result.reserve(includes.size());
      for (auto& inc : includes)
        result.push_back({inc});
      return result;
    }

    std::string
    filename_for_namespace(const std::string& target_ns) {
      if (target_ns.empty()) return "generated.hpp";

      auto last_sep = target_ns.rfind('/');
      std::string segment;
      if (last_sep != std::string::npos)
        segment = target_ns.substr(last_sep + 1);
      else
        segment = target_ns;

      return to_snake_case(segment) + ".hpp";
    }

    // Get the name of a declaration (for dependency resolution)
    std::string
    decl_name(const cpp_decl& decl) {
      return std::visit(
          [](const auto& d) -> std::string {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_struct>)
              return d.name;
            else if constexpr (std::is_same_v<T, cpp_enum>)
              return d.name;
            else if constexpr (std::is_same_v<T, cpp_type_alias>)
              return d.name;
            else if constexpr (std::is_same_v<T, cpp_forward_decl>)
              return d.name;
            else if constexpr (std::is_same_v<T, cpp_function>)
              return d.name;
            else
              return "";
          },
          decl);
    }

    // Collect type names referenced by a declaration
    std::set<std::string>
    decl_dependencies(const cpp_decl& decl) {
      std::set<std::string> deps;

      auto extract_type_refs = [&](const std::string& type_expr) {
        // Extract identifiers that could be type names
        // Look for bare identifiers (not in std:: or xb:: namespaces)
        std::string token;
        for (std::size_t i = 0; i < type_expr.size(); ++i) {
          char c = type_expr[i];
          if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            token += c;
          } else {
            if (!token.empty() && token != "std" && token != "xb" &&
                token != "const" && token != "bool" && token != "int" &&
                token != "float" && token != "double" && token != "void" &&
                token != "char")
              deps.insert(token);
            token.clear();
          }
        }
        if (!token.empty() && token != "std" && token != "xb" &&
            token != "const" && token != "bool" && token != "int" &&
            token != "float" && token != "double" && token != "void" &&
            token != "char")
          deps.insert(token);
      };

      std::visit(
          [&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_struct>) {
              for (const auto& f : d.fields)
                extract_type_refs(f.type);
            } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
              extract_type_refs(d.target);
            }
          },
          decl);

      return deps;
    }

    // Topological sort of declarations based on type dependencies
    std::vector<cpp_decl>
    order_declarations(std::vector<cpp_decl> decls) {
      if (decls.size() <= 1) return decls;

      // Build name -> index map
      std::unordered_map<std::string, std::size_t> name_to_idx;
      for (std::size_t i = 0; i < decls.size(); ++i)
        name_to_idx[decl_name(decls[i])] = i;

      // Build adjacency list (dependency edges)
      std::vector<std::set<std::size_t>> deps(decls.size());
      for (std::size_t i = 0; i < decls.size(); ++i) {
        for (const auto& dep_name : decl_dependencies(decls[i])) {
          auto it = name_to_idx.find(dep_name);
          if (it != name_to_idx.end() && it->second != i)
            deps[i].insert(it->second);
        }
      }

      // Kahn's algorithm for topological sort
      std::vector<std::size_t> in_degree(decls.size(), 0);
      for (std::size_t i = 0; i < decls.size(); ++i)
        for (auto dep : deps[i])
          (void)dep; // deps[i] are the nodes i depends on

      // Recompute: for each node, count how many others depend on it
      std::vector<std::vector<std::size_t>> reverse_deps(decls.size());
      for (std::size_t i = 0; i < decls.size(); ++i) {
        for (auto dep : deps[i])
          reverse_deps[dep].push_back(i);
        in_degree[i] = deps[i].size();
      }

      std::vector<std::size_t> order;
      std::vector<std::size_t> queue;
      for (std::size_t i = 0; i < decls.size(); ++i) {
        if (in_degree[i] == 0) queue.push_back(i);
      }

      while (!queue.empty()) {
        auto idx = queue.front();
        queue.erase(queue.begin());
        order.push_back(idx);

        for (auto dependent : reverse_deps[idx]) {
          if (--in_degree[dependent] == 0) queue.push_back(dependent);
        }
      }

      // If cycle detected (not all nodes visited), append remaining
      if (order.size() < decls.size()) {
        std::set<std::size_t> visited(order.begin(), order.end());
        for (std::size_t i = 0; i < decls.size(); ++i) {
          if (visited.find(i) == visited.end()) order.push_back(i);
        }
      }

      std::vector<cpp_decl> result;
      result.reserve(decls.size());
      for (auto idx : order)
        result.push_back(std::move(decls[idx]));
      return result;
    }

    // ===== Serialization code generation =====

    // Determine if a type name resolves to an enum
    bool
    is_enum_type(const schema_set& schemas, const qname& type_name) {
      auto* st = schemas.find_simple_type(type_name);
      return st && !st->facets().enumeration.empty();
    }

    // Determine if a type name resolves to a complex type
    bool
    is_complex_type(const schema_set& schemas, const qname& type_name) {
      return schemas.find_complex_type(type_name) != nullptr;
    }

    // Generate the format expression for a value, considering type
    std::string
    format_expr(const std::string& value_expr, const qname& type_name,
                const schema_set& schemas, const type_resolver& /*resolver*/) {
      if (is_enum_type(schemas, type_name))
        return "to_string(" + value_expr + ")";
      return "xb::format(" + value_expr + ")";
    }

    // Generate write code for a single element particle
    struct write_element_info {
      std::string field_name;
      qname element_name;
      qname type_name;
      occurrence occurs;
      bool nillable;
      bool is_recursive;
    };

    void
    emit_write_element(std::string& body, const write_element_info& info,
                       const schema_set& schemas,
                       const type_resolver& /*resolver*/) {
      std::string qn = "xb::qname{\"" + info.element_name.namespace_uri() +
                       "\", \"" + info.element_name.local_name() + "\"}";
      std::string field = "value." + info.field_name;

      bool is_complex = is_complex_type(schemas, info.type_name);

      if (info.is_recursive && info.occurs.min_occurs == 0 &&
          !info.occurs.is_unbounded() && info.occurs.max_occurs <= 1) {
        // unique_ptr field
        std::string write_fn =
            "write_" + to_cpp_identifier(info.type_name.local_name());
        body += "  if (" + field + ") {\n";
        body += "    writer.start_element(" + qn + ");\n";
        body += "    " + write_fn + "(*" + field + ", writer);\n";
        body += "    writer.end_element();\n";
        body += "  }\n";
        return;
      }

      if (info.occurs.is_unbounded() || info.occurs.max_occurs > 1) {
        // vector field
        body += "  for (const auto& item : " + field + ") {\n";
        if (is_complex) {
          std::string write_fn =
              "write_" + to_cpp_identifier(info.type_name.local_name());
          body += "    writer.start_element(" + qn + ");\n";
          body += "    " + write_fn + "(item, writer);\n";
          body += "    writer.end_element();\n";
        } else {
          body += "    xb::write_simple(writer, " + qn + ", item);\n";
        }
        body += "  }\n";
        return;
      }

      if (info.occurs.min_occurs == 0) {
        // optional field
        body += "  if (" + field + ") {\n";
        if (is_complex) {
          std::string write_fn =
              "write_" + to_cpp_identifier(info.type_name.local_name());
          body += "    writer.start_element(" + qn + ");\n";
          body += "    " + write_fn + "(*" + field + ", writer);\n";
          body += "    writer.end_element();\n";
        } else {
          body += "    xb::write_simple(writer, " + qn + ", *" + field + ");\n";
        }
        body += "  }\n";
        return;
      }

      // Required field
      if (is_complex) {
        std::string write_fn =
            "write_" + to_cpp_identifier(info.type_name.local_name());
        body += "  writer.start_element(" + qn + ");\n";
        body += "  " + write_fn + "(" + field + ", writer);\n";
        body += "  writer.end_element();\n";
      } else {
        body += "  xb::write_simple(writer, " + qn + ", " + field + ");\n";
      }
    }

    // Forward declare
    void
    emit_write_particles(std::string& body,
                         const std::vector<particle>& particles,
                         compositor_kind compositor,
                         const type_resolver& resolver,
                         const qname& containing_type_name);

    void
    emit_write_particle_term(std::string& body, const particle& p,
                             const type_resolver& resolver,
                             const qname& containing_type_name) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              bool is_recursive = (term.type_name() == containing_type_name);
              emit_write_element(body,
                                 {to_cpp_identifier(term.name().local_name()),
                                  term.name(), term.type_name(), p.occurs,
                                  term.nillable(), is_recursive},
                                 resolver.schemas, resolver);
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto* elem = resolver.schemas.find_element(term.ref);
              if (!elem) return;
              emit_write_element(body,
                                 {to_cpp_identifier(elem->name().local_name()),
                                  elem->name(), elem->type_name(), p.occurs,
                                  elem->nillable(), false},
                                 resolver.schemas, resolver);
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def)
                emit_write_particles(body, group_def->group().particles(),
                                     group_def->group().compositor(), resolver,
                                     containing_type_name);
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term)
                emit_write_particles(body, term->particles(),
                                     term->compositor(), resolver,
                                     containing_type_name);
            } else if constexpr (std::is_same_v<T, wildcard>) {
              body += "  for (const auto& e : value.any) {\n";
              body += "    e.write(writer);\n";
              body += "  }\n";
            }
          },
          p.term);
    }

    void
    emit_write_particles(std::string& body,
                         const std::vector<particle>& particles,
                         compositor_kind compositor,
                         const type_resolver& resolver,
                         const qname& containing_type_name) {
      if (compositor == compositor_kind::choice) {
        // std::visit dispatch
        body += "  std::visit([&](const auto& v) {\n";
        body += "    using T = std::decay_t<decltype(v)>;\n";

        bool first = true;
        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using TermT = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<TermT, element_decl>) {
                  std::string cpp_type = resolver.resolve(term.type_name());
                  std::string qn = "xb::qname{\"" +
                                   term.name().namespace_uri() + "\", \"" +
                                   term.name().local_name() + "\"}";
                  std::string kw = first ? "if" : "else if";
                  body += "    " + kw + " constexpr (std::is_same_v<T, " +
                          cpp_type + ">) {\n";
                  if (is_complex_type(resolver.schemas, term.type_name())) {
                    std::string write_fn =
                        "write_" +
                        to_cpp_identifier(term.type_name().local_name());
                    body += "      writer.start_element(" + qn + ");\n";
                    body += "      " + write_fn + "(v, writer);\n";
                    body += "      writer.end_element();\n";
                  } else {
                    body += "      xb::write_simple(writer, " + qn + ", v);\n";
                  }
                  body += "    }\n";
                  first = false;
                } else if constexpr (std::is_same_v<TermT, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (!elem) return;
                  std::string cpp_type = resolver.resolve(elem->type_name());
                  std::string qn = "xb::qname{\"" +
                                   elem->name().namespace_uri() + "\", \"" +
                                   elem->name().local_name() + "\"}";
                  std::string kw = first ? "if" : "else if";
                  body += "    " + kw + " constexpr (std::is_same_v<T, " +
                          cpp_type + ">) {\n";
                  body += "      xb::write_simple(writer, " + qn + ", v);\n";
                  body += "    }\n";
                  first = false;
                }
              },
              p.term);
        }

        body += "  }, value.choice);\n";
        return;
      }

      // Sequence or all: write each particle in order
      for (const auto& p : particles)
        emit_write_particle_term(body, p, resolver, containing_type_name);
    }

    void
    emit_write_attributes(std::string& body,
                          const std::vector<attribute_use>& attrs,
                          const schema_set& schemas,
                          const type_resolver& resolver) {
      for (const auto& attr : attrs) {
        std::string name = to_cpp_identifier(attr.name.local_name());
        std::string qn = "xb::qname{\"" + attr.name.namespace_uri() + "\", \"" +
                         attr.name.local_name() + "\"}";
        std::string fmt_expr =
            format_expr("value." + name, attr.type_name, schemas, resolver);

        if (attr.required) {
          body += "  writer.attribute(" + qn + ", " + fmt_expr + ");\n";
        } else {
          body += "  if (value." + name + ") {\n";
          std::string opt_fmt =
              format_expr("*value." + name, attr.type_name, schemas, resolver);
          body += "    writer.attribute(" + qn + ", " + opt_fmt + ");\n";
          body += "  }\n";
        }
      }
    }

    void
    emit_write_attribute_group_refs(
        std::string& body, const std::vector<attribute_group_ref>& refs,
        const schema_set& schemas, const type_resolver& resolver) {
      for (const auto& ref : refs) {
        auto* group_def = schemas.find_attribute_group_def(ref.ref);
        if (group_def) {
          emit_write_attributes(body, group_def->attributes(), schemas,
                                resolver);
          emit_write_attribute_group_refs(
              body, group_def->attribute_group_refs(), schemas, resolver);
        }
      }
    }

    void
    emit_write_base_fields(std::string& body, const schema_set& schemas,
                           const qname& base_name,
                           const type_resolver& resolver,
                           const qname& containing_type_name) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;

      if (base_ct->content().kind == content_kind::element_only ||
          base_ct->content().kind == content_kind::mixed) {
        if (auto* cc =
                std::get_if<complex_content>(&base_ct->content().detail)) {
          if (!cc->base_type_name.namespace_uri().empty() ||
              !cc->base_type_name.local_name().empty()) {
            if (cc->derivation == derivation_method::extension)
              emit_write_base_fields(body, schemas, cc->base_type_name,
                                     resolver, containing_type_name);
          }
          if (cc->content_model.has_value())
            emit_write_particles(body, cc->content_model->particles(),
                                 cc->content_model->compositor(), resolver,
                                 containing_type_name);
        }
      }

      emit_write_attributes(body, base_ct->attributes(), schemas, resolver);
      emit_write_attribute_group_refs(body, base_ct->attribute_group_refs(),
                                      schemas, resolver);
    }

    cpp_function
    generate_write_function(const complex_type& ct,
                            const type_resolver& resolver) {
      cpp_function fn;
      std::string struct_name = to_cpp_identifier(ct.name().local_name());
      fn.return_type = "void";
      fn.name = "write_" + struct_name;
      fn.parameters =
          "const " + struct_name + "& value, xb::xml_writer& writer";

      std::string body;

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        emit_write_attributes(body, ct.attributes(), resolver.schemas,
                              resolver);
        emit_write_attribute_group_refs(body, ct.attribute_group_refs(),
                                        resolver.schemas, resolver);

        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          std::string fmt = format_expr("value.value", sc->base_type_name,
                                        resolver.schemas, resolver);
          body += "  writer.characters(" + fmt + ");\n";
        }

        fn.body = body;
        return fn;
      }

      // Handle attributes first
      emit_write_attributes(body, ct.attributes(), resolver.schemas, resolver);
      emit_write_attribute_group_refs(body, ct.attribute_group_refs(),
                                      resolver.schemas, resolver);

      if (ct.attribute_wildcard().has_value()) {
        body += "  for (const auto& a : value.any_attribute) {\n";
        body += "    writer.attribute(a.name(), a.value());\n";
        body += "  }\n";
      }

      // Handle mixed content
      if (ct.mixed() && (ct.content().kind == content_kind::mixed ||
                         ct.content().kind == content_kind::element_only)) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          if (cc->content_model.has_value()) {
            body += "  for (const auto& item : value.content) {\n";
            body += "    std::visit([&](const auto& v) {\n";
            body += "      using T = std::decay_t<decltype(v)>;\n";
            body += "      if constexpr (std::is_same_v<T, std::string>) {\n";
            body += "        writer.characters(v);\n";
            body += "      }\n";
            // TODO: handle element alternatives in mixed content
            body += "    }, item);\n";
            body += "  }\n";
          }
        }
        fn.body = body;
        return fn;
      }

      // Handle element_only content
      if (ct.content().kind == content_kind::element_only) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          // Extension: write base fields first
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            emit_write_base_fields(body, resolver.schemas, cc->base_type_name,
                                   resolver, ct.name());
          }

          if (cc->content_model.has_value()) {
            emit_write_particles(body, cc->content_model->particles(),
                                 cc->content_model->compositor(), resolver,
                                 ct.name());
          }
        }
      }

      fn.body = body;
      return fn;
    }

    // ===== Deserialization code generation =====

    // Generate the parse expression for an attribute value
    std::string
    parse_expr(const std::string& text_expr, const qname& type_name,
               const schema_set& schemas, const type_resolver& resolver) {
      if (is_enum_type(schemas, type_name)) {
        std::string enum_name = to_cpp_identifier(type_name.local_name());
        return enum_name + "_from_string(" + text_expr + ")";
      }
      std::string cpp_type = resolver.resolve(type_name);
      return "xb::parse<" + cpp_type + ">(" + text_expr + ")";
    }

    // Element read info
    struct read_element_info {
      std::string field_name;
      qname element_name;
      qname type_name;
      occurrence occurs;
      bool nillable;
      bool is_recursive;
    };

    std::string
    emit_read_element(const read_element_info& info, const schema_set& schemas,
                      const type_resolver& resolver) {
      std::string field = "result." + info.field_name;
      bool is_complex = is_complex_type(schemas, info.type_name);
      std::string cpp_type = resolver.resolve(info.type_name);

      if (info.is_recursive && info.occurs.min_occurs == 0 &&
          !info.occurs.is_unbounded() && info.occurs.max_occurs <= 1) {
        // unique_ptr field
        std::string read_fn =
            "read_" + to_cpp_identifier(info.type_name.local_name());
        return "      " + field + " = std::make_unique<" + cpp_type + ">(" +
               read_fn + "(reader));\n";
      }

      if (info.occurs.is_unbounded() || info.occurs.max_occurs > 1) {
        // vector field -> push_back
        if (is_complex) {
          std::string read_fn =
              "read_" + to_cpp_identifier(info.type_name.local_name());
          return "      " + field + ".push_back(" + read_fn + "(reader));\n";
        }
        return "      " + field + ".push_back(xb::read_simple<" + cpp_type +
               ">(reader));\n";
      }

      // Required or optional (both assign directly — optional::operator= works)
      if (is_complex) {
        std::string read_fn =
            "read_" + to_cpp_identifier(info.type_name.local_name());
        return "      " + field + " = " + read_fn + "(reader);\n";
      }
      return "      " + field + " = xb::read_simple<" + cpp_type +
             ">(reader);\n";
    }

    // Forward declare
    void
    emit_read_particles(std::string& body,
                        const std::vector<particle>& particles,
                        compositor_kind compositor,
                        const type_resolver& resolver,
                        const qname& containing_type_name);

    void
    emit_read_particle_match(std::string& body, const particle& p,
                             const type_resolver& resolver,
                             const qname& containing_type_name,
                             bool& first_branch) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              bool is_recursive = (term.type_name() == containing_type_name);
              std::string qn = "xb::qname{\"" + term.name().namespace_uri() +
                               "\", \"" + term.name().local_name() + "\"}";
              std::string kw = first_branch ? "if" : "else if";
              body += "    " + kw + " (name == " + qn + ") {\n";
              body += emit_read_element(
                  {to_cpp_identifier(term.name().local_name()), term.name(),
                   term.type_name(), p.occurs, term.nillable(), is_recursive},
                  resolver.schemas, resolver);
              body += "    }\n";
              first_branch = false;
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto* elem = resolver.schemas.find_element(term.ref);
              if (!elem) return;
              std::string qn = "xb::qname{\"" + elem->name().namespace_uri() +
                               "\", \"" + elem->name().local_name() + "\"}";
              std::string kw = first_branch ? "if" : "else if";
              body += "    " + kw + " (name == " + qn + ") {\n";
              body += emit_read_element(
                  {to_cpp_identifier(elem->name().local_name()), elem->name(),
                   elem->type_name(), p.occurs, elem->nillable(), false},
                  resolver.schemas, resolver);
              body += "    }\n";
              first_branch = false;
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def) {
                for (const auto& gp : group_def->group().particles())
                  emit_read_particle_match(body, gp, resolver,
                                           containing_type_name, first_branch);
              }
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) {
                for (const auto& sp : term->particles())
                  emit_read_particle_match(body, sp, resolver,
                                           containing_type_name, first_branch);
              }
            } else if constexpr (std::is_same_v<T, wildcard>) {
              std::string kw = first_branch ? "if" : "else if";
              body += "    " + kw + " (true) {\n";
              body +=
                  "      result.any.emplace_back(xb::any_element(reader));\n";
              body += "    }\n";
              first_branch = false;
            }
          },
          p.term);
    }

    void
    emit_read_particles(std::string& body,
                        const std::vector<particle>& particles,
                        compositor_kind compositor,
                        const type_resolver& resolver,
                        const qname& containing_type_name) {
      if (compositor == compositor_kind::choice) {
        // Choice: element name selects variant alternative
        bool first_branch = true;
        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, element_decl>) {
                  std::string qn = "xb::qname{\"" +
                                   term.name().namespace_uri() + "\", \"" +
                                   term.name().local_name() + "\"}";
                  std::string cpp_type = resolver.resolve(term.type_name());
                  bool is_complex =
                      is_complex_type(resolver.schemas, term.type_name());

                  std::string kw = first_branch ? "if" : "else if";
                  body += "    " + kw + " (name == " + qn + ") {\n";
                  if (is_complex) {
                    std::string read_fn =
                        "read_" +
                        to_cpp_identifier(term.type_name().local_name());
                    body += "      result.choice = " + read_fn + "(reader);\n";
                  } else {
                    body += "      result.choice = xb::read_simple<" +
                            cpp_type + ">(reader);\n";
                  }
                  body += "    }\n";
                  first_branch = false;
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (!elem) return;
                  std::string qn = "xb::qname{\"" +
                                   elem->name().namespace_uri() + "\", \"" +
                                   elem->name().local_name() + "\"}";
                  std::string cpp_type = resolver.resolve(elem->type_name());
                  std::string kw = first_branch ? "if" : "else if";
                  body += "    " + kw + " (name == " + qn + ") {\n";
                  body += "      result.choice = xb::read_simple<" + cpp_type +
                          ">(reader);\n";
                  body += "    }\n";
                  first_branch = false;
                }
              },
              p.term);
        }
        return;
      }

      // Sequence or all: dispatch by element name
      bool first_branch = true;
      for (const auto& p : particles)
        emit_read_particle_match(body, p, resolver, containing_type_name,
                                 first_branch);

      // Skip unknown elements
      if (!first_branch) {
        body += "    else {\n";
        body += "      xb::skip_element(reader);\n";
        body += "    }\n";
      }
    }

    void
    emit_read_attributes(std::string& body,
                         const std::vector<attribute_use>& attrs,
                         const schema_set& schemas,
                         const type_resolver& resolver) {
      for (const auto& attr : attrs) {
        std::string name = to_cpp_identifier(attr.name.local_name());
        std::string qn = "xb::qname{\"" + attr.name.namespace_uri() + "\", \"" +
                         attr.name.local_name() + "\"}";

        if (attr.required) {
          std::string expr = parse_expr("reader.attribute_value(" + qn + ")",
                                        attr.type_name, schemas, resolver);
          body += "  result." + name + " = " + expr + ";\n";
        } else {
          body += "  {\n";
          body += "    auto attr_val__ = reader.attribute_value(" + qn + ");\n";
          body += "    if (!attr_val__.empty()) {\n";
          std::string expr =
              parse_expr("attr_val__", attr.type_name, schemas, resolver);
          body += "      result." + name + " = " + expr + ";\n";
          body += "    }\n";
          body += "  }\n";
        }
      }
    }

    void
    emit_read_attribute_group_refs(std::string& body,
                                   const std::vector<attribute_group_ref>& refs,
                                   const schema_set& schemas,
                                   const type_resolver& resolver) {
      for (const auto& ref : refs) {
        auto* group_def = schemas.find_attribute_group_def(ref.ref);
        if (group_def) {
          emit_read_attributes(body, group_def->attributes(), schemas,
                               resolver);
          emit_read_attribute_group_refs(
              body, group_def->attribute_group_refs(), schemas, resolver);
        }
      }
    }

    void
    emit_read_base_fields(std::string& body, const schema_set& schemas,
                          const qname& base_name, const type_resolver& resolver,
                          const qname& containing_type_name,
                          bool& first_branch) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;

      if (base_ct->content().kind == content_kind::element_only ||
          base_ct->content().kind == content_kind::mixed) {
        if (auto* cc =
                std::get_if<complex_content>(&base_ct->content().detail)) {
          if (!cc->base_type_name.namespace_uri().empty() ||
              !cc->base_type_name.local_name().empty()) {
            if (cc->derivation == derivation_method::extension)
              emit_read_base_fields(body, schemas, cc->base_type_name, resolver,
                                    containing_type_name, first_branch);
          }
          if (cc->content_model.has_value()) {
            for (const auto& p : cc->content_model->particles())
              emit_read_particle_match(body, p, resolver, containing_type_name,
                                       first_branch);
          }
        }
      }
    }

    cpp_function
    generate_read_function(const complex_type& ct,
                           const type_resolver& resolver) {
      cpp_function fn;
      std::string struct_name = to_cpp_identifier(ct.name().local_name());
      fn.return_type = struct_name;
      fn.name = "read_" + struct_name;
      fn.parameters = "xb::xml_reader& reader";

      std::string body;
      body += "  " + struct_name + " result;\n";

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        emit_read_attributes(body, ct.attributes(), resolver.schemas, resolver);
        emit_read_attribute_group_refs(body, ct.attribute_group_refs(),
                                       resolver.schemas, resolver);

        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          std::string cpp_type = resolver.resolve(sc->base_type_name);
          body += "  result.value = xb::parse<" + cpp_type +
                  ">(xb::read_text(reader));\n";
        }

        body += "  return result;\n";
        fn.body = body;
        return fn;
      }

      // Read attributes from current start_element
      emit_read_attributes(body, ct.attributes(), resolver.schemas, resolver);
      emit_read_attribute_group_refs(body, ct.attribute_group_refs(),
                                     resolver.schemas, resolver);

      // Read child elements
      bool has_children = false;
      if (ct.content().kind == content_kind::element_only ||
          ct.content().kind == content_kind::mixed) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          bool has_particles = (cc->content_model.has_value() &&
                                !cc->content_model->particles().empty());
          bool has_extension = cc->derivation == derivation_method::extension &&
                               (!cc->base_type_name.namespace_uri().empty() ||
                                !cc->base_type_name.local_name().empty());
          has_children = has_particles || has_extension;
        }
      }

      if (has_children) {
        body += "  auto start_depth = reader.depth();\n";
        body += "  while (reader.read()) {\n";
        body += "    if (reader.node_type() == xb::xml_node_type::end_element "
                "&& reader.depth() == start_depth) break;\n";
        body += "    if (reader.node_type() != "
                "xb::xml_node_type::start_element) continue;\n";
        body += "    auto& name = reader.name();\n";

        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          bool first_branch = true;

          // Extension: read base fields first
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            emit_read_base_fields(body, resolver.schemas, cc->base_type_name,
                                  resolver, ct.name(), first_branch);
          }

          if (cc->content_model.has_value()) {
            emit_read_particles(body, cc->content_model->particles(),
                                cc->content_model->compositor(), resolver,
                                ct.name());
          } else if (!first_branch) {
            body += "    else {\n";
            body += "      xb::skip_element(reader);\n";
            body += "    }\n";
          }
        }

        body += "  }\n";
      }

      body += "  return result;\n";
      fn.body = body;
      return fn;
    }

  } // namespace

  std::vector<cpp_file>
  codegen::generate() const {
    std::vector<cpp_file> files;

    for (const auto& s : schemas_.schemas()) {
      std::set<std::string> referenced_namespaces;

      type_resolver resolver{schemas_, types_, options_, s.target_namespace(),
                             referenced_namespaces};

      std::vector<cpp_decl> declarations;

      for (const auto& st : s.simple_types())
        declarations.push_back(translate_simple_type(st, resolver));

      for (const auto& ct : s.complex_types())
        declarations.push_back(translate_complex_type(ct, resolver));

      // Order type declarations first (structs, enums, aliases, forward decls)
      auto ordered_types = order_declarations(std::move(declarations));

      // Build a map from type name -> complex_type for ordered generation
      std::unordered_map<std::string, const complex_type*> ct_by_name;
      for (const auto& ct : s.complex_types())
        ct_by_name[to_cpp_identifier(ct.name().local_name())] = &ct;

      // Generate write_ functions in the same order as the sorted type decls
      // (which respects dependencies: if A depends on B, B comes first)
      // Collect struct names first, then generate to avoid iterator
      // invalidation
      std::vector<const complex_type*> ordered_cts;
      for (const auto& decl : ordered_types) {
        auto* st_decl = std::get_if<cpp_struct>(&decl);
        if (!st_decl) continue;
        auto it = ct_by_name.find(st_decl->name);
        if (it != ct_by_name.end()) ordered_cts.push_back(it->second);
      }
      for (const auto* ct_ptr : ordered_cts) {
        ordered_types.push_back(generate_read_function(*ct_ptr, resolver));
        ordered_types.push_back(generate_write_function(*ct_ptr, resolver));
      }

      cpp_namespace ns;
      ns.name = cpp_namespace_for(s.target_namespace(), options_);
      ns.declarations = std::move(ordered_types);

      auto includes = compute_includes(referenced_namespaces,
                                       schemas_.schemas(), ns.declarations);

      cpp_file file;
      file.filename = filename_for_namespace(s.target_namespace());
      file.includes = std::move(includes);
      file.namespaces.push_back(std::move(ns));

      files.push_back(std::move(file));
    }

    return files;
  }

} // namespace xb
