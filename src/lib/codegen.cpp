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

      cpp_namespace ns;
      ns.name = cpp_namespace_for(s.target_namespace(), options_);
      ns.declarations = order_declarations(std::move(declarations));

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
