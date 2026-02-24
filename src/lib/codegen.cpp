#include <xb/codegen.hpp>
#include <xb/naming.hpp>
#include <xb/open_content.hpp>
#include <xb/xpath_expr.hpp>

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

    // Translate a CTA XPath test to a C++ condition.
    // Supports: @attr = 'value', @attr != 'value'
    // Returns nullopt if the expression is unsupported.
    struct cta_condition {
      std::string attr_name;
      std::string op;
      std::string value;
    };

    std::optional<cta_condition>
    parse_cta_test(const std::string& xpath) {
      // Pattern: @attr = 'value' or @attr != 'value'
      // Trim whitespace
      auto trim = [](std::string_view sv) {
        while (!sv.empty() && sv.front() == ' ')
          sv.remove_prefix(1);
        while (!sv.empty() && sv.back() == ' ')
          sv.remove_suffix(1);
        return sv;
      };

      std::string_view sv = trim(xpath);

      // Must start with @
      if (sv.empty() || sv.front() != '@') return std::nullopt;
      sv.remove_prefix(1);

      // Find operator (= or !=)
      auto eq_pos = sv.find("!=");
      std::string op;
      std::string_view attr_part;
      std::string_view value_part;

      if (eq_pos != std::string_view::npos) {
        op = "!=";
        attr_part = trim(sv.substr(0, eq_pos));
        value_part = trim(sv.substr(eq_pos + 2));
      } else {
        eq_pos = sv.find('=');
        if (eq_pos == std::string_view::npos) return std::nullopt;
        op = "==";
        attr_part = trim(sv.substr(0, eq_pos));
        value_part = trim(sv.substr(eq_pos + 1));
      }

      // Value must be quoted with ' or "
      if (value_part.size() < 2) return std::nullopt;
      char quote = value_part.front();
      if ((quote != '\'' && quote != '"') || value_part.back() != quote)
        return std::nullopt;
      value_part = value_part.substr(1, value_part.size() - 2);

      return cta_condition{std::string(attr_part), op, std::string(value_part)};
    }

    struct resolved_alternative {
      std::string cpp_type;
      qname type_name;
    };

    // Deduplicate CTA alternatives by resolved C++ type.
    // Returns unique alternatives in first-seen order.
    // If all alternatives resolve to the same type, returns a single entry.
    std::vector<resolved_alternative>
    deduplicate_alternatives(const std::vector<type_alternative>& alts,
                             const type_resolver& resolver) {
      std::vector<resolved_alternative> result;
      std::set<std::string> seen;
      for (const auto& alt : alts) {
        std::string cpp_type = resolver.resolve(alt.type_name);
        if (seen.insert(cpp_type).second) {
          result.push_back({std::move(cpp_type), alt.type_name});
        }
      }
      return result;
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
              // Check for conditional type assignment (CTA)
              auto deduped =
                  deduplicate_alternatives(term.type_alternatives(), resolver);
              if (deduped.size() > 1) {
                std::string variant = "std::variant<";
                bool first = true;
                for (const auto& alt : deduped) {
                  if (!first) variant += ", ";
                  variant += alt.cpp_type;
                  first = false;
                }
                variant += ">";

                std::string type = variant;
                if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                  type = "std::vector<" + type + ">";
                else if (p.occurs.min_occurs == 0)
                  type = "std::optional<" + type + ">";

                fields.push_back(
                    {type, to_cpp_identifier(term.name().local_name()), ""});
              } else if (deduped.size() == 1) {
                // Single unique CTA type — use it directly
                std::string type = deduped[0].cpp_type;
                if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                  type = "std::vector<" + type + ">";
                else if (p.occurs.min_occurs == 0)
                  type = "std::optional<" + type + ">";
                fields.push_back(
                    {type, to_cpp_identifier(term.name().local_name()), ""});
              } else {
                // No alternatives — use normal field type
                fields.push_back(
                    {field_type_for_element(term, p.occurs, resolver,
                                            containing_type_name),
                     to_cpp_identifier(term.name().local_name()),
                     default_value_for_element(term)});
              }
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

    bool
    has_wildcard_particle(const complex_content& cc) {
      if (!cc.content_model.has_value()) return false;
      for (const auto& p : cc.content_model->particles())
        if (std::holds_alternative<wildcard>(p.term)) return true;
      return false;
    }

    std::optional<open_content>
    effective_open_content(const complex_type& ct, const schema& s) {
      if (ct.open_content_value().has_value()) {
        if (ct.open_content_value()->mode == open_content_mode::none)
          return std::nullopt;
        return ct.open_content_value();
      }
      if (s.default_open_content().has_value()) {
        if (ct.content().kind == content_kind::empty &&
            !s.default_open_content_applies_to_empty())
          return std::nullopt;
        return s.default_open_content();
      }
      return std::nullopt;
    }

    cpp_decl
    translate_complex_type(const complex_type& ct,
                           const type_resolver& resolver,
                           const schema& current_schema) {
      cpp_struct s;
      s.name = to_cpp_identifier(ct.name().local_name());
      s.generate_equality = true;

      // Compute effective open content
      auto eff_oc = effective_open_content(ct, current_schema);
      bool content_has_wildcard = false;
      if (auto* cc = std::get_if<complex_content>(&ct.content().detail))
        content_has_wildcard = has_wildcard_particle(*cc);
      bool needs_oc_field = eff_oc.has_value() && !content_has_wildcard;

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
        if (needs_oc_field)
          s.fields.push_back(
              {"std::vector<xb::any_element>", "open_content", ""});
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

      if (needs_oc_field)
        s.fields.push_back(
            {"std::vector<xb::any_element>", "open_content", ""});

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

    void
    add_type_includes(std::set<std::string>& includes,
                      const std::string& type_expr) {
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
    }

    void
    add_cross_namespace_includes(std::set<std::string>& includes,
                                 const std::set<std::string>& referenced_ns,
                                 const std::vector<schema>& schemas) {
      for (const auto& ref_ns : referenced_ns) {
        for (const auto& s : schemas) {
          if (s.target_namespace() == ref_ns) {
            std::string filename =
                to_snake_case(ref_ns.substr(ref_ns.rfind('/') + 1)) + ".hpp";
            includes.insert("\"" + filename + "\"");
          }
        }
      }
    }

    bool
    has_enum_decl(const std::vector<cpp_decl>& declarations) {
      return std::any_of(declarations.begin(), declarations.end(),
                         [](const cpp_decl& d) {
                           return std::holds_alternative<cpp_enum>(d);
                         });
    }

    bool
    has_function_decl(const std::vector<cpp_decl>& declarations) {
      return std::any_of(declarations.begin(), declarations.end(),
                         [](const cpp_decl& d) {
                           return std::holds_alternative<cpp_function>(d);
                         });
    }

    std::vector<cpp_include>
    compute_includes(const std::set<std::string>& referenced_namespaces,
                     const std::vector<schema>& schemas,
                     const std::vector<cpp_decl>& declarations,
                     file_kind kind = file_kind::header,
                     const std::string& self_header = "") {
      std::set<std::string> includes;

      if (kind == file_kind::source) {
        // Source file: include self header + runtime
        if (!self_header.empty()) includes.insert("\"" + self_header + "\"");
        if (has_function_decl(declarations)) {
          includes.insert("\"xb/xml_value.hpp\"");
          includes.insert("\"xb/xml_io.hpp\"");
          includes.insert("\"xb/xml_reader.hpp\"");
          includes.insert("\"xb/xml_writer.hpp\"");
        }
      } else {
        // Header file: type includes + cross-namespace + enum includes
        for (const auto& decl : declarations) {
          std::visit(
              [&](const auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, cpp_struct>) {
                  for (const auto& f : d.fields)
                    add_type_includes(includes, f.type);
                } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
                  add_type_includes(includes, d.target);
                }
              },
              decl);
        }

        if (has_enum_decl(declarations)) {
          includes.insert("<stdexcept>");
          includes.insert("<string>");
          includes.insert("<string_view>");
        }

        // Check if functions are inline or just declarations
        bool has_non_inline_fn = false;
        for (const auto& decl : declarations) {
          if (auto* fn = std::get_if<cpp_function>(&decl)) {
            if (!fn->is_inline) {
              has_non_inline_fn = true;
              break;
            }
          }
        }
        if (has_function_decl(declarations)) {
          // Always include reader/writer headers — needed for function
          // parameter types in both declarations and definitions
          includes.insert("\"xb/xml_reader.hpp\"");
          includes.insert("\"xb/xml_writer.hpp\"");
          if (!has_non_inline_fn) {
            // All functions inline (header_only mode): also include
            // value/io headers needed for function bodies
            includes.insert("\"xb/xml_value.hpp\"");
            includes.insert("\"xb/xml_io.hpp\"");
          }
        }

        add_cross_namespace_includes(includes, referenced_namespaces, schemas);
      }

      std::vector<cpp_include> result;
      result.reserve(includes.size());
      for (auto& inc : includes)
        result.push_back({inc});
      return result;
    }

    std::string
    stem_for_namespace(const std::string& target_ns) {
      if (target_ns.empty()) return "generated";

      auto last_sep = target_ns.rfind('/');
      std::string segment;
      if (last_sep != std::string::npos)
        segment = target_ns.substr(last_sep + 1);
      else
        segment = target_ns;

      return to_snake_case(segment);
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
              auto deduped =
                  deduplicate_alternatives(term.type_alternatives(), resolver);
              if (deduped.size() > 1) {
                // CTA: std::visit dispatch over variant alternatives
                std::string field =
                    "value." + to_cpp_identifier(term.name().local_name());
                std::string qn = "xb::qname{\"" + term.name().namespace_uri() +
                                 "\", \"" + term.name().local_name() + "\"}";

                auto emit_visit = [&](const std::string& val_expr) {
                  body += "  std::visit([&](const auto& v) {\n";
                  body += "    using VT = std::decay_t<decltype(v)>;\n";
                  bool first = true;
                  for (const auto& alt : deduped) {
                    std::string kw = first ? "if" : "else if";
                    body += "    " + kw + " constexpr (std::is_same_v<VT, " +
                            alt.cpp_type + ">) {\n";
                    body += "      writer.start_element(" + qn + ");\n";
                    if (is_complex_type(resolver.schemas, alt.type_name)) {
                      std::string write_fn =
                          "write_" +
                          to_cpp_identifier(alt.type_name.local_name());
                      body += "      " + write_fn + "(v, writer);\n";
                    } else {
                      body +=
                          "      xb::write_simple(writer, " + qn + ", v);\n";
                    }
                    body += "      writer.end_element();\n";
                    body += "    }\n";
                    first = false;
                  }
                  body += "  }, " + val_expr + ");\n";
                };

                if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1) {
                  body += "  for (const auto& item : " + field + ") {\n";
                  emit_visit("item");
                  body += "  }\n";
                } else if (p.occurs.min_occurs == 0) {
                  body += "  if (" + field + ") {\n";
                  emit_visit("*" + field);
                  body += "  }\n";
                } else {
                  emit_visit(field);
                }
              } else if (deduped.size() == 1) {
                // Single unique CTA type — write using that type
                emit_write_element(body,
                                   {to_cpp_identifier(term.name().local_name()),
                                    term.name(), deduped[0].type_name, p.occurs,
                                    term.nillable(), false},
                                   resolver.schemas, resolver);
              } else {
                bool is_recursive = (term.type_name() == containing_type_name);
                emit_write_element(body,
                                   {to_cpp_identifier(term.name().local_name()),
                                    term.name(), term.type_name(), p.occurs,
                                    term.nillable(), is_recursive},
                                   resolver.schemas, resolver);
              }
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
                            const type_resolver& resolver,
                            const schema& current_schema) {
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

      // Write open content elements (suffix position)
      {
        auto eff_oc = effective_open_content(ct, current_schema);
        bool wc = false;
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail))
          wc = has_wildcard_particle(*cc);
        if (eff_oc.has_value() && !wc) {
          body += "  for (const auto& e : value.open_content) {\n";
          body += "    e.write(writer);\n";
          body += "  }\n";
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
                        const qname& containing_type_name,
                        bool has_open_content = false);

    void
    emit_read_particle_match(std::string& body, const particle& p,
                             const type_resolver& resolver,
                             const qname& containing_type_name,
                             bool& first_branch) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              std::string qn = "xb::qname{\"" + term.name().namespace_uri() +
                               "\", \"" + term.name().local_name() + "\"}";
              std::string kw = first_branch ? "if" : "else if";
              body += "    " + kw + " (name == " + qn + ") {\n";

              {
                auto deduped = deduplicate_alternatives(
                    term.type_alternatives(), resolver);
                if (deduped.size() > 1) {
                  // CTA: dispatch based on attribute values
                  std::string field =
                      "result." + to_cpp_identifier(term.name().local_name());

                  auto emit_read_alt = [&](const type_alternative& alt) {
                    bool is_complex =
                        is_complex_type(resolver.schemas, alt.type_name);
                    if (is_complex) {
                      std::string read_fn =
                          "read_" +
                          to_cpp_identifier(alt.type_name.local_name());
                      if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                        body += "        " + field + ".push_back(" + read_fn +
                                "(reader));\n";
                      else
                        body += "        " + field + " = " + read_fn +
                                "(reader);\n";
                    } else {
                      std::string cpp_type = resolver.resolve(alt.type_name);
                      if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                        body += "        " + field +
                                ".push_back(xb::read_simple<" + cpp_type +
                                ">(reader));\n";
                      else
                        body += "        " + field + " = xb::read_simple<" +
                                cpp_type + ">(reader);\n";
                    }
                  };

                  bool first_alt = true;
                  const type_alternative* default_alt = nullptr;
                  for (const auto& alt : term.type_alternatives()) {
                    if (!alt.test.has_value()) {
                      default_alt = &alt;
                      continue;
                    }
                    auto cond = parse_cta_test(alt.test.value());
                    if (!cond) {
                      body += "      // WARNING: unsupported CTA test "
                              "expression: '" +
                              alt.test.value() + "' — alternative skipped\n";
                      continue;
                    }

                    std::string akw = first_alt ? "if" : "else if";
                    body += "      " + akw +
                            " (reader.attribute_value(xb::qname{\"\", \"" +
                            cond->attr_name + "\"}) " + cond->op + " \"" +
                            cond->value + "\") {\n";
                    emit_read_alt(alt);
                    body += "      }\n";
                    first_alt = false;
                  }
                  if (default_alt) {
                    body += "      else {\n";
                    emit_read_alt(*default_alt);
                    body += "      }\n";
                  }
                } else if (deduped.size() == 1) {
                  // Single unique CTA type — read using that type
                  body += emit_read_element(
                      {to_cpp_identifier(term.name().local_name()), term.name(),
                       deduped[0].type_name, p.occurs, term.nillable(), false},
                      resolver.schemas, resolver);
                } else {
                  // No alternatives — normal read path
                  bool is_recursive =
                      (term.type_name() == containing_type_name);
                  body += emit_read_element(
                      {to_cpp_identifier(term.name().local_name()), term.name(),
                       term.type_name(), p.occurs, term.nillable(),
                       is_recursive},
                      resolver.schemas, resolver);
                }
              }

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
                        const qname& containing_type_name,
                        bool has_open_content) {
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

      // Handle unknown elements
      if (!first_branch) {
        if (has_open_content) {
          body += "    else {\n";
          body += "      result.open_content.emplace_back("
                  "xb::any_element(reader));\n";
          body += "    }\n";
        } else {
          body += "    else {\n";
          body += "      xb::skip_element(reader);\n";
          body += "    }\n";
        }
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
                           const type_resolver& resolver,
                           const schema& current_schema) {
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

      // Compute effective open content
      auto eff_oc = effective_open_content(ct, current_schema);
      bool content_has_wildcard = false;
      if (auto* cc = std::get_if<complex_content>(&ct.content().detail))
        content_has_wildcard = has_wildcard_particle(*cc);
      bool has_oc = eff_oc.has_value() && !content_has_wildcard;

      // Read child elements
      bool has_children = false;
      bool has_particles = false;
      if (ct.content().kind == content_kind::element_only ||
          ct.content().kind == content_kind::mixed) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          has_particles = (cc->content_model.has_value() &&
                           !cc->content_model->particles().empty());
          bool has_extension = cc->derivation == derivation_method::extension &&
                               (!cc->base_type_name.namespace_uri().empty() ||
                                !cc->base_type_name.local_name().empty());
          has_children = has_particles || has_extension;
        }
      }
      has_children = has_children || has_oc;

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
                                ct.name(), has_oc);
          } else if (!first_branch) {
            if (has_oc) {
              body += "    else {\n";
              body += "      result.open_content.emplace_back("
                      "xb::any_element(reader));\n";
              body += "    }\n";
            } else {
              body += "    else {\n";
              body += "      xb::skip_element(reader);\n";
              body += "    }\n";
            }
          }
        }

        // Open content on empty/simple type (no complex_content detail)
        if (has_oc && !has_particles &&
            !std::holds_alternative<complex_content>(ct.content().detail)) {
          body += "    result.open_content.emplace_back("
                  "xb::any_element(reader));\n";
        }

        body += "  }\n";
      }

      body += "  return result;\n";
      fn.body = body;
      return fn;
    }

    // Generate a validate_<type>() function for a complex type with assertions.
    // Returns nullopt if the type has no assertions.
    std::optional<cpp_function>
    generate_validate_function(const complex_type& ct) {
      if (ct.assertions().empty()) return std::nullopt;

      std::string struct_name = to_cpp_identifier(ct.name().local_name());
      xpath_context ctx{"value."};

      cpp_function fn;
      fn.return_type = "bool";
      fn.name = "validate_" + struct_name;
      fn.parameters = "const " + struct_name + "& value";

      std::string body = "  return ";
      bool first = true;

      for (const auto& a : ct.assertions()) {
        auto translated = translate_xpath_assertion(a.test, ctx);
        if (!translated.has_value()) {
          // Unsupported expression: emit warning, function returns true
          fn.body = "  // WARNING: unsupported assertion: '" + a.test + "'\n";
          fn.body += "  return true;\n";
          return fn;
        }
        if (!first) body += "\n      && ";
        body += translated.value();
        first = false;
      }

      body += ";\n";
      fn.body = body;
      return fn;
    }

    // Generate a validate_<type>() function for a simple type with assertions.
    // Returns nullopt if the type has no assertions.
    std::optional<cpp_function>
    generate_simple_validate_function(const simple_type& st,
                                      const type_resolver& resolver) {
      if (st.assertions().empty()) return std::nullopt;

      std::string type_name = to_cpp_identifier(st.name().local_name());
      std::string cpp_type = resolver.resolve(st.base_type_name());
      xpath_context ctx{"value"};

      cpp_function fn;
      fn.return_type = "bool";
      fn.name = "validate_" + type_name;
      fn.parameters = "const " + cpp_type + "& value";

      std::string body = "  return ";
      bool first = true;

      for (const auto& a : st.assertions()) {
        auto translated = translate_xpath_assertion(a.test, ctx);
        if (!translated.has_value()) {
          fn.body = "  // WARNING: unsupported assertion: '" + a.test + "'\n";
          fn.body += "  return true;\n";
          return fn;
        }
        if (!first) body += "\n      && ";
        body += translated.value();
        first = false;
      }

      body += ";\n";
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
        declarations.push_back(translate_complex_type(ct, resolver, s));

      // Order type declarations first (structs, enums, aliases, forward decls)
      auto ordered_types = order_declarations(std::move(declarations));

      // Build a map from type name -> complex_type for ordered generation
      std::unordered_map<std::string, const complex_type*> ct_by_name;
      for (const auto& ct : s.complex_types())
        ct_by_name[to_cpp_identifier(ct.name().local_name())] = &ct;

      // Generate read_/write_ functions in dependency order
      std::vector<const complex_type*> ordered_cts;
      for (const auto& decl : ordered_types) {
        auto* st_decl = std::get_if<cpp_struct>(&decl);
        if (!st_decl) continue;
        auto it = ct_by_name.find(st_decl->name);
        if (it != ct_by_name.end()) ordered_cts.push_back(it->second);
      }
      for (const auto* ct_ptr : ordered_cts) {
        ordered_types.push_back(generate_read_function(*ct_ptr, resolver, s));
        ordered_types.push_back(generate_write_function(*ct_ptr, resolver, s));
      }

      // Generate validate_ functions for complex types with assertions
      for (const auto* ct_ptr : ordered_cts) {
        if (auto vf = generate_validate_function(*ct_ptr))
          ordered_types.push_back(std::move(*vf));
      }

      // Generate validate_ functions for simple types with assertions
      for (const auto& st : s.simple_types()) {
        if (auto vf = generate_simple_validate_function(st, resolver))
          ordered_types.push_back(std::move(*vf));
      }

      // In split mode, mark read_/write_ functions as non-inline
      if (options_.mode == output_mode::split ||
          options_.mode == output_mode::file_per_type) {
        for (auto& decl : ordered_types) {
          if (auto* fn = std::get_if<cpp_function>(&decl))
            fn->is_inline = false;
        }
      }

      std::string ns_name = cpp_namespace_for(s.target_namespace(), options_);
      std::string stem = stem_for_namespace(s.target_namespace());
      std::string header_filename = stem + ".hpp";

      if (options_.mode == output_mode::header_only) {
        // Single header file per namespace (current behavior)
        cpp_namespace ns;
        ns.name = ns_name;
        ns.declarations = std::move(ordered_types);

        auto includes = compute_includes(referenced_namespaces,
                                         schemas_.schemas(), ns.declarations);

        cpp_file file;
        file.filename = header_filename;
        file.kind = file_kind::header;
        file.includes = std::move(includes);
        file.namespaces.push_back(std::move(ns));

        files.push_back(std::move(file));
      } else if (options_.mode == output_mode::split) {
        // Header + source file per namespace
        cpp_namespace ns;
        ns.name = ns_name;
        ns.declarations = std::move(ordered_types);

        auto header_includes =
            compute_includes(referenced_namespaces, schemas_.schemas(),
                             ns.declarations, file_kind::header);

        auto source_includes = compute_includes(
            referenced_namespaces, schemas_.schemas(), ns.declarations,
            file_kind::source, header_filename);

        cpp_file header;
        header.filename = header_filename;
        header.kind = file_kind::header;
        header.includes = std::move(header_includes);
        header.namespaces.push_back(ns); // copy — shared with source

        cpp_file source;
        source.filename = stem + ".cpp";
        source.kind = file_kind::source;
        source.includes = std::move(source_includes);
        source.namespaces.push_back(std::move(ns));

        files.push_back(std::move(header));
        files.push_back(std::move(source));
      } else if (options_.mode == output_mode::file_per_type) {
        // Per-type headers + umbrella header + source file

        // Partition declarations into per-type groups
        // Each struct or enum gets its own header file
        struct type_group {
          std::string type_name;
          std::vector<cpp_decl> decls;
        };

        std::vector<type_group> groups;
        std::vector<cpp_decl> function_decls;

        for (auto& decl : ordered_types) {
          if (auto* st = std::get_if<cpp_struct>(&decl)) {
            groups.push_back({st->name, {std::move(decl)}});
          } else if (auto* en = std::get_if<cpp_enum>(&decl)) {
            groups.push_back({en->name, {std::move(decl)}});
          } else if (auto* alias = std::get_if<cpp_type_alias>(&decl)) {
            // Aliases go with their name as standalone type files
            groups.push_back({alias->name, {std::move(decl)}});
          } else if (auto* fwd = std::get_if<cpp_forward_decl>(&decl)) {
            // Forward decls go with their associated type — find the group
            bool found = false;
            for (auto& g : groups) {
              if (g.type_name == fwd->name) {
                g.decls.insert(g.decls.begin(), std::move(decl));
                found = true;
                break;
              }
            }
            if (!found) {
              // Forward decl for a type not yet seen — create group
              groups.push_back({fwd->name, {std::move(decl)}});
            }
          } else if (std::holds_alternative<cpp_function>(decl)) {
            function_decls.push_back(std::move(decl));
          }
        }

        // Emit per-type header files
        std::vector<std::string> per_type_filenames;
        for (const auto& group : groups) {
          std::string type_filename = stem + "_" + group.type_name + ".hpp";
          per_type_filenames.push_back(type_filename);

          // Compute includes for this type's declarations
          std::set<std::string> empty_ns_refs;
          auto type_includes =
              compute_includes(empty_ns_refs, schemas_.schemas(), group.decls,
                               file_kind::header);

          // Add includes for cross-type dependencies within the namespace
          auto deps = decl_dependencies(group.decls.back());
          for (const auto& dep_name : deps) {
            for (const auto& other : groups) {
              if (other.type_name == dep_name &&
                  other.type_name != group.type_name) {
                type_includes.push_back(
                    {"\"" + stem + "_" + other.type_name + ".hpp\""});
                break;
              }
            }
          }

          cpp_namespace type_ns;
          type_ns.name = ns_name;
          type_ns.declarations = group.decls;

          cpp_file type_file;
          type_file.filename = type_filename;
          type_file.kind = file_kind::header;
          type_file.includes = std::move(type_includes);
          type_file.namespaces.push_back(std::move(type_ns));

          files.push_back(std::move(type_file));
        }

        // Also add cross-namespace includes to per-type files
        if (!referenced_namespaces.empty()) {
          for (auto& file : files) {
            if (file.kind == file_kind::header &&
                file.filename != header_filename) {
              std::set<std::string> inc_set;
              add_cross_namespace_includes(inc_set, referenced_namespaces,
                                           schemas_.schemas());
              for (auto& inc : inc_set)
                file.includes.push_back({inc});
            }
          }
        }

        // Emit umbrella header
        cpp_file umbrella;
        umbrella.filename = header_filename;
        umbrella.kind = file_kind::header;
        for (const auto& pf : per_type_filenames)
          umbrella.includes.push_back({"\"" + pf + "\""});
        files.push_back(std::move(umbrella));

        // Emit source file with all function definitions
        cpp_namespace fn_ns;
        fn_ns.name = ns_name;
        fn_ns.declarations = std::move(function_decls);

        auto source_includes = compute_includes(
            referenced_namespaces, schemas_.schemas(), fn_ns.declarations,
            file_kind::source, header_filename);

        cpp_file source;
        source.filename = stem + ".cpp";
        source.kind = file_kind::source;
        source.includes = std::move(source_includes);
        source.namespaces.push_back(std::move(fn_ns));

        files.push_back(std::move(source));
      }
    }

    return files;
  }

} // namespace xb
