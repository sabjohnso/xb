#include <xb/codegen.hpp>
#include <xb/naming.hpp>
#include <xb/open_content.hpp>
#include <xb/xpath_expr.hpp>

#include "codegen_internal.hpp"
#include "json_codegen.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace xb {

  codegen::codegen(const schema_set& schemas, const type_map& types,
                   codegen_options options)
      : schemas_(schemas), types_(types), options_(std::move(options)) {}

  namespace {

    // Forward declarations for functions used before their definitions.
    bool
    is_complex_type(const schema_set& schemas, const qname& type_name);

    // Collect qnames of complex types that participate in dependency cycles.
    // A cycle exists when type A references type B (directly or via extension)
    // and B (transitively) references A.  Fields referencing cycle types need
    // std::unique_ptr to break the cycle at the C++ level.
    std::set<qname>
    find_cycle_types(const schema_set& schemas) {
      // Build type dependency graph
      std::vector<qname> type_names;
      std::unordered_map<std::string, std::size_t> name_to_idx;

      for (const auto& s : schemas.schemas()) {
        for (const auto& ct : s.complex_types()) {
          auto key = ct.name().namespace_uri() + "#" + ct.name().local_name();
          if (name_to_idx.find(key) == name_to_idx.end()) {
            name_to_idx[key] = type_names.size();
            type_names.push_back(ct.name());
          }
        }
      }

      std::size_t n = type_names.size();
      std::vector<std::set<std::size_t>> adj(n);

      // Helper to collect type references from a content model
      auto collect_refs = [&](const complex_type& ct,
                              [[maybe_unused]] auto& self) -> void {
        auto add_ref = [&](const qname& ref) {
          auto key = ref.namespace_uri() + "#" + ref.local_name();
          auto it = name_to_idx.find(key);
          if (it != name_to_idx.end()) {
            auto src_key =
                ct.name().namespace_uri() + "#" + ct.name().local_name();
            auto src_it = name_to_idx.find(src_key);
            if (src_it != name_to_idx.end())
              adj[src_it->second].insert(it->second);
          }
        };

        // Check base type (extension/restriction)
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          add_ref(cc->base_type_name);

          // Check element particles
          if (cc->content_model.has_value()) {
            std::function<void(const std::vector<particle>&)> scan_particles;
            scan_particles = [&](const std::vector<particle>& particles) {
              for (const auto& p : particles) {
                std::visit(
                    [&](const auto& term) {
                      using T = std::decay_t<decltype(term)>;
                      if constexpr (std::is_same_v<T, element_decl>) {
                        add_ref(term.type_name());
                      } else if constexpr (std::is_same_v<T, element_ref>) {
                        auto* elem = schemas.find_element(term.ref);
                        if (elem) add_ref(elem->type_name());
                      } else if constexpr (std::is_same_v<T, group_ref>) {
                        auto* g = schemas.find_model_group_def(term.ref);
                        if (g) scan_particles(g->group().particles());
                      } else if constexpr (std::is_same_v<
                                               T,
                                               std::unique_ptr<model_group>>) {
                        if (term) scan_particles(term->particles());
                      }
                    },
                    p.term);
              }
            };
            scan_particles(cc->content_model->particles());
          }
        }
        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          add_ref(sc->base_type_name);
        }
      };

      for (const auto& s : schemas.schemas())
        for (const auto& ct : s.complex_types())
          collect_refs(ct, collect_refs);

      // Tarjan's SCC algorithm
      std::vector<int> index_arr(n, -1);
      std::vector<int> lowlink(n, -1);
      std::vector<bool> on_stack(n, false);
      std::vector<std::size_t> stack;
      int current_index = 0;
      std::set<qname> cycle_types;

      std::function<void(std::size_t)> strongconnect = [&](std::size_t v) {
        index_arr[v] = lowlink[v] = current_index++;
        stack.push_back(v);
        on_stack[v] = true;

        for (auto w : adj[v]) {
          if (index_arr[w] == -1) {
            strongconnect(w);
            lowlink[v] = std::min(lowlink[v], lowlink[w]);
          } else if (on_stack[w]) {
            lowlink[v] = std::min(lowlink[v], index_arr[w]);
          }
        }

        if (lowlink[v] == index_arr[v]) {
          std::vector<std::size_t> scc;
          std::size_t w;
          do {
            w = stack.back();
            stack.pop_back();
            on_stack[w] = false;
            scc.push_back(w);
          } while (w != v);

          // SCCs with more than one node, or self-loops, are cycles
          if (scc.size() > 1) {
            for (auto idx : scc)
              cycle_types.insert(type_names[idx]);
          } else if (adj[scc[0]].count(scc[0])) {
            cycle_types.insert(type_names[scc[0]]);
          }
        }
      };

      for (std::size_t i = 0; i < n; ++i) {
        if (index_arr[i] == -1) strongconnect(i);
      }

      return cycle_types;
    }

    // type_resolver is defined in codegen_internal.hpp (shared with
    // json_codegen.cpp).

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

      // Check for recursive self-reference or mutual recursion (cycle)
      bool is_recursive = (elem.type_name() == containing_type_name) ||
                          resolver.is_cycle_type(elem.type_name());

      // Nillable -> optional
      if (elem.nillable() && !is_recursive)
        base_type = "std::optional<" + base_type + ">";

      // Cycle/self-reference -> unique_ptr to break the cycle.
      // This applies both to single-occurrence and sequence fields,
      // because std::vector<T> requires T to be complete when the
      // containing type is defined.
      if (is_recursive) base_type = "std::unique_ptr<" + base_type + ">";

      // Apply cardinality
      if (occurs.is_unbounded() || occurs.max_occurs > 1)
        return "std::vector<" + base_type + ">";

      // unique_ptr already encodes optionality via nullptr
      if (occurs.min_occurs == 0 && !is_recursive)
        return "std::optional<" + base_type + ">";

      return base_type;
    }

    // Check if a resolved C++ type can accept a numeric literal default.
    // Rejects class types (xb::integer, xb::decimal, std::string, etc.)
    // where a bare numeric literal would require multiple implicit
    // conversions through std::optional.
    bool
    is_numeric_cpp_type(const std::string& cpp_type) {
      // Class types and templates cannot accept bare numeric literals
      // through std::optional (two implicit conversions needed).
      static const std::set<std::string> non_numeric = {
          "std::string", "xb::integer", "xb::decimal",   "xb::qname",
          "xb::date",    "xb::time",    "xb::date_time", "xb::duration",
      };
      if (non_numeric.count(cpp_type)) return false;
      // Templates and other complex types
      if (cpp_type.find('<') != std::string::npos) return false;
      // Allowlist: only C++ built-in numeric types and their aliases
      static const std::set<std::string> numeric = {
          "bool",        "int",
          "unsigned",    "unsigned int",
          "short",       "unsigned short",
          "long",        "unsigned long",
          "long long",   "unsigned long long",
          "float",       "double",
          "long double", "char",
          "signed char", "unsigned char",
          "int8_t",      "int16_t",
          "int32_t",     "int64_t",
          "uint8_t",     "uint16_t",
          "uint32_t",    "uint64_t",
          "size_t",
      };
      return numeric.count(cpp_type) > 0;
    }

    // Check if a raw XSD default/fixed value is a valid C++ literal.
    // Rejects dates ("2010-11-16"), durations, etc. that would be
    // misinterpreted as arithmetic expressions.
    bool
    is_safe_cpp_literal(const std::string& val) {
      if (val.empty()) return false;
      if (val == "true" || val == "false") return true;

      // Numeric: optional sign, digits, optional decimal, optional exponent
      std::size_t i = 0;
      if (val[i] == '+' || val[i] == '-') ++i;
      if (i >= val.size()) return false;

      bool has_digits = false;
      while (i < val.size() &&
             std::isdigit(static_cast<unsigned char>(val[i]))) {
        has_digits = true;
        ++i;
      }
      if (i < val.size() && val[i] == '.') {
        ++i;
        while (i < val.size() &&
               std::isdigit(static_cast<unsigned char>(val[i])))
          ++i;
      }
      if (i < val.size() && (val[i] == 'e' || val[i] == 'E')) {
        ++i;
        if (i < val.size() && (val[i] == '+' || val[i] == '-')) ++i;
        while (i < val.size() &&
               std::isdigit(static_cast<unsigned char>(val[i])))
          ++i;
      }
      return has_digits && i == val.size();
    }

    std::string
    default_value_for_element(const element_decl& elem) {
      std::string val;
      if (elem.default_value().has_value())
        val = elem.default_value().value();
      else if (elem.fixed_value().has_value())
        val = elem.fixed_value().value();
      if (is_safe_cpp_literal(val)) return val;
      return "";
    }

    void
    translate_particles(const std::vector<particle>& particles,
                        compositor_kind compositor,
                        std::vector<cpp_field>& fields,
                        const type_resolver& resolver,
                        const qname& containing_type_name,
                        occurrence outer_occurs = {},
                        field_plan* plan = nullptr);

    void
    translate_particle_term(const particle& p, std::vector<cpp_field>& fields,
                            const type_resolver& resolver,
                            const qname& containing_type_name,
                            field_plan* plan = nullptr,
                            occurrence seq_outer_occurs = {1, 1}) {
      // Compute effective occurrence: if the enclosing sequence repeats,
      // inner particles inherit the repeating cardinality.
      occurrence eff_occurs = p.occurs;
      if (seq_outer_occurs.is_unbounded() || seq_outer_occurs.max_occurs > 1) {
        eff_occurs.max_occurs = xb::unbounded;
        eff_occurs.min_occurs = 0;
      }

      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
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
                if (eff_occurs.is_unbounded() || eff_occurs.max_occurs > 1)
                  type = "std::vector<" + type + ">";
                else if (eff_occurs.min_occurs == 0)
                  type = "std::optional<" + type + ">";

                fields.push_back(
                    {type,
                     resolver.field_name(term.name().local_name(), enc_type),
                     ""});
              } else if (deduped.size() == 1) {
                // Single unique CTA type — use it directly
                std::string type = deduped[0].cpp_type;
                if (eff_occurs.is_unbounded() || eff_occurs.max_occurs > 1)
                  type = "std::vector<" + type + ">";
                else if (eff_occurs.min_occurs == 0)
                  type = "std::optional<" + type + ">";
                fields.push_back(
                    {type,
                     resolver.field_name(term.name().local_name(), enc_type),
                     ""});
              } else {
                // No alternatives — use normal field type
                fields.push_back(
                    {field_type_for_element(term, eff_occurs, resolver,
                                            containing_type_name),
                     resolver.field_name(term.name().local_name(), enc_type),
                     default_value_for_element(term)});
              }
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
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
                  if (eff_occurs.is_unbounded() || eff_occurs.max_occurs > 1)
                    type = "std::vector<" + type + ">";
                  else if (eff_occurs.min_occurs == 0)
                    type = "std::optional<" + type + ">";

                  fields.push_back(
                      {type,
                       resolver.field_name(elem->name().local_name(), enc_type),
                       ""});
                  return;
                }
              }

              fields.push_back(
                  {field_type_for_element(*elem, eff_occurs, resolver,
                                          containing_type_name),
                   resolver.field_name(elem->name().local_name(), enc_type),
                   default_value_for_element(*elem)});
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def) {
                translate_particles(group_def->group().particles(),
                                    group_def->group().compositor(), fields,
                                    resolver, containing_type_name, eff_occurs,
                                    plan);
              }
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) {
                translate_particles(term->particles(), term->compositor(),
                                    fields, resolver, containing_type_name,
                                    eff_occurs, plan);
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
                        const qname& containing_type_name,
                        occurrence outer_occurs, field_plan* plan) {
      if (compositor == compositor_kind::choice) {
        std::string variant_type = "std::variant<";
        bool first = true;
        std::vector<choice_alternative> alts;

        // Build variant type string and collect choice alternatives for the
        // field plan in a single pass — these must stay synchronized.
        // Track seen C++ types to avoid duplicate variant alternatives
        // (e.g., xs:choice and xs:sequence both map to explicit_group).
        std::set<std::string> seen_variant_types;
        auto add_alt = [&](const std::string& type,
                           choice_alternative alt = {}) {
          if (seen_variant_types.insert(type).second) {
            // New type — add to variant
            if (!first) variant_type += ", ";
            variant_type += type;
            first = false;
          }
          // Always record in the plan (for element name dispatch)
          alt.cpp_type = type;
          alts.push_back(std::move(alt));
        };

        auto add_element_alt = [&](const qname& xml_name,
                                   const qname& type_name,
                                   const occurrence& occurs) {
          std::string type = resolver.resolve(type_name);
          bool cycle = resolver.is_cycle_type(type_name);
          if (cycle) type = "std::unique_ptr<" + type + ">";
          if (occurs.is_unbounded() || occurs.max_occurs > 1)
            type = "std::vector<" + type + ">";
          bool complex = is_complex_type(resolver.schemas, type_name);
          add_alt(
              type,
              {xml_name, type_name, type, complex, cycle, false, false, {}});
        };

        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, element_decl>) {
                  add_element_alt(term.name(), term.type_name(), p.occurs);
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (!elem) return;
                  if (elem->abstract()) {
                    auto members =
                        find_substitution_members(resolver.schemas, term.ref);
                    for (const auto* m : members) {
                      std::string type = resolver.resolve(m->type_name());
                      bool complex =
                          is_complex_type(resolver.schemas, m->type_name());
                      add_alt(type, {m->name(),
                                     m->type_name(),
                                     type,
                                     complex,
                                     false,
                                     true,
                                     false,
                                     {}});
                    }
                  } else {
                    add_element_alt(elem->name(), elem->type_name(), p.occurs);
                  }
                } else if constexpr (std::is_same_v<T, group_ref>) {
                  auto* group_def =
                      resolver.schemas.find_model_group_def(term.ref);
                  if (!group_def) return;
                  for (const auto& gp : group_def->group().particles()) {
                    std::visit(
                        [&](const auto& gt) {
                          using GT = std::decay_t<decltype(gt)>;
                          if constexpr (std::is_same_v<GT, element_decl>) {
                            add_element_alt(gt.name(), gt.type_name(),
                                            gp.occurs);
                          } else if constexpr (std::is_same_v<GT,
                                                              element_ref>) {
                            auto* elem = resolver.schemas.find_element(gt.ref);
                            if (!elem) return;
                            add_element_alt(elem->name(), elem->type_name(),
                                            gp.occurs);
                          }
                        },
                        gp.term);
                  }
                }
              },
              p.term);
        }

        variant_type += ">";
        if (!first) { // Only add choice field if there are alternatives
          // Apply outer cardinality from group ref
          field_cardinality card = field_cardinality::required;
          std::string field_type = variant_type;
          if (outer_occurs.is_unbounded() || outer_occurs.max_occurs > 1) {
            field_type = "std::vector<" + variant_type + ">";
            card = field_cardinality::repeating;
          } else if (outer_occurs.min_occurs == 0 &&
                     outer_occurs.max_occurs <= 1) {
            field_type = "std::optional<" + variant_type + ">";
            card = field_cardinality::optional;
          }

          // Ensure unique field name when multiple choice groups exist
          std::string name = "choice";
          int suffix = 2;
          while (
              std::any_of(fields.begin(), fields.end(),
                          [&](const cpp_field& f) { return f.name == name; }))
            name = "choice_" + std::to_string(suffix++);
          fields.push_back({field_type, name, ""});

          // Record in the field plan so read/write generators use the
          // same name and cardinality.
          if (plan) {
            field_plan_entry entry;
            entry.cpp_field_name = name;
            entry.cpp_type = field_type;
            entry.cardinality = card;
            entry.alternatives = std::move(alts);
            plan->push_back(std::move(entry));
          }
        }
        return;
      }

      for (const auto& p : particles)
        translate_particle_term(p, fields, resolver, containing_type_name, plan,
                                outer_occurs);
    }

    std::string
    default_value_for_attr(const attribute_use& attr,
                           const type_resolver& resolver) {
      if (attr.fixed_value.has_value()) {
        std::string cpp_type = resolver.resolve(attr.type_name);
        if (cpp_type == "std::string")
          return "\"" + attr.fixed_value.value() + "\"";
        if (is_safe_cpp_literal(attr.fixed_value.value()) &&
            is_numeric_cpp_type(cpp_type))
          return attr.fixed_value.value();
        if (cpp_type == "bool") {
          if (attr.fixed_value.value() == "true") return "true";
          if (attr.fixed_value.value() == "false") return "false";
        }
        return "";
      }
      if (attr.default_value.has_value()) {
        std::string cpp_type = resolver.resolve(attr.type_name);
        if (cpp_type == "std::string")
          return "\"" + attr.default_value.value() + "\"";
        if (is_safe_cpp_literal(attr.default_value.value()) &&
            is_numeric_cpp_type(cpp_type))
          return attr.default_value.value();
        if (cpp_type == "bool") {
          if (attr.default_value.value() == "true") return "true";
          if (attr.default_value.value() == "false") return "false";
        }
      }
      return "";
    }

    // Collect attributes inherited by a restriction-derived type.
    // Returns base attributes that aren't explicitly overridden or prohibited
    // by the restriction's own attribute list.
    std::vector<attribute_use>
    collect_restriction_inherited_attrs(const schema_set& schemas,
                                        const complex_type& ct) {
      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      if (!cc || cc->derivation != derivation_method::restriction) return {};
      if (cc->base_type_name.local_name().empty() &&
          cc->base_type_name.namespace_uri().empty())
        return {};

      std::set<std::string> existing;
      for (const auto& attr : ct.attributes())
        existing.insert(attr.name.local_name());

      std::vector<attribute_use> inherited;
      auto* base_ct = schemas.find_complex_type(cc->base_type_name);
      while (base_ct) {
        for (const auto& attr : base_ct->attributes()) {
          if (existing.count(attr.name.local_name())) continue;
          if (attr.type_name.local_name().empty() &&
              attr.type_name.namespace_uri().empty() && !attr.required)
            continue;
          inherited.push_back(attr);
          existing.insert(attr.name.local_name());
        }
        if (auto* bcc =
                std::get_if<complex_content>(&base_ct->content().detail)) {
          if (!bcc->base_type_name.local_name().empty())
            base_ct = schemas.find_complex_type(bcc->base_type_name);
          else
            base_ct = nullptr;
        } else {
          base_ct = nullptr;
        }
      }
      return inherited;
    }

    void
    translate_attributes(const std::vector<attribute_use>& attrs,
                         std::vector<cpp_field>& fields,
                         const type_resolver& resolver) {
      for (const auto& attr : attrs) {
        // Skip prohibited attributes (empty type, not required)
        if (attr.type_name.local_name().empty() &&
            attr.type_name.namespace_uri().empty() && !attr.required)
          continue;

        std::string base_type = resolver.resolve(attr.type_name);
        std::string name = resolver.field_name(attr.name.local_name());
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
                        const qname& containing_type_name,
                        field_plan* plan = nullptr) {
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
                                  containing_type_name, plan);
          }

          if (cc->content_model.has_value()) {
            translate_particles(cc->content_model->particles(),
                                cc->content_model->compositor(), fields,
                                resolver, containing_type_name, {}, plan);
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

    // Follow a simpleContent base type chain to find the ultimate simple
    // value type.  For example, CBC AmountType extends UDT AmountType which
    // restricts CCTS AmountType (complexType/simpleContent over xsd:decimal).
    // This function returns xsd:decimal — the leaf type that can be formatted
    // with xb::format() and parsed with xb::parse<>().
    qname
    resolve_simple_content_value_type(const schema_set& schemas,
                                      const qname& base_type_name) {
      auto* ct = schemas.find_complex_type(base_type_name);
      if (ct && ct->content().kind == content_kind::simple) {
        if (auto* sc = std::get_if<simple_content>(&ct->content().detail))
          return resolve_simple_content_value_type(schemas, sc->base_type_name);
      }
      return base_type_name;
    }

    // Collect attributes from a simpleContent base type chain.
    // For extension: inherit base attributes.  Called before translating
    // the current type's own attributes.
    void
    collect_simple_content_base_attrs(const schema_set& schemas,
                                      const qname& base_name,
                                      std::vector<cpp_field>& fields,
                                      const type_resolver& resolver) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;
      if (base_ct->content().kind != content_kind::simple) return;

      // Recurse into the base's base first
      if (auto* sc = std::get_if<simple_content>(&base_ct->content().detail)) {
        if (sc->derivation == derivation_method::extension)
          collect_simple_content_base_attrs(schemas, sc->base_type_name, fields,
                                            resolver);
      }

      translate_attributes(base_ct->attributes(), fields, resolver);
      translate_attribute_group_refs(base_ct->attribute_group_refs(), fields,
                                     resolver);
    }

    cpp_decl
    wrap_if_needed(cpp_struct s, const type_resolver& resolver) {
      if (resolver.options.encapsulation == encapsulation_mode::wrapped) {
        cpp_class cls;
        cls.name = s.name;
        cls.raw_struct_name = s.name + "_data";
        cls.fields = std::move(s.fields);
        cls.generate_equality = s.generate_equality;
        cls.generate_member_docs = resolver.options.generate_docs;
        cls.doc_comment = std::move(s.doc_comment);
        return cls;
      }
      return s;
    }

    cpp_decl
    translate_complex_type(const complex_type& ct,
                           const type_resolver& resolver,
                           const schema& current_schema,
                           field_plan* plan = nullptr) {
      cpp_struct s;
      s.name = resolver.type_name(ct.name().local_name());
      s.generate_equality = true;
      if (resolver.options.generate_docs && ct.doc_annotation().has_value())
        s.doc_comment = ct.doc_annotation()->documentation;

      // Disambiguate field names that shadow type names in the same namespace.
      // Appends '_' (same convention as C++ keyword escaping in naming.cpp).
      // After type-name disambiguation, resolve any remaining duplicate names
      // by appending additional '_' suffixes.
      auto disambiguate_fields = [&s, &resolver]() {
        if (resolver.schema_type_names) {
          for (auto& f : s.fields) {
            if (f.name != s.name && resolver.schema_type_names->count(f.name))
              f.name += '_';
          }
        }
        // Resolve remaining duplicates (e.g., element "revision" and attribute
        // "revision" both renamed to "revision_")
        std::set<std::string> seen;
        for (auto& f : s.fields) {
          while (seen.count(f.name))
            f.name += '_';
          seen.insert(f.name);
        }
      };

      // Compute effective open content
      auto eff_oc = effective_open_content(ct, current_schema);
      bool content_has_wildcard = false;
      if (auto* cc = std::get_if<complex_content>(&ct.content().detail))
        content_has_wildcard = has_wildcard_particle(*cc);
      bool needs_oc_field = eff_oc.has_value() && !content_has_wildcard;

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          // Resolve through the simpleContent chain to find the ultimate
          // simple value type (e.g., xsd:decimal for CBC AmountType ->
          // UDT AmountType -> CCTS AmountType -> xsd:decimal).
          qname value_qname = resolve_simple_content_value_type(
              resolver.schemas, sc->base_type_name);
          s.fields.push_back({resolver.resolve(value_qname), "value", ""});

          // For extensions, inherit base type attributes
          if (sc->derivation == derivation_method::extension)
            collect_simple_content_base_attrs(
                resolver.schemas, sc->base_type_name, s.fields, resolver);
        }

        translate_attributes(ct.attributes(), s.fields, resolver);
        translate_attribute_group_refs(ct.attribute_group_refs(), s.fields,
                                       resolver);
        if (ct.attribute_wildcard().has_value())
          s.fields.push_back(
              {"std::vector<xb::any_attribute>", "any_attribute", ""});
        disambiguate_fields();
        return wrap_if_needed(std::move(s), resolver);
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
        disambiguate_fields();
        return wrap_if_needed(std::move(s), resolver);
      }

      // Handle element_only content
      if (ct.content().kind == content_kind::element_only) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          // Extension: flatten base fields first
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            collect_base_fields(resolver.schemas, cc->base_type_name, s.fields,
                                resolver, ct.name(), plan);
          }

          if (cc->content_model.has_value()) {
            translate_particles(cc->content_model->particles(),
                                cc->content_model->compositor(), s.fields,
                                resolver, ct.name(), {}, plan);
          }
        }
      }

      // Translate attributes
      translate_attributes(ct.attributes(), s.fields, resolver);
      translate_attribute_group_refs(ct.attribute_group_refs(), s.fields,
                                     resolver);

      // For restriction-derived types, inherit base attributes that aren't
      // overridden or prohibited by the restriction.
      auto inherited =
          collect_restriction_inherited_attrs(resolver.schemas, ct);
      if (!inherited.empty())
        translate_attributes(inherited, s.fields, resolver);

      if (ct.attribute_wildcard().has_value())
        s.fields.push_back(
            {"std::vector<xb::any_attribute>", "any_attribute", ""});

      if (needs_oc_field)
        s.fields.push_back(
            {"std::vector<xb::any_element>", "open_content", ""});

      disambiguate_fields();

      // Record the wildcard field's final name in the plan (it may have
      // been renamed to "any_attribute_" by disambiguate_fields).
      if (plan && ct.attribute_wildcard().has_value()) {
        for (const auto& f : s.fields) {
          if (f.type == "std::vector<xb::any_attribute>") {
            field_plan_entry wc_entry;
            wc_entry.cpp_field_name = f.name;
            wc_entry.cpp_type = f.type;
            wc_entry.cardinality = field_cardinality::repeating;
            wc_entry.is_attribute = true; // mark as wildcard, not element
            plan->push_back(std::move(wc_entry));
            break;
          }
        }
      }
      return wrap_if_needed(std::move(s), resolver);
    }

    cpp_decl
    translate_simple_type(
        const simple_type& st, const type_resolver& resolver,
        std::map<std::string, std::string>& union_variant_map) {
      if (!st.facets().enumeration.empty()) {
        cpp_enum e;
        e.name = resolver.type_name(st.name().local_name());
        if (resolver.options.generate_docs && st.doc_annotation().has_value())
          e.doc_comment = st.doc_annotation()->documentation;
        for (const auto& val : st.facets().enumeration) {
          std::string name = resolver.enum_value_name(val);
          if (name.empty()) name = "empty_";
          e.values.push_back({name, val});
        }

        // Disambiguate case-only collisions (e.g. "A" and "a" both map
        // to "a"). Append "_upper" to the uppercase variant.
        std::unordered_map<std::string, std::size_t> id_count;
        for (const auto& v : e.values)
          id_count[v.name]++;

        for (auto& v : e.values) {
          if (id_count[v.name] > 1 && !v.xml_value.empty() &&
              v.xml_value[0] >= 'A' && v.xml_value[0] <= 'Z')
            v.name += "_upper";
        }

        return e;
      }

      if (st.variety() == simple_type_variety::list) {
        std::string item_type = "std::string";
        if (st.item_type_name().has_value())
          item_type = resolver.resolve(st.item_type_name().value());
        return cpp_type_alias{resolver.type_name(st.name().local_name()),
                              "std::vector<" + item_type + ">"};
      }

      if (st.variety() == simple_type_variety::union_type) {
        // If this union is a restriction of another union type (not an XSD
        // builtin), alias the base type's C++ name. This avoids generating
        // duplicate std::variant expansions and duplicate format() overloads.
        auto base = st.base_type_name();
        if (!base.namespace_uri().empty() &&
            base.namespace_uri() != "http://www.w3.org/2001/XMLSchema") {
          auto* base_st = resolver.schemas.find_simple_type(base);
          if (base_st &&
              base_st->variety() == simple_type_variety::union_type) {
            std::string base_cpp = resolver.resolve(base);
            return cpp_type_alias{resolver.type_name(st.name().local_name()),
                                  base_cpp};
          }
        }
        // Build variant type; deduplicate independent unions with identical
        // resolved member types by aliasing to the first one seen.
        std::string variant = "std::variant<";
        std::string variant_key;
        bool first = true;
        for (const auto& member : st.member_type_names()) {
          if (!first) {
            variant += ", ";
            variant_key += ",";
          }
          std::string cpp = resolver.resolve(member);
          variant += cpp;
          variant_key += cpp;
          first = false;
        }
        variant += ">";

        auto it = union_variant_map.find(variant_key);
        if (it != union_variant_map.end()) {
          // Alias to the first union with this variant signature
          return cpp_type_alias{resolver.type_name(st.name().local_name()),
                                it->second};
        }
        std::string name = resolver.type_name(st.name().local_name());
        union_variant_map[variant_key] = name;
        return cpp_type_alias{name, variant};
      }

      std::string base = resolver.resolve(st.base_type_name());
      return cpp_type_alias{resolver.type_name(st.name().local_name()), base};
    }

    // Generate a format() overload for a union simple type.
    // Placed in the generated namespace so unqualified format() calls
    // find it (from emit_simple_element_write and format_expr).
    std::optional<cpp_function>
    generate_format_function(const simple_type& st,
                             const type_resolver& resolver,
                             std::set<std::string>& seen_variant_types) {
      std::string name = resolver.type_name(st.name().local_name());

      // Union type: std::visit over member types.
      // Skip if this is a restriction of another union type (it's a type
      // alias, so the base type's format function already applies).
      if (st.variety() == simple_type_variety::union_type &&
          !st.member_type_names().empty()) {
        auto base = st.base_type_name();
        if (!base.namespace_uri().empty() &&
            base.namespace_uri() != "http://www.w3.org/2001/XMLSchema") {
          auto* base_st = resolver.schemas.find_simple_type(base);
          if (base_st && base_st->variety() == simple_type_variety::union_type)
            return std::nullopt;
        }

        // Build variant type string to check for duplicates: independent
        // union types with identical member types produce the same C++
        // std::variant, so only the first needs a format() overload.
        std::string variant_key;
        for (const auto& member : st.member_type_names()) {
          if (!variant_key.empty()) variant_key += ",";
          variant_key += resolver.resolve(member);
        }
        if (seen_variant_types.count(variant_key)) return std::nullopt;
        seen_variant_types.insert(variant_key);
        cpp_function fn;
        fn.return_type = "std::string";
        fn.name = "format";
        fn.parameters = "const " + name + "& v";

        std::string body;
        body += "  return std::visit([](const auto& x) -> std::string {\n";
        body += "    using V = std::decay_t<decltype(x)>;\n";
        bool first = true;
        for (const auto& member : st.member_type_names()) {
          std::string cpp_type = resolver.resolve(member);
          std::string kw = first ? "if" : "else if";
          body += "    " + kw + " constexpr (std::is_same_v<V, " + cpp_type +
                  ">) {\n";
          auto* member_st = resolver.schemas.find_simple_type(member);
          if (member_st && !member_st->facets().enumeration.empty()) {
            std::string fn = resolver.qualify_call("to_string", member);
            body += "      return std::string(" + fn + "(x));\n";
          } else if (cpp_type == "xb::qname") {
            body += "      return std::string(x.local_name());\n";
          } else if (cpp_type.find("std::vector<") == 0) {
            // List member type — TODO: proper list serialization
            body += "      return std::string{} /* TODO: list format */;\n";
          } else {
            body += "      return xb::format(x);\n";
          }
          body += "    }\n";
          first = false;
        }
        body += "    else { return std::string{}; }\n";
        body += "  }, v);\n";

        fn.body = body;
        return fn;
      }

      return std::nullopt;
    }

    // Generate a parse function for a union simple type.
    // Tries each member type in order; the first successful parse wins.
    // For enum members, uses from_string; for others, uses xb::parse<T>.
    std::optional<cpp_function>
    generate_parse_function(const simple_type& st,
                            const type_resolver& resolver,
                            std::set<std::string>& seen_variant_types) {
      std::string name = resolver.type_name(st.name().local_name());

      if (st.variety() == simple_type_variety::union_type &&
          !st.member_type_names().empty()) {
        // Skip restrictions of other union types (type alias)
        auto base = st.base_type_name();
        if (!base.namespace_uri().empty() &&
            base.namespace_uri() != "http://www.w3.org/2001/XMLSchema") {
          auto* base_st = resolver.schemas.find_simple_type(base);
          if (base_st && base_st->variety() == simple_type_variety::union_type)
            return std::nullopt;
        }

        // Deduplicate by variant type string
        std::string variant_key = "parse:";
        for (const auto& member : st.member_type_names()) {
          variant_key += resolver.resolve(member) + ",";
        }
        if (seen_variant_types.count(variant_key)) return std::nullopt;
        seen_variant_types.insert(variant_key);

        cpp_function fn;
        fn.return_type = name;
        fn.name = "parse_" + name;
        fn.parameters = "std::string_view text";

        std::string body;
        auto members = st.member_type_names();
        for (std::size_t i = 0; i < members.size(); ++i) {
          std::string cpp_type = resolver.resolve(members[i]);
          auto* member_st = resolver.schemas.find_simple_type(members[i]);
          bool is_last = (i == members.size() - 1);

          if (is_last) {
            // Last member: no try/catch, just return
            if (member_st && !member_st->facets().enumeration.empty()) {
              std::string qualified =
                  resolver.qualify_fn("", member_st->name());
              body += "  return " + qualified + "_from_string(text);\n";
            } else {
              body += "  return xb::parse<" + cpp_type + ">(text);\n";
            }
          } else {
            // Try this member type; on failure, try next
            body += "  try {\n";
            if (member_st && !member_st->facets().enumeration.empty()) {
              std::string qualified =
                  resolver.qualify_fn("", member_st->name());
              body += "    return " + qualified + "_from_string(text);\n";
            } else {
              body += "    return xb::parse<" + cpp_type + ">(text);\n";
            }
            body += "  } catch (...) {}\n";
          }
        }

        fn.body = body;
        return fn;
      }

      return std::nullopt;
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

    // Extract the last segment of a namespace URI, treating both '/' and ':'
    // as separators.  This handles URL-style namespaces
    // (http://example.com/order) and URN-style namespaces
    // (urn:oasis:...:Invoice-2) uniformly. Trailing '#' (fragment identifiers)
    // are stripped before extracting.
    std::string
    ns_stem(const std::string& ns) {
      std::string cleaned = ns;
      while (!cleaned.empty() && cleaned.back() == '#')
        cleaned.pop_back();

      auto last_slash = cleaned.rfind('/');
      auto last_colon = cleaned.rfind(':');
      auto last_sep = std::string::npos;
      if (last_slash != std::string::npos && last_colon != std::string::npos)
        last_sep = std::max(last_slash, last_colon);
      else if (last_slash != std::string::npos)
        last_sep = last_slash;
      else
        last_sep = last_colon;

      if (last_sep != std::string::npos)
        return to_snake_case(cleaned.substr(last_sep + 1));
      return to_snake_case(cleaned);
    }

    void
    add_cross_namespace_includes(std::set<std::string>& includes,
                                 const std::set<std::string>& referenced_ns,
                                 const std::vector<schema>& schemas,
                                 const std::string& header_suffix = ".hpp") {
      for (const auto& ref_ns : referenced_ns) {
        for (const auto& s : schemas) {
          if (s.target_namespace() == ref_ns) {
            std::string filename = ns_stem(ref_ns) + header_suffix;
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

    bool
    has_json_functions(const std::vector<cpp_decl>& declarations) {
      return std::any_of(declarations.begin(), declarations.end(),
                         [](const cpp_decl& d) {
                           auto* fn = std::get_if<cpp_function>(&d);
                           return fn && fn->parameters.find("nlohmann::json") !=
                                            std::string::npos;
                         });
    }

    bool
    has_regex_usage(const std::vector<cpp_decl>& declarations) {
      return std::any_of(
          declarations.begin(), declarations.end(), [](const cpp_decl& d) {
            auto* fn = std::get_if<cpp_function>(&d);
            return fn && fn->body.find("std::regex") != std::string::npos;
          });
    }

    std::vector<cpp_include>
    compute_includes(const std::set<std::string>& referenced_namespaces,
                     const std::vector<schema>& schemas,
                     const std::vector<cpp_decl>& declarations,
                     file_kind kind = file_kind::header,
                     const std::string& self_header = "",
                     const std::string& hdr_suffix = ".hpp") {
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
        if (has_regex_usage(declarations)) includes.insert("<regex>");
        if (has_json_functions(declarations)) {
          includes.insert("<nlohmann/json.hpp>");
          includes.insert("\"xb/json_value.hpp\"");
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
                } else if constexpr (std::is_same_v<T, cpp_class>) {
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

        if (has_regex_usage(declarations)) includes.insert("<regex>");

        // JSON includes when to_json/from_json functions are present
        if (has_json_functions(declarations)) {
          includes.insert("<nlohmann/json.hpp>");
          includes.insert("\"xb/json_value.hpp\"");
        }

        add_cross_namespace_includes(includes, referenced_namespaces, schemas,
                                     hdr_suffix);
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
      return ns_stem(target_ns);
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
            else if constexpr (std::is_same_v<T, cpp_class>)
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
        // Extract unqualified identifiers that could be local type names.
        // Skip namespace-qualified names (preceded by ::) since those refer
        // to types in other namespaces and aren't local ordering deps.
        std::string token;
        std::size_t token_start = 0;
        for (std::size_t i = 0; i < type_expr.size(); ++i) {
          char c = type_expr[i];
          if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            if (token.empty()) token_start = i;
            token += c;
          } else {
            if (!token.empty()) {
              bool qualified = token_start >= 2 &&
                               type_expr[token_start - 1] == ':' &&
                               type_expr[token_start - 2] == ':';
              if (!qualified) deps.insert(token);
            }
            token.clear();
          }
        }
        if (!token.empty()) {
          bool qualified = token_start >= 2 &&
                           type_expr[token_start - 1] == ':' &&
                           type_expr[token_start - 2] == ':';
          if (!qualified) deps.insert(token);
        }
      };

      std::visit(
          [&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_struct> ||
                          std::is_same_v<T, cpp_class>) {
              for (const auto& f : d.fields) {
                // Strip unique_ptr<...> segments so we don't depend on
                // forward-declared cycle types, but keep other type refs
                // (e.g. char_class_type in a variant alongside unique_ptrs).
                auto type_str = f.type;
                for (;;) {
                  auto pos = type_str.find("unique_ptr<");
                  if (pos == std::string::npos) break;
                  int depth = 1;
                  auto i = pos + 11; // past "unique_ptr<"
                  while (i < type_str.size() && depth > 0) {
                    if (type_str[i] == '<')
                      ++depth;
                    else if (type_str[i] == '>')
                      --depth;
                    ++i;
                  }
                  type_str.erase(pos, i - pos);
                }
                extract_type_refs(type_str);
              }
            } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
              extract_type_refs(d.target);
            }
          },
          decl);

      return deps;
    }

    // Collect type names referenced via unique_ptr in a declaration.
    // These are the types that need forward declarations (not full includes)
    // in file-per-type mode.
    std::set<std::string>
    unique_ptr_dependencies(const cpp_decl& decl) {
      std::set<std::string> deps;

      auto extract_uptr_refs = [&](const std::string& type_expr) {
        auto str = type_expr;
        for (;;) {
          auto pos = str.find("unique_ptr<");
          if (pos == std::string::npos) break;
          auto start = pos + 11; // past "unique_ptr<"
          int depth = 1;
          auto i = start;
          while (i < str.size() && depth > 0) {
            if (str[i] == '<')
              ++depth;
            else if (str[i] == '>')
              --depth;
            ++i;
          }
          // The content between start and i-1 is the unique_ptr argument
          std::string inner = str.substr(start, i - 1 - start);
          // Extract the bare type name (strip whitespace, qualifiers)
          std::string token;
          for (char c : inner) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
              token += c;
            else if (!token.empty())
              break;
          }
          if (!token.empty()) deps.insert(token);
          str.erase(pos, i - pos);
        }
      };

      std::visit(
          [&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_struct> ||
                          std::is_same_v<T, cpp_class>) {
              for (const auto& f : d.fields)
                extract_uptr_refs(f.type);
            }
          },
          decl);

      return deps;
    }

    // Topological sort of declarations based on type dependencies.
    // Since unique_ptr fields are excluded from dependency extraction
    // (they only need forward declarations), the dependency graph is
    // acyclic after cycle-breaking. Cycle types get forward declarations
    // at the top, then all types are emitted in topological order.
    std::vector<cpp_decl>
    order_declarations(std::vector<cpp_decl> decls,
                       const std::set<std::string>& cycle_type_names = {}) {
      if (decls.size() <= 1) return decls;

      // Build name -> index map
      std::unordered_map<std::string, std::size_t> name_to_idx;
      for (std::size_t i = 0; i < decls.size(); ++i)
        name_to_idx[decl_name(decls[i])] = i;

      // Identify which indices are cycle types
      std::set<std::size_t> cycle_indices;
      for (std::size_t i = 0; i < decls.size(); ++i) {
        if (cycle_type_names.count(decl_name(decls[i])))
          cycle_indices.insert(i);
      }

      // Build adjacency list (hard dependency edges only — unique_ptr
      // fields are already excluded by decl_dependencies)
      std::vector<std::set<std::size_t>> deps(decls.size());
      for (std::size_t i = 0; i < decls.size(); ++i) {
        for (const auto& dep_name : decl_dependencies(decls[i])) {
          auto it = name_to_idx.find(dep_name);
          if (it != name_to_idx.end() && it->second != i)
            deps[i].insert(it->second);
        }
      }

      // Kahn's topological sort on all declarations
      std::vector<std::size_t> all_nodes;
      all_nodes.reserve(decls.size());
      for (std::size_t i = 0; i < decls.size(); ++i)
        all_nodes.push_back(i);

      std::set<std::size_t> node_set(all_nodes.begin(), all_nodes.end());
      std::unordered_map<std::size_t, std::size_t> in_deg;
      std::unordered_map<std::size_t, std::vector<std::size_t>> rev;

      for (auto i : all_nodes) {
        in_deg[i] = 0;
        for (auto dep : deps[i]) {
          if (node_set.count(dep)) {
            in_deg[i]++;
            rev[dep].push_back(i);
          }
        }
      }

      std::vector<std::size_t> sorted;
      std::vector<std::size_t> q;
      for (auto i : all_nodes) {
        if (in_deg[i] == 0) q.push_back(i);
      }

      while (!q.empty()) {
        auto idx = q.front();
        q.erase(q.begin());
        sorted.push_back(idx);
        for (auto dependent : rev[idx]) {
          if (--in_deg[dependent] == 0) q.push_back(dependent);
        }
      }

      // Collect any remaining types not reached by Kahn's (hard-dep
      // cycles, e.g. party_type <-> power_of_attorney_type via vector).
      std::set<std::size_t> sorted_set(sorted.begin(), sorted.end());
      std::vector<std::size_t> remaining;
      for (auto i : all_nodes) {
        if (!sorted_set.count(i)) remaining.push_back(i);
      }

      // For remaining types, find the actual cycle members using Tarjan's
      // SCC, then re-sort with cycle-to-cycle edges removed.
      std::set<std::size_t> hard_cycle_members;
      if (!remaining.empty()) {
        // Build subgraph for remaining types
        std::set<std::size_t> rem_set(remaining.begin(), remaining.end());

        // Tarjan's SCC on remaining subgraph
        std::unordered_map<std::size_t, int> idx_arr, ll;
        std::unordered_map<std::size_t, bool> on_stk;
        std::vector<std::size_t> stk;
        int cur_idx = 0;

        std::function<void(std::size_t)> scc_visit = [&](std::size_t v) {
          idx_arr[v] = ll[v] = cur_idx++;
          stk.push_back(v);
          on_stk[v] = true;
          for (auto w : deps[v]) {
            if (!rem_set.count(w)) continue;
            if (idx_arr.find(w) == idx_arr.end()) {
              scc_visit(w);
              ll[v] = std::min(ll[v], ll[w]);
            } else if (on_stk[w]) {
              ll[v] = std::min(ll[v], idx_arr[w]);
            }
          }
          if (ll[v] == idx_arr[v]) {
            std::vector<std::size_t> scc;
            std::size_t w;
            do {
              w = stk.back();
              stk.pop_back();
              on_stk[w] = false;
              scc.push_back(w);
            } while (w != v);
            if (scc.size() > 1) {
              for (auto x : scc)
                hard_cycle_members.insert(x);
            } else if (deps[scc[0]].count(scc[0]) && rem_set.count(scc[0])) {
              hard_cycle_members.insert(scc[0]);
            }
          }
        };

        for (auto i : remaining) {
          if (idx_arr.find(i) == idx_arr.end()) scc_visit(i);
        }

        // Re-sort remaining types with cycle-to-cycle edges removed
        std::unordered_map<std::size_t, std::size_t> rem_in_deg;
        std::unordered_map<std::size_t, std::vector<std::size_t>> rem_rev;
        for (auto i : remaining) {
          rem_in_deg[i] = 0;
          for (auto dep : deps[i]) {
            if (rem_set.count(dep)) {
              // Skip edges between hard cycle members
              if (hard_cycle_members.count(i) && hard_cycle_members.count(dep))
                continue;
              rem_in_deg[i]++;
              rem_rev[dep].push_back(i);
            }
          }
        }

        std::vector<std::size_t> rem_sorted;
        std::vector<std::size_t> rem_q;
        for (auto i : remaining) {
          if (rem_in_deg[i] == 0) rem_q.push_back(i);
        }
        while (!rem_q.empty()) {
          auto x = rem_q.front();
          rem_q.erase(rem_q.begin());
          rem_sorted.push_back(x);
          for (auto dependent : rem_rev[x]) {
            if (--rem_in_deg[dependent] == 0) rem_q.push_back(dependent);
          }
        }
        // Append any still-remaining (deep cascading cycles)
        for (auto i : remaining) {
          if (std::find(rem_sorted.begin(), rem_sorted.end(), i) ==
              rem_sorted.end())
            rem_sorted.push_back(i);
        }
        remaining = std::move(rem_sorted);
      }

      // Build result
      std::vector<cpp_decl> result;
      result.reserve(decls.size() + cycle_indices.size() +
                     hard_cycle_members.size());

      auto fwd_is_class = [&](std::size_t idx) {
        if (std::holds_alternative<cpp_class>(decls[idx])) return true;
        if (auto* fwd = std::get_if<cpp_forward_decl>(&decls[idx]))
          return fwd->is_class;
        return false;
      };

      // Forward declare XSD-identified cycle types
      for (auto idx : cycle_indices) {
        auto n = decl_name(decls[idx]);
        if (!n.empty())
          result.push_back(cpp_forward_decl{n, fwd_is_class(idx)});
      }

      // Forward declare hard-dep cycle types (not already forward-declared)
      for (auto idx : hard_cycle_members) {
        if (!cycle_indices.count(idx)) {
          auto n = decl_name(decls[idx]);
          if (!n.empty())
            result.push_back(cpp_forward_decl{n, fwd_is_class(idx)});
        }
      }

      // Emit Kahn-sorted types, then re-sorted remaining types
      for (auto idx : sorted)
        result.push_back(std::move(decls[idx]));
      for (auto idx : remaining)
        result.push_back(std::move(decls[idx]));

      return result;
    }

    // ===== Serialization code generation =====

    // Determine if a type name resolves to an enum, following restriction
    // chains (e.g. CommType_t → CommType_enum_t → has enumerations).
    bool
    is_enum_type(const schema_set& schemas, const qname& type_name) {
      auto* st = schemas.find_simple_type(type_name);
      while (st) {
        if (!st->facets().enumeration.empty()) return true;
        auto base = st->base_type_name();
        if (base.namespace_uri() == "http://www.w3.org/2001/XMLSchema") break;
        st = schemas.find_simple_type(base);
      }
      return false;
    }

    // Determine if a type name resolves to a union type, following
    // restriction chains.
    bool
    is_union_type(const schema_set& schemas, const qname& type_name) {
      auto* st = schemas.find_simple_type(type_name);
      while (st) {
        if (st->variety() == simple_type_variety::union_type) return true;
        auto base = st->base_type_name();
        if (base.namespace_uri() == "http://www.w3.org/2001/XMLSchema") break;
        st = schemas.find_simple_type(base);
      }
      return false;
    }

    // Determine if a type name resolves to a complex type
    bool
    is_complex_type(const schema_set& schemas, const qname& type_name) {
      return schemas.find_complex_type(type_name) != nullptr;
    }

    // Generate the format expression for a value, considering type.
    // Enums use to_string(), union types use format(), built-in types use
    // xb::format().  Cross-namespace calls are explicitly qualified.
    std::string
    format_expr(const std::string& value_expr, const qname& type_name,
                const schema_set& schemas, const type_resolver& resolver) {
      if (is_enum_type(schemas, type_name)) {
        std::string fn = resolver.qualify_call("to_string", type_name);
        return "std::string(" + fn + "(" + value_expr + "))";
      }
      if (is_union_type(schemas, type_name)) {
        std::string fn = resolver.qualify_call("format", type_name);
        return fn + "(" + value_expr + ")";
      }
      // QName attributes need special serialization (prefix:local).
      // For now, emit local_name() — correct for unqualified names
      // and the common case where the target namespace is defaulted.
      std::string cpp_type = resolver.resolve(type_name);
      if (cpp_type == "xb::qname")
        return "std::string((" + value_expr + ").local_name())";
      // List and vector types: generate space-separated serialization.
      // TODO: generate proper format() overloads for list types.
      if (cpp_type.find("std::vector<") == 0)
        return "std::string{} /* TODO: list serialization */";
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

    // Emit inline write code for a simple-typed element value.
    // Uses to_string() for enums, format() for unions, and xb::format()
    // for built-in types.  Cross-namespace calls are explicitly qualified.
    void
    emit_simple_element_write(std::string& body, const std::string& indent,
                              const std::string& qn,
                              const std::string& val_expr,
                              const qname& type_name, const schema_set& schemas,
                              const type_resolver& resolver) {
      body += indent + "writer.start_element(" + qn + ");\n";
      if (is_enum_type(schemas, type_name)) {
        std::string fn = resolver.qualify_call("to_string", type_name);
        body += indent + "writer.characters(std::string(" + fn + "(" +
                val_expr + ")));\n";
      } else if (is_union_type(schemas, type_name)) {
        std::string fn = resolver.qualify_call("format", type_name);
        body += indent + "writer.characters(" + fn + "(" + val_expr + "));\n";
      } else
        body += indent + "writer.characters(xb::format(" + val_expr + "));\n";
      body += indent + "writer.end_element();\n";
    }

    void
    emit_write_element(std::string& body, const write_element_info& info,
                       const schema_set& schemas,
                       const type_resolver& resolver) {
      std::string qn = "xb::qname{\"" + info.element_name.namespace_uri() +
                       "\", \"" + info.element_name.local_name() + "\"}";
      std::string field = resolver.field_access("value", info.field_name);

      bool is_complex = is_complex_type(schemas, info.type_name);

      if (info.is_recursive && !info.occurs.is_unbounded() &&
          info.occurs.max_occurs <= 1) {
        // unique_ptr field (cycle-breaking)
        std::string write_fn = resolver.qualify_fn("write_", info.type_name);
        body += "  if (" + field + ") {\n";
        body += "    writer.start_element(" + qn + ");\n";
        body += "    " + write_fn + "(*" + field + ", writer);\n";
        body += "    writer.end_element();\n";
        body += "  }\n";
        return;
      }

      if (info.occurs.is_unbounded() || info.occurs.max_occurs > 1) {
        // vector field
        body += "  " +
                resolver.sequence_for_begin("value", info.field_name, "item");
        if (is_complex) {
          std::string write_fn = resolver.qualify_fn("write_", info.type_name);
          std::string val = info.is_recursive ? "*item" : "item";
          body += "    writer.start_element(" + qn + ");\n";
          body += "    " + write_fn + "(" + val + ", writer);\n";
          body += "    writer.end_element();\n";
        } else {
          emit_simple_element_write(body, "    ", qn, "item", info.type_name,
                                    schemas, resolver);
        }
        body += "  }\n";
        return;
      }

      if (info.occurs.min_occurs == 0) {
        // optional field
        body += "  if (" + field + ") {\n";
        if (is_complex) {
          std::string write_fn = resolver.qualify_fn("write_", info.type_name);
          body += "    writer.start_element(" + qn + ");\n";
          body += "    " + write_fn + "(*" + field + ", writer);\n";
          body += "    writer.end_element();\n";
        } else {
          emit_simple_element_write(body, "    ", qn, "*" + field,
                                    info.type_name, schemas, resolver);
        }
        body += "  }\n";
        return;
      }

      // Required field
      if (is_complex) {
        std::string write_fn = resolver.qualify_fn("write_", info.type_name);
        body += "  writer.start_element(" + qn + ");\n";
        body += "  " + write_fn + "(" + field + ", writer);\n";
        body += "  writer.end_element();\n";
      } else {
        emit_simple_element_write(body, "  ", qn, field, info.type_name,
                                  schemas, resolver);
      }
    }

    // Forward declare
    void
    emit_write_particles(
        std::string& body, const std::vector<particle>& particles,
        compositor_kind compositor, const type_resolver& resolver,
        const qname& containing_type_name, occurrence outer_occurs = {},
        const field_plan* plan = nullptr, int choice_index = 0);

    void
    emit_write_particle_term(std::string& body, const particle& p,
                             const type_resolver& resolver,
                             const qname& containing_type_name,
                             const field_plan* plan = nullptr,
                             int* choice_index_ptr = nullptr) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
              auto deduped =
                  deduplicate_alternatives(term.type_alternatives(), resolver);
              if (deduped.size() > 1) {
                // CTA: std::visit dispatch over variant alternatives
                std::string fname =
                    resolver.field_name(term.name().local_name(), enc_type);
                std::string field = resolver.field_access("value", fname);
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
                          resolver.qualify_fn("write_", alt.type_name);
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
                  body += "  " +
                          resolver.sequence_for_begin("value", fname, "item");
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
                emit_write_element(
                    body,
                    {resolver.field_name(term.name().local_name(), enc_type),
                     term.name(), deduped[0].type_name, p.occurs,
                     term.nillable(), false},
                    resolver.schemas, resolver);
              } else {
                bool is_recursive =
                    (term.type_name() == containing_type_name) ||
                    resolver.is_cycle_type(term.type_name());
                emit_write_element(
                    body,
                    {resolver.field_name(term.name().local_name(), enc_type),
                     term.name(), term.type_name(), p.occurs, term.nillable(),
                     is_recursive},
                    resolver.schemas, resolver);
              }
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
              auto* elem = resolver.schemas.find_element(term.ref);
              if (!elem) return;

              // Abstract element -> substitution group variant
              if (elem->abstract()) {
                auto members =
                    find_substitution_members(resolver.schemas, term.ref);
                if (!members.empty()) {
                  std::string fname =
                      resolver.field_name(elem->name().local_name(), enc_type);
                  std::string field = resolver.field_access("value", fname);
                  auto emit_visit = [&](const std::string& val_expr) {
                    body += "  std::visit([&](const auto& v) {\n";
                    body += "    using V = std::decay_t<decltype(v)>;\n";
                    bool first = true;
                    for (const auto* m : members) {
                      std::string cpp_type = resolver.resolve(m->type_name());
                      std::string kw = first ? "if" : "else if";
                      body += "    " + kw + " constexpr (std::is_same_v<V, " +
                              cpp_type + ">) {\n";
                      std::string mqn = "xb::qname{\"" +
                                        m->name().namespace_uri() + "\", \"" +
                                        m->name().local_name() + "\"}";
                      body += "      writer.start_element(" + mqn + ");\n";
                      std::string write_fn =
                          resolver.qualify_fn("write_", m->type_name());
                      body += "      " + write_fn + "(v, writer);\n";
                      body += "      writer.end_element();\n";
                      body += "    }\n";
                      first = false;
                    }
                    body += "  }, " + val_expr + ");\n";
                  };

                  if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1) {
                    body += "  " +
                            resolver.sequence_for_begin("value", fname, "item");
                    emit_visit("item");
                    body += "  }\n";
                  } else if (p.occurs.min_occurs == 0) {
                    body += "  if (" + field + ") {\n";
                    emit_visit("*" + field);
                    body += "  }\n";
                  } else {
                    emit_visit(field);
                  }
                  return;
                }
              }

              emit_write_element(
                  body,
                  {resolver.field_name(elem->name().local_name(), enc_type),
                   elem->name(), elem->type_name(), p.occurs, elem->nillable(),
                   resolver.is_cycle_type(elem->type_name())},
                  resolver.schemas, resolver);
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def) {
                bool is_choice =
                    group_def->group().compositor() == compositor_kind::choice;
                // Only count this choice if the type generator created a
                // field for it (has a plan entry).  Choices with no variant
                // alternatives produce no field and no plan entry.
                bool plan_has_entry = true;
                if (plan && is_choice && choice_index_ptr) {
                  int ci = *choice_index_ptr;
                  plan_has_entry = false;
                  int pci = 0;
                  for (const auto& e : *plan) {
                    if (!e.alternatives.empty()) {
                      if (pci == ci) {
                        plan_has_entry = true;
                        break;
                      }
                      ++pci;
                    }
                  }
                }
                if (plan_has_entry) {
                  int ci = choice_index_ptr ? *choice_index_ptr : 0;
                  emit_write_particles(body, group_def->group().particles(),
                                       group_def->group().compositor(),
                                       resolver, containing_type_name, p.occurs,
                                       plan, ci);
                  if (choice_index_ptr && is_choice) ++(*choice_index_ptr);
                }
              }
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) {
                if (term->compositor() == compositor_kind::choice) {
                  // Only emit and count this choice if it has element
                  // particles that produce variant alternatives.
                  bool has_elems = false;
                  body += "  // INLINE_CHOICE_CHECK\n";
                  for (const auto& sp : term->particles()) {
                    if (std::holds_alternative<element_decl>(sp.term) ||
                        std::holds_alternative<element_ref>(sp.term))
                      has_elems = true;
                  }
                  if (has_elems) {
                    int ci = choice_index_ptr ? *choice_index_ptr : 0;
                    emit_write_particles(
                        body, term->particles(), term->compositor(), resolver,
                        containing_type_name, p.occurs, plan, ci);
                    if (choice_index_ptr) ++(*choice_index_ptr);
                  }
                } else {
                  emit_write_particles(
                      body, term->particles(), term->compositor(), resolver,
                      containing_type_name, p.occurs, plan,
                      choice_index_ptr ? *choice_index_ptr : 0);
                }
              }
            } else if constexpr (std::is_same_v<T, wildcard>) {
              body += "  " + resolver.sequence_for_begin("value", "any", "e");
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
                         const qname& containing_type_name,
                         occurrence outer_occurs, const field_plan* plan,
                         int choice_index) {
      if (compositor == compositor_kind::choice) {
        // Use field plan for name and cardinality if available.
        std::string choice_field_name = "choice";
        bool is_repeating =
            outer_occurs.is_unbounded() || outer_occurs.max_occurs > 1;
        bool is_optional = !is_repeating && outer_occurs.min_occurs == 0 &&
                           outer_occurs.max_occurs <= 1;
        bool found_in_plan = false;
        if (plan) {
          int ci = 0;
          for (const auto& entry : *plan) {
            if (!entry.alternatives.empty()) {
              if (ci == choice_index) {
                choice_field_name = entry.cpp_field_name;
                is_repeating =
                    entry.cardinality == field_cardinality::repeating;
                is_optional = entry.cardinality == field_cardinality::optional;
                found_in_plan = true;
                break;
              }
              ++ci;
            }
          }
          // If the plan exists but has no entry for this choice index,
          // the type generator didn't create a field — skip entirely.
          if (!found_in_plan) return;
        }
        std::string field = resolver.field_access("value", choice_field_name);

        std::string visit_target = is_repeating  ? "choice_item"
                                   : is_optional ? ("*" + field)
                                                 : field;

        // Generate visit body directly into body.  After the loop, check
        // if any if-constexpr branches were generated (first == false).
        // If not, remove the visit preamble.
        auto body_before = body.size();
        if (is_repeating) {
          body += "  " + resolver.sequence_for_begin("value", choice_field_name,
                                                     "choice_item");
        } else if (is_optional) {
          body += "  if (" + field + ") {\n";
        }
        body += "  std::visit([&](const auto& v) {\n";
        body += "    using T = std::decay_t<decltype(v)>;\n";

        bool first = true;
        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using TermT = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<TermT, element_decl>) {
                  std::string cpp_type = resolver.resolve(term.type_name());
                  std::string match_type = cpp_type;
                  bool is_cycle = resolver.is_cycle_type(term.type_name());
                  if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1)
                    match_type = "std::vector<" + cpp_type + ">";
                  else if (is_cycle)
                    match_type = "std::unique_ptr<" + cpp_type + ">";
                  std::string qn = "xb::qname{\"" +
                                   term.name().namespace_uri() + "\", \"" +
                                   term.name().local_name() + "\"}";
                  std::string kw = first ? "if" : "else if";
                  body += "    " + kw + " constexpr (std::is_same_v<T, " +
                          match_type + ">) {\n";
                  bool is_complex =
                      is_complex_type(resolver.schemas, term.type_name());
                  std::string val = is_cycle ? "*v" : "v";
                  if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1) {
                    // Vector alternative: iterate and write each
                    body += "      for (const auto& item : v) {\n";
                    if (is_complex) {
                      std::string write_fn =
                          resolver.qualify_fn("write_", term.type_name());
                      body += "        writer.start_element(" + qn + ");\n";
                      body += "        " + write_fn + "(item, writer);\n";
                      body += "        writer.end_element();\n";
                    } else {
                      emit_simple_element_write(body, "        ", qn, "item",
                                                term.type_name(),
                                                resolver.schemas, resolver);
                    }
                    body += "      }\n";
                  } else if (is_complex) {
                    std::string write_fn =
                        resolver.qualify_fn("write_", term.type_name());
                    body += "      writer.start_element(" + qn + ");\n";
                    body += "      " + write_fn + "(" + val + ", writer);\n";
                    body += "      writer.end_element();\n";
                  } else {
                    emit_simple_element_write(body, "      ", qn, val,
                                              term.type_name(),
                                              resolver.schemas, resolver);
                  }
                  body += "    }\n";
                  first = false;
                } else if constexpr (std::is_same_v<TermT, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (!elem) return;
                  // Abstract element -> expand substitution group members
                  if (elem->abstract()) {
                    auto members =
                        find_substitution_members(resolver.schemas, term.ref);
                    for (const auto* m : members) {
                      std::string cpp_type = resolver.resolve(m->type_name());
                      std::string mqn = "xb::qname{\"" +
                                        m->name().namespace_uri() + "\", \"" +
                                        m->name().local_name() + "\"}";
                      std::string kw = first ? "if" : "else if";
                      body += "    " + kw + " constexpr (std::is_same_v<T, " +
                              cpp_type + ">) {\n";
                      std::string write_fn =
                          resolver.qualify_fn("write_", m->type_name());
                      body += "      writer.start_element(" + mqn + ");\n";
                      body += "      " + write_fn + "(v, writer);\n";
                      body += "      writer.end_element();\n";
                      body += "    }\n";
                      first = false;
                    }
                  } else {
                    std::string cpp_type = resolver.resolve(elem->type_name());
                    bool is_cycle = resolver.is_cycle_type(elem->type_name());
                    std::string match_type =
                        is_cycle ? "std::unique_ptr<" + cpp_type + ">"
                                 : cpp_type;
                    std::string val = is_cycle ? "*v" : "v";
                    std::string qn = "xb::qname{\"" +
                                     elem->name().namespace_uri() + "\", \"" +
                                     elem->name().local_name() + "\"}";
                    std::string kw = first ? "if" : "else if";
                    body += "    " + kw + " constexpr (std::is_same_v<T, " +
                            match_type + ">) {\n";
                    if (is_complex_type(resolver.schemas, elem->type_name())) {
                      std::string write_fn =
                          resolver.qualify_fn("write_", elem->type_name());
                      body += "      writer.start_element(" + qn + ");\n";
                      body += "      " + write_fn + "(" + val + ", writer);\n";
                      body += "      writer.end_element();\n";
                    } else {
                      emit_simple_element_write(body, "      ", qn, val,
                                                elem->type_name(),
                                                resolver.schemas, resolver);
                    }
                    body += "    }\n";
                    first = false;
                  }
                }
              },
              p.term);
        }

        if (first) {
          // No branches generated — roll back the preamble
          body.resize(body_before);
        } else {
          body += "  }, " + visit_target + ");\n";
          if (is_repeating || is_optional) { body += "  }\n"; }
        }
        return;
      }

      // Sequence, all, or interleave: write each particle in order
      int seq_choice_idx = choice_index;
      for (const auto& p : particles)
        emit_write_particle_term(body, p, resolver, containing_type_name, plan,
                                 &seq_choice_idx);
    }

    // Collect field names occupied by element particles in a complex type.
    // Used to prevent attribute field names from colliding with element fields.
    std::set<std::string>
    collect_element_field_names(const complex_type& ct,
                                const type_resolver& resolver) {
      std::set<std::string> names;
      auto enc_type = resolver.type_name(ct.name().local_name());

      if (ct.mixed()) { names.insert("content"); }

      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      if (!cc || !cc->content_model.has_value()) return names;

      std::function<void(const std::vector<particle>&)> scan;
      scan = [&](const std::vector<particle>& particles) {
        for (const auto& p : particles) {
          std::visit(
              [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, element_decl>) {
                  names.insert(
                      resolver.field_name(term.name().local_name(), enc_type));
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (elem)
                    names.insert(resolver.field_name(elem->name().local_name(),
                                                     enc_type));
                } else if constexpr (std::is_same_v<T, group_ref>) {
                  auto* g = resolver.schemas.find_model_group_def(term.ref);
                  if (g) scan(g->group().particles());
                } else if constexpr (std::is_same_v<
                                         T, std::unique_ptr<model_group>>) {
                  if (term) scan(term->particles());
                }
              },
              p.term);
        }
      };
      scan(cc->content_model->particles());
      return names;
    }

    void
    emit_write_attributes(std::string& body,
                          const std::vector<attribute_use>& attrs,
                          const schema_set& schemas,
                          const type_resolver& resolver,
                          std::set<std::string> occupied = {}) {
      for (const auto& attr : attrs) {
        if (attr.type_name.local_name().empty() &&
            attr.type_name.namespace_uri().empty() && !attr.required)
          continue;
        std::string name = resolver.field_name(attr.name.local_name());
        while (occupied.count(name))
          name += '_';
        occupied.insert(name);
        std::string qn = "xb::qname{\"" + attr.name.namespace_uri() + "\", \"" +
                         attr.name.local_name() + "\"}";
        std::string field = resolver.field_access("value", name);
        std::string fmt_expr =
            format_expr(field, attr.type_name, schemas, resolver);

        if (attr.required) {
          body += "  writer.attribute(" + qn + ", " + fmt_expr + ");\n";
        } else {
          body += "  if (" + field + ") {\n";
          std::string opt_fmt =
              format_expr("*" + field, attr.type_name, schemas, resolver);
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
                           const qname& containing_type_name,
                           const field_plan* plan = nullptr,
                           int* choice_index_ptr = nullptr) {
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
                                     resolver, containing_type_name, plan,
                                     choice_index_ptr);
          }
          if (cc->content_model.has_value()) {
            int ci = choice_index_ptr ? *choice_index_ptr : 0;
            emit_write_particles(body, cc->content_model->particles(),
                                 cc->content_model->compositor(), resolver,
                                 containing_type_name, {}, plan, ci);
            // Count choice groups in this content model
            if (choice_index_ptr && plan) {
              for (const auto& p : cc->content_model->particles()) {
                std::visit(
                    [&](const auto& t) {
                      using PT = std::decay_t<decltype(t)>;
                      if constexpr (std::is_same_v<PT, group_ref>) {
                        auto* gd = schemas.find_model_group_def(t.ref);
                        if (gd &&
                            gd->group().compositor() == compositor_kind::choice)
                          ++(*choice_index_ptr);
                      } else if constexpr (std::is_same_v<
                                               PT,
                                               std::unique_ptr<model_group>>) {
                        if (t && t->compositor() == compositor_kind::choice)
                          ++(*choice_index_ptr);
                      }
                    },
                    p.term);
              }
            }
          }
        }
      }

      emit_write_attributes(body, base_ct->attributes(), schemas, resolver);
      emit_write_attribute_group_refs(body, base_ct->attribute_group_refs(),
                                      schemas, resolver);
    }

    // Emit attribute writes from a simpleContent base type chain.
    void
    emit_write_simple_content_base_attrs(std::string& body,
                                         const schema_set& schemas,
                                         const qname& base_name,
                                         const type_resolver& resolver) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;
      if (base_ct->content().kind != content_kind::simple) return;

      if (auto* sc = std::get_if<simple_content>(&base_ct->content().detail)) {
        if (sc->derivation == derivation_method::extension)
          emit_write_simple_content_base_attrs(body, schemas,
                                               sc->base_type_name, resolver);
      }

      emit_write_attributes(body, base_ct->attributes(), schemas, resolver);
      emit_write_attribute_group_refs(body, base_ct->attribute_group_refs(),
                                      schemas, resolver);
    }

    cpp_function
    generate_write_function(const complex_type& ct,
                            const type_resolver& resolver,
                            const schema& current_schema,
                            [[maybe_unused]] const field_plan* plan = nullptr) {
      cpp_function fn;
      std::string struct_name = resolver.type_name(ct.name().local_name());
      fn.return_type = "void";
      fn.name = "write_" + struct_name;
      fn.parameters =
          "const " + struct_name + "& value, xb::xml_writer& writer";

      std::string body;

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          // For extensions, write base type attributes first
          if (sc->derivation == derivation_method::extension)
            emit_write_simple_content_base_attrs(body, resolver.schemas,
                                                 sc->base_type_name, resolver);
        }
        emit_write_attributes(body, ct.attributes(), resolver.schemas, resolver,
                              collect_element_field_names(ct, resolver));
        emit_write_attribute_group_refs(body, ct.attribute_group_refs(),
                                        resolver.schemas, resolver);

        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          // Resolve through the chain to get the ultimate simple type
          qname value_qname = resolve_simple_content_value_type(
              resolver.schemas, sc->base_type_name);
          std::string fmt =
              format_expr(resolver.field_access("value", "value"), value_qname,
                          resolver.schemas, resolver);
          body += "  writer.characters(" + fmt + ");\n";
        }

        fn.body = body;
        return fn;
      }

      // Handle attributes first
      auto occupied = collect_element_field_names(ct, resolver);
      emit_write_attributes(body, ct.attributes(), resolver.schemas, resolver,
                            occupied);
      emit_write_attribute_group_refs(body, ct.attribute_group_refs(),
                                      resolver.schemas, resolver);

      // For restrictions, write inherited base attributes
      {
        auto inherited =
            collect_restriction_inherited_attrs(resolver.schemas, ct);
        if (!inherited.empty())
          emit_write_attributes(body, inherited, resolver.schemas, resolver);
      }

      if (ct.attribute_wildcard().has_value()) {
        // Use the plan to find the wildcard field's actual name (may
        // have been renamed by disambiguate_fields).
        std::string wc_field = "any_attribute";
        if (plan) {
          for (const auto& entry : *plan) {
            if (entry.cpp_type == "std::vector<xb::any_attribute>") {
              wc_field = entry.cpp_field_name;
              break;
            }
          }
        }
        body += "  for (const auto& a : " +
                resolver.field_access("value", wc_field) + ") {\n";
        body += "    writer.attribute(a.name(), a.value());\n";
        body += "  }\n";
      }

      // Handle mixed content
      if (ct.mixed() && (ct.content().kind == content_kind::mixed ||
                         ct.content().kind == content_kind::element_only)) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          if (cc->content_model.has_value()) {
            body += "  for (const auto& item : " +
                    resolver.field_access("value", "content") + ") {\n";
            body += "    std::visit([&](const auto& v) {\n";
            body += "      using T = std::decay_t<decltype(v)>;\n";
            body += "      if constexpr (std::is_same_v<T, std::string>) {\n";
            body += "        writer.characters(v);\n";
            body += "      }\n";
            for (const auto& p : cc->content_model->particles()) {
              std::visit(
                  [&](const auto& term) {
                    using TermT = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<TermT, element_decl>) {
                      std::string cpp_type = resolver.resolve(term.type_name());
                      std::string elem_qn =
                          "xb::qname{\"" + term.name().namespace_uri() +
                          "\", \"" + term.name().local_name() + "\"}";
                      body += "      else if constexpr (std::is_same_v<T, " +
                              cpp_type + ">) {\n";
                      body +=
                          "        writer.start_element(" + elem_qn + ");\n";
                      if (is_complex_type(resolver.schemas, term.type_name())) {
                        std::string write_fn =
                            resolver.qualify_fn("write_", term.type_name());
                        body += "        " + write_fn + "(v, writer);\n";
                      } else {
                        body += "        writer.characters(xb::format(v));\n";
                      }
                      body += "        writer.end_element();\n";
                      body += "      }\n";
                    }
                  },
                  p.term);
            }
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
          int wr_choice_idx = 0;
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            emit_write_base_fields(body, resolver.schemas, cc->base_type_name,
                                   resolver, ct.name(), plan, &wr_choice_idx);
          }

          if (cc->content_model.has_value()) {
            emit_write_particles(body, cc->content_model->particles(),
                                 cc->content_model->compositor(), resolver,
                                 ct.name(), {}, plan, wr_choice_idx);
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
          body += "  for (const auto& e : " +
                  resolver.field_access("value", "open_content") + ") {\n";
          body += "    e.write(writer);\n";
          body += "  }\n";
        }
      }

      if (body.empty())
        fn.parameters = "[[maybe_unused]] const " + struct_name +
                        "& value, [[maybe_unused]] xb::xml_writer& writer";

      fn.body = body;
      return fn;
    }

    // ===== Deserialization code generation =====

    // Generate the parse expression for an attribute value
    std::string
    parse_expr(const std::string& text_expr, const qname& type_name,
               const schema_set& schemas, const type_resolver& resolver) {
      if (is_enum_type(schemas, type_name)) {
        // Follow restriction chain to find the actual enum type name,
        // since from_string is generated for the enum, not its aliases.
        auto* st = schemas.find_simple_type(type_name);
        while (st && st->facets().enumeration.empty())
          st = schemas.find_simple_type(st->base_type_name());
        qname actual_name = st ? st->name() : type_name;
        return resolver.qualify_fn("", actual_name) + "_from_string(" +
               text_expr + ")";
      }
      if (is_union_type(schemas, type_name)) {
        // Find the canonical parse function name for this union type.
        // Independent unions with identical resolved member types share a
        // single parse function (named after the first one seen).
        auto* st = schemas.find_simple_type(type_name);
        // Follow restriction chain to find a type with member types
        while (st && st->member_type_names().empty()) {
          auto base = st->base_type_name();
          if (base.namespace_uri() == "http://www.w3.org/2001/XMLSchema") break;
          st = schemas.find_simple_type(base);
        }
        std::string union_name;
        if (st && !st->member_type_names().empty() &&
            resolver.union_variant_map) {
          // Build variant key to find canonical name
          std::string variant_key;
          bool first = true;
          for (const auto& member : st->member_type_names()) {
            if (!first) variant_key += ",";
            variant_key += resolver.resolve(member);
            first = false;
          }
          auto it = resolver.union_variant_map->find(variant_key);
          if (it != resolver.union_variant_map->end()) union_name = it->second;
        }
        if (union_name.empty()) {
          qname actual_name = st ? st->name() : type_name;
          return resolver.qualify_fn("parse_", actual_name) + "(" + text_expr +
                 ")";
        }
        return "parse_" + union_name + "(" + text_expr + ")";
      }
      std::string cpp_type = resolver.resolve(type_name);
      if (cpp_type == "xb::qname")
        return "xb::parse_qname(" + text_expr + ", reader)";
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

      if (info.is_recursive && !info.occurs.is_unbounded() &&
          info.occurs.max_occurs <= 1) {
        // unique_ptr field (cycle-breaking)
        std::string read_fn = resolver.qualify_fn("read_", info.type_name);
        return "      " + field + " = std::make_unique<" + cpp_type + ">(" +
               read_fn + "(reader));\n";
      }

      if (info.occurs.is_unbounded() || info.occurs.max_occurs > 1) {
        // vector field -> push_back
        if (is_complex) {
          std::string read_fn = resolver.qualify_fn("read_", info.type_name);
          if (info.is_recursive)
            return "      " + field + ".push_back(std::make_unique<" +
                   cpp_type + ">(" + read_fn + "(reader)));\n";
          return "      " + field + ".push_back(" + read_fn + "(reader));\n";
        }
        return "      " + field + ".push_back(xb::read_simple<" + cpp_type +
               ">(reader));\n";
      }

      // Required or optional (both assign directly — optional::operator= works)
      if (is_complex) {
        std::string read_fn = resolver.qualify_fn("read_", info.type_name);
        return "      " + field + " = " + read_fn + "(reader);\n";
      }
      return "      " + field + " = xb::read_simple<" + cpp_type +
             ">(reader);\n";
    }

    // Forward declare
    void
    emit_read_particles(
        std::string& body, const std::vector<particle>& particles,
        compositor_kind compositor, const type_resolver& resolver,
        const qname& containing_type_name, bool has_open_content = false,
        occurrence outer_occurs = {}, const field_plan* plan = nullptr,
        int choice_index = 0, bool* outer_first_branch = nullptr);

    void
    emit_read_particle_match(std::string& body, const particle& p,
                             const type_resolver& resolver,
                             const qname& containing_type_name,
                             bool& first_branch,
                             const field_plan* plan = nullptr,
                             int* choice_index_ptr = nullptr) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
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
                      "result." +
                      resolver.field_name(term.name().local_name(), enc_type);

                  auto emit_read_alt = [&](const type_alternative& alt) {
                    bool is_complex =
                        is_complex_type(resolver.schemas, alt.type_name);
                    if (is_complex) {
                      std::string read_fn =
                          resolver.qualify_fn("read_", alt.type_name);
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
                      {resolver.field_name(term.name().local_name(), enc_type),
                       term.name(), deduped[0].type_name, p.occurs,
                       term.nillable(), false},
                      resolver.schemas, resolver);
                } else {
                  // No alternatives — normal read path
                  bool is_recursive =
                      (term.type_name() == containing_type_name) ||
                      resolver.is_cycle_type(term.type_name());
                  body += emit_read_element(
                      {resolver.field_name(term.name().local_name(), enc_type),
                       term.name(), term.type_name(), p.occurs, term.nillable(),
                       is_recursive},
                      resolver.schemas, resolver);
                }
              }

              body += "    }\n";
              first_branch = false;
            } else if constexpr (std::is_same_v<T, element_ref>) {
              auto enc_type =
                  resolver.type_name(containing_type_name.local_name());
              auto* elem = resolver.schemas.find_element(term.ref);
              if (!elem) return;

              // Abstract element -> match substitution group member names
              if (elem->abstract()) {
                auto members =
                    find_substitution_members(resolver.schemas, term.ref);
                if (!members.empty()) {
                  std::string field =
                      "result." +
                      resolver.field_name(elem->name().local_name(), enc_type);
                  for (const auto* m : members) {
                    std::string mqn = "xb::qname{\"" +
                                      m->name().namespace_uri() + "\", \"" +
                                      m->name().local_name() + "\"}";
                    std::string kw = first_branch ? "if" : "else if";
                    body += "    " + kw + " (name == " + mqn + ") {\n";
                    std::string read_fn =
                        resolver.qualify_fn("read_", m->type_name());
                    if (p.occurs.is_unbounded() || p.occurs.max_occurs > 1) {
                      body += "      " + field + ".push_back(" + read_fn +
                              "(reader));\n";
                    } else if (p.occurs.min_occurs == 0) {
                      body +=
                          "      " + field + " = " + read_fn + "(reader);\n";
                    } else {
                      body +=
                          "      " + field + " = " + read_fn + "(reader);\n";
                    }
                    body += "    }\n";
                    first_branch = false;
                  }
                  return;
                }
              }

              std::string qn = "xb::qname{\"" + elem->name().namespace_uri() +
                               "\", \"" + elem->name().local_name() + "\"}";
              std::string kw = first_branch ? "if" : "else if";
              body += "    " + kw + " (name == " + qn + ") {\n";
              body += emit_read_element(
                  {resolver.field_name(elem->name().local_name(), enc_type),
                   elem->name(), elem->type_name(), p.occurs, elem->nillable(),
                   resolver.is_cycle_type(elem->type_name())},
                  resolver.schemas, resolver);
              body += "    }\n";
              first_branch = false;
            } else if constexpr (std::is_same_v<T, group_ref>) {
              auto* group_def = resolver.schemas.find_model_group_def(term.ref);
              if (group_def) {
                bool grp_is_choice =
                    group_def->group().compositor() == compositor_kind::choice;

                if (grp_is_choice) {
                  // Delegate choice reading to emit_read_particles which
                  // correctly handles cardinality via the field plan.
                  int ci = choice_index_ptr ? *choice_index_ptr : 0;
                  emit_read_particles(body, group_def->group().particles(),
                                      compositor_kind::choice, resolver,
                                      containing_type_name, false, p.occurs,
                                      plan, ci, &first_branch);
                  if (choice_index_ptr) ++(*choice_index_ptr);
                } else {
                  // Non-choice group: expand particles inline
                  for (const auto& gp : group_def->group().particles())
                    emit_read_particle_match(body, gp, resolver,
                                             containing_type_name, first_branch,
                                             plan, choice_index_ptr);
                }
              }
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) {
                bool inline_is_choice =
                    term->compositor() == compositor_kind::choice;

                if (inline_is_choice) {
                  // Delegate to emit_read_particles with the choice
                  // compositor and the correct outer_occurs from this
                  // particle.  Track which choice field index this is
                  // so the plan lookup finds the right field name.
                  int ci = choice_index_ptr ? *choice_index_ptr : 0;
                  emit_read_particles(body, term->particles(),
                                      compositor_kind::choice, resolver,
                                      containing_type_name, false, p.occurs,
                                      plan, ci, &first_branch);
                  if (choice_index_ptr) ++(*choice_index_ptr);
                } else {
                  for (const auto& sp : term->particles())
                    emit_read_particle_match(body, sp, resolver,
                                             containing_type_name, first_branch,
                                             plan, choice_index_ptr);
                }
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
                        bool has_open_content, occurrence outer_occurs,
                        const field_plan* plan, int choice_index,
                        bool* outer_first_branch) {
      if (compositor == compositor_kind::choice) {
        // Use field plan for name and cardinality if available —
        // this is the single source of truth computed by translate_particles.
        std::string choice_field = "choice";
        bool outer_repeating =
            outer_occurs.is_unbounded() || outer_occurs.max_occurs > 1;
        if (plan) {
          int ci = 0;
          for (const auto& entry : *plan) {
            if (!entry.alternatives.empty()) {
              if (ci == choice_index) {
                choice_field = entry.cpp_field_name;
                outer_repeating =
                    entry.cardinality == field_cardinality::repeating;
                break;
              }
              ++ci;
            }
          }
        }

        // Helper: emit assignment or push_back depending on outer cardinality
        auto emit_choice_assign = [&](const std::string& read_expr) {
          if (outer_repeating)
            body += "      result." + choice_field + ".push_back(" + read_expr +
                    ");\n";
          else
            body += "      result." + choice_field + " = " + read_expr + ";\n";
        };

        // Choice: element name selects variant alternative.
        // When called from a sequence context, outer_first_branch carries
        // the chain state so we generate else-if continuations instead of
        // starting a new if chain.
        bool local_first = true;
        bool& first_branch =
            outer_first_branch ? *outer_first_branch : local_first;
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
                  bool is_repeating =
                      p.occurs.is_unbounded() || p.occurs.max_occurs > 1;

                  std::string kw = first_branch ? "if" : "else if";
                  body += "    " + kw + " (name == " + qn + ") {\n";
                  if (is_repeating && !outer_repeating) {
                    // Vector alternative: push_back each occurrence
                    std::string vec_type = "std::vector<" + cpp_type + ">";
                    std::string read_expr;
                    if (is_complex)
                      read_expr =
                          resolver.qualify_fn("read_", term.type_name()) +
                          "(reader)";
                    else
                      read_expr = "xb::read_simple<" + cpp_type + ">(reader)";
                    body += "      if (auto* vec = std::get_if<" + vec_type +
                            ">(&result.choice)) {\n";
                    body += "        vec->push_back(" + read_expr + ");\n";
                    body += "      } else {\n";
                    body += "        result.choice = " + vec_type + "{" +
                            read_expr + "};\n";
                    body += "      }\n";
                  } else if (is_complex) {
                    std::string read_fn =
                        resolver.qualify_fn("read_", term.type_name());
                    if (resolver.is_cycle_type(term.type_name()))
                      emit_choice_assign("std::make_unique<" + cpp_type + ">(" +
                                         read_fn + "(reader))");
                    else
                      emit_choice_assign(read_fn + "(reader)");
                  } else {
                    emit_choice_assign("xb::read_simple<" + cpp_type +
                                       ">(reader)");
                  }
                  body += "    }\n";
                  first_branch = false;
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  auto* elem = resolver.schemas.find_element(term.ref);
                  if (!elem) return;
                  // Abstract element -> match substitution group members
                  if (elem->abstract()) {
                    auto members =
                        find_substitution_members(resolver.schemas, term.ref);
                    for (const auto* m : members) {
                      std::string mqn = "xb::qname{\"" +
                                        m->name().namespace_uri() + "\", \"" +
                                        m->name().local_name() + "\"}";
                      std::string kw = first_branch ? "if" : "else if";
                      body += "    " + kw + " (name == " + mqn + ") {\n";
                      std::string read_fn =
                          resolver.qualify_fn("read_", m->type_name());
                      if (resolver.is_cycle_type(m->type_name())) {
                        std::string cpp_type = resolver.resolve(m->type_name());
                        emit_choice_assign("std::make_unique<" + cpp_type +
                                           ">(" + read_fn + "(reader))");
                      } else {
                        emit_choice_assign(read_fn + "(reader)");
                      }
                      body += "    }\n";
                      first_branch = false;
                    }
                  } else {
                    std::string qn = "xb::qname{\"" +
                                     elem->name().namespace_uri() + "\", \"" +
                                     elem->name().local_name() + "\"}";
                    std::string cpp_type = resolver.resolve(elem->type_name());
                    bool is_complex =
                        is_complex_type(resolver.schemas, elem->type_name());
                    std::string kw = first_branch ? "if" : "else if";
                    body += "    " + kw + " (name == " + qn + ") {\n";
                    if (is_complex) {
                      std::string read_fn =
                          resolver.qualify_fn("read_", elem->type_name());
                      if (resolver.is_cycle_type(elem->type_name()))
                        emit_choice_assign("std::make_unique<" + cpp_type +
                                           ">(" + read_fn + "(reader))");
                      else
                        emit_choice_assign(read_fn + "(reader)");
                    } else {
                      emit_choice_assign("xb::read_simple<" + cpp_type +
                                         ">(reader)");
                    }
                    body += "    }\n";
                    first_branch = false;
                  }
                } else if constexpr (std::is_same_v<T, group_ref>) {
                  // Group ref inside a choice: expand the group's elements
                  // into this choice's dispatch chain.
                  auto* group_def =
                      resolver.schemas.find_model_group_def(term.ref);
                  if (group_def && group_def->group().compositor() ==
                                       compositor_kind::choice) {
                    // Recursively emit choice dispatch for the group's
                    // particles
                    for (const auto& gp : group_def->group().particles()) {
                      std::visit(
                          [&](const auto& gt) {
                            using GT = std::decay_t<decltype(gt)>;
                            if constexpr (std::is_same_v<GT, element_decl>) {
                              std::string gqn =
                                  "xb::qname{\"" + gt.name().namespace_uri() +
                                  "\", \"" + gt.name().local_name() + "\"}";
                              std::string kw = first_branch ? "if" : "else if";
                              body +=
                                  "    " + kw + " (name == " + gqn + ") {\n";
                              std::string cpp_type =
                                  resolver.resolve(gt.type_name());
                              bool gc = is_complex_type(resolver.schemas,
                                                        gt.type_name());
                              if (gc) {
                                std::string rfn = resolver.qualify_fn(
                                    "read_", gt.type_name());
                                if (resolver.is_cycle_type(gt.type_name()))
                                  emit_choice_assign("std::make_unique<" +
                                                     cpp_type + ">(" + rfn +
                                                     "(reader))");
                                else
                                  emit_choice_assign(rfn + "(reader)");
                              } else {
                                emit_choice_assign("xb::read_simple<" +
                                                   cpp_type + ">(reader)");
                              }
                              body += "    }\n";
                              first_branch = false;
                            } else if constexpr (std::is_same_v<GT,
                                                                element_ref>) {
                              auto* elem =
                                  resolver.schemas.find_element(gt.ref);
                              if (!elem) return;
                              std::string gqn =
                                  "xb::qname{\"" +
                                  elem->name().namespace_uri() + "\", \"" +
                                  elem->name().local_name() + "\"}";
                              std::string kw = first_branch ? "if" : "else if";
                              body +=
                                  "    " + kw + " (name == " + gqn + ") {\n";
                              std::string rfn = resolver.qualify_fn(
                                  "read_", elem->type_name());
                              if (resolver.is_cycle_type(elem->type_name())) {
                                std::string cpp_type =
                                    resolver.resolve(elem->type_name());
                                emit_choice_assign("std::make_unique<" +
                                                   cpp_type + ">(" + rfn +
                                                   "(reader))");
                              } else {
                                emit_choice_assign(rfn + "(reader)");
                              }
                              body += "    }\n";
                              first_branch = false;
                            }
                          },
                          gp.term);
                    }
                  }
                }
              },
              p.term);
        }
        return;
      }

      // Sequence, all, or interleave: dispatch by element name
      bool first_branch = true;
      int seq_choice_idx = choice_index;
      for (const auto& p : particles)
        emit_read_particle_match(body, p, resolver, containing_type_name,
                                 first_branch, plan, &seq_choice_idx);

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
                         const type_resolver& resolver,
                         std::set<std::string> occupied = {}) {
      for (const auto& attr : attrs) {
        if (attr.type_name.local_name().empty() &&
            attr.type_name.namespace_uri().empty() && !attr.required)
          continue;
        std::string name = resolver.field_name(attr.name.local_name());
        while (occupied.count(name))
          name += '_';
        occupied.insert(name);
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
                          const qname& containing_type_name, bool& first_branch,
                          const field_plan* plan = nullptr,
                          int* choice_index_ptr = nullptr) {
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
                                    containing_type_name, first_branch, plan,
                                    choice_index_ptr);
          }
          if (cc->content_model.has_value()) {
            for (const auto& p : cc->content_model->particles())
              emit_read_particle_match(body, p, resolver, containing_type_name,
                                       first_branch, plan, choice_index_ptr);
          }
        }
      }
    }

    // Emit attribute reads from a simpleContent base type chain.
    void
    emit_read_simple_content_base_attrs(std::string& body,
                                        const schema_set& schemas,
                                        const qname& base_name,
                                        const type_resolver& resolver) {
      auto* base_ct = schemas.find_complex_type(base_name);
      if (!base_ct) return;
      if (base_ct->content().kind != content_kind::simple) return;

      if (auto* sc = std::get_if<simple_content>(&base_ct->content().detail)) {
        if (sc->derivation == derivation_method::extension)
          emit_read_simple_content_base_attrs(body, schemas, sc->base_type_name,
                                              resolver);
      }

      emit_read_attributes(body, base_ct->attributes(), schemas, resolver);
      emit_read_attribute_group_refs(body, base_ct->attribute_group_refs(),
                                     schemas, resolver);
    }

    cpp_function
    generate_read_function(const complex_type& ct,
                           const type_resolver& resolver,
                           const schema& current_schema,
                           [[maybe_unused]] const field_plan* plan = nullptr) {
      cpp_function fn;
      std::string struct_name = resolver.type_name(ct.name().local_name());
      fn.return_type = struct_name;
      fn.name = "read_" + struct_name;
      fn.parameters = "xb::xml_reader& reader";

      std::string body;
      body += "  " + struct_name + " result;\n";

      // Handle simpleContent
      if (ct.content().kind == content_kind::simple) {
        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          // For extensions, read base type attributes first
          if (sc->derivation == derivation_method::extension)
            emit_read_simple_content_base_attrs(body, resolver.schemas,
                                                sc->base_type_name, resolver);
        }
        emit_read_attributes(body, ct.attributes(), resolver.schemas, resolver,
                             collect_element_field_names(ct, resolver));
        emit_read_attribute_group_refs(body, ct.attribute_group_refs(),
                                       resolver.schemas, resolver);

        if (auto* sc = std::get_if<simple_content>(&ct.content().detail)) {
          // Resolve through the chain to get the ultimate simple type
          qname value_qname = resolve_simple_content_value_type(
              resolver.schemas, sc->base_type_name);
          std::string cpp_type = resolver.resolve(value_qname);
          body += "  result.value = xb::parse<" + cpp_type +
                  ">(xb::read_text(reader));\n";
        }

        body += "  return result;\n";
        fn.body = body;
        return fn;
      }

      // Read attributes from current start_element
      auto occupied_read = collect_element_field_names(ct, resolver);
      emit_read_attributes(body, ct.attributes(), resolver.schemas, resolver,
                           occupied_read);
      emit_read_attribute_group_refs(body, ct.attribute_group_refs(),
                                     resolver.schemas, resolver);

      // For restrictions, read inherited base attributes
      {
        auto inherited =
            collect_restriction_inherited_attrs(resolver.schemas, ct);
        if (!inherited.empty())
          emit_read_attributes(body, inherited, resolver.schemas, resolver);
      }

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

      // Mixed content: read text nodes and element nodes into content variant
      if (ct.mixed() && (ct.content().kind == content_kind::mixed ||
                         ct.content().kind == content_kind::element_only)) {
        if (auto* cc = std::get_if<complex_content>(&ct.content().detail)) {
          if (cc->content_model.has_value() &&
              !cc->content_model->particles().empty()) {
            body += "  auto start_depth = reader.depth();\n";
            body += "  while (reader.read()) {\n";
            body += "    if (reader.node_type() == "
                    "xb::xml_node_type::end_element "
                    "&& reader.depth() == start_depth) break;\n";
            body += "    if (reader.node_type() == "
                    "xb::xml_node_type::characters) {\n";
            body += "      result.content.emplace_back("
                    "std::string(reader.text()));\n";
            body += "      continue;\n";
            body += "    }\n";
            body += "    if (reader.node_type() != "
                    "xb::xml_node_type::start_element) continue;\n";
            // Only declare name variable if there are element particles
            bool has_elem_particles = false;
            for (const auto& p : cc->content_model->particles()) {
              if (std::holds_alternative<element_decl>(p.term)) {
                has_elem_particles = true;
                break;
              }
            }
            if (has_elem_particles) body += "    auto& name = reader.name();\n";
            bool first = true;
            for (const auto& p : cc->content_model->particles()) {
              std::visit(
                  [&](const auto& term) {
                    using TermT = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<TermT, element_decl>) {
                      std::string kw = first ? "if" : "else if";
                      std::string elem_qn =
                          "xb::qname{\"" + term.name().namespace_uri() +
                          "\", \"" + term.name().local_name() + "\"}";
                      body += "    " + kw + " (name == " + elem_qn + ") {\n";
                      if (is_complex_type(resolver.schemas, term.type_name())) {
                        std::string read_fn =
                            resolver.qualify_fn("read_", term.type_name());
                        body += "      result.content.emplace_back(" + read_fn +
                                "(reader));\n";
                      } else {
                        std::string cpp_type =
                            resolver.resolve(term.type_name());
                        body += "      result.content.emplace_back("
                                "xb::read_simple_content<" +
                                cpp_type + ">(reader));\n";
                      }
                      body += "    }\n";
                      first = false;
                    }
                  },
                  p.term);
            }
            if (!first) { body += "    else { xb::skip_element(reader); }\n"; }
            body += "  }\n";
            body += "  return result;\n";
            fn.body = body;
            return fn;
          }
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
          int type_choice_idx = 0;
          if (cc->derivation == derivation_method::extension &&
              (!cc->base_type_name.namespace_uri().empty() ||
               !cc->base_type_name.local_name().empty())) {
            emit_read_base_fields(body, resolver.schemas, cc->base_type_name,
                                  resolver, ct.name(), first_branch, plan,
                                  &type_choice_idx);
          }

          if (cc->content_model.has_value()) {
            emit_read_particles(body, cc->content_model->particles(),
                                cc->content_model->compositor(), resolver,
                                ct.name(), has_oc, {}, plan, type_choice_idx,
                                &first_branch);
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

      if (body.find("reader") == std::string::npos)
        fn.parameters = "[[maybe_unused]] xb::xml_reader& reader";

      fn.body = body;
      return fn;
    }

    // Returns true if the facet_set has any constraining facets that we
    // generate validation checks for (excludes enumeration, which is
    // type-safe via enum codegen, and total_digits/fraction_digits which
    // are deferred).
    bool
    has_constraining_facets(const facet_set& f) {
      return f.min_inclusive.has_value() || f.max_inclusive.has_value() ||
             f.min_exclusive.has_value() || f.max_exclusive.has_value() ||
             f.length.has_value() || f.min_length.has_value() ||
             f.max_length.has_value() || f.pattern.has_value();
    }

    // Generate C++ boolean expressions for facet checks.
    // value_expr: the C++ expression for the value being validated
    // cpp_type: the resolved C++ type name (for xb::parse<T>)
    std::vector<std::string>
    generate_facet_checks(const facet_set& facets,
                          const std::string& value_expr,
                          const std::string& cpp_type) {
      std::vector<std::string> checks;

      if (facets.min_inclusive.has_value())
        checks.push_back(value_expr + " >= xb::parse<" + cpp_type + ">(\"" +
                         facets.min_inclusive.value() + "\")");

      if (facets.max_inclusive.has_value())
        checks.push_back(value_expr + " <= xb::parse<" + cpp_type + ">(\"" +
                         facets.max_inclusive.value() + "\")");

      if (facets.min_exclusive.has_value())
        checks.push_back(value_expr + " > xb::parse<" + cpp_type + ">(\"" +
                         facets.min_exclusive.value() + "\")");

      if (facets.max_exclusive.has_value())
        checks.push_back(value_expr + " < xb::parse<" + cpp_type + ">(\"" +
                         facets.max_exclusive.value() + "\")");

      // Length facets: use value.size() for std::string,
      // xb::format(value).size() otherwise (length applies to lexical
      // representation per XSD spec)
      bool is_string = (cpp_type == "std::string");
      bool is_vector = (cpp_type.find("std::vector<") == 0);
      auto size_expr = [&]() -> std::string {
        if (is_string) return value_expr + ".size()";
        if (is_vector) return value_expr + ".size()";
        return "xb::format(" + value_expr + ").size()";
      };

      if (facets.length.has_value())
        checks.push_back(size_expr() +
                         " == " + std::to_string(facets.length.value()));

      if (facets.min_length.has_value())
        checks.push_back(size_expr() +
                         " >= " + std::to_string(facets.min_length.value()));

      if (facets.max_length.has_value())
        checks.push_back(size_expr() +
                         " <= " + std::to_string(facets.max_length.value()));

      if (facets.pattern.has_value()) {
        std::string format_expr =
            is_string ? value_expr : "xb::format(" + value_expr + ")";
        // Escape backslashes for C++ string literal (XSD patterns use \d,
        // \s etc. which must become \\d, \\s in C++)
        std::string pat;
        for (char c : facets.pattern.value()) {
          if (c == '\\')
            pat += "\\\\";
          else if (c == '"')
            pat += "\\\"";
          else
            pat += c;
        }
        checks.push_back("std::regex_match(" + format_expr +
                         ", std::regex(\"^" + pat + "$\"))");
      }

      return checks;
    }

    // Collect cardinality validation checks for particles in a content model.
    // Returns boolean expressions checking size constraints on vector/optional
    // fields. Skips default (1,1) and unconstrained (0,unbounded) cardinality.
    // Choice groups are skipped — their cardinality is encoded in the variant
    // type, not as separate fields.
    std::vector<std::string>
    collect_cardinality_checks(const model_group& mg,
                               const std::string& struct_prefix,
                               const type_resolver* resolver = nullptr,
                               const std::string& enclosing_type = {}) {
      std::vector<std::string> checks;

      // Choice groups map to a single variant field; individual particle
      // cardinality is embedded in the variant alternative types.
      if (mg.compositor() == compositor_kind::choice) return checks;

      bool wrapped = resolver && resolver->is_wrapped();

      for (const auto& p : mg.particles()) {
        std::visit(
            [&](const auto& term) {
              using T = std::decay_t<decltype(term)>;
              if constexpr (std::is_same_v<T, element_decl>) {
                std::string fname =
                    resolver ? resolver->field_name(term.name().local_name(),
                                                    enclosing_type)
                             : to_cpp_identifier(term.name().local_name());
                std::string field =
                    struct_prefix + fname + (wrapped ? "()" : "");
                bool is_collection =
                    p.occurs.is_unbounded() || p.occurs.max_occurs > 1;
                bool is_optional =
                    p.occurs.min_occurs == 0 && p.occurs.max_occurs == 1;

                if (is_collection) {
                  // vector field: check min/max size
                  std::string size_expr =
                      wrapped ? struct_prefix + fname + "_size()"
                              : field + ".size()";
                  if (p.occurs.min_occurs > 0)
                    checks.push_back(size_expr + " >= " +
                                     std::to_string(p.occurs.min_occurs));
                  if (!p.occurs.is_unbounded())
                    checks.push_back(size_expr + " <= " +
                                     std::to_string(p.occurs.max_occurs));
                } else if (is_optional && p.occurs.min_occurs == 1) {
                  // optional with min=1: must have value (shouldn't occur
                  // in practice, but handle defensively)
                  checks.push_back(field + ".has_value()");
                }
                // Required (1,1): no check needed — always present by
                // construction. Unconstrained (0,unbounded): no check needed.
              }
            },
            p.term);
      }

      return checks;
    }

    // Returns true if any particle has non-trivial cardinality that requires
    // validation (i.e., not default (1,1) and not unconstrained (0,unbounded)).
    bool
    has_cardinality_constraints(const model_group& mg) {
      return !collect_cardinality_checks(mg, "").empty();
    }

    // Generate a validate_<type>() function for a complex type with assertions,
    // simple content facets, or cardinality constraints.
    // Returns nullopt if none are present.
    std::optional<cpp_function>
    generate_validate_function(const complex_type& ct,
                               const type_resolver& resolver) {
      // Check for simple content facets
      bool has_sc_facets = false;
      const simple_content* sc = nullptr;
      if (auto* sc_ptr = std::get_if<simple_content>(&ct.content().detail)) {
        sc = sc_ptr;
        has_sc_facets = has_constraining_facets(sc_ptr->facets);
      }

      // Check for cardinality constraints in element_only content.
      // Mixed content types store elements in a variant vector, so individual
      // element cardinality checks don't apply.
      bool has_card = false;
      const complex_content* cc = nullptr;
      if (!ct.mixed()) {
        if (auto* cc_ptr = std::get_if<complex_content>(&ct.content().detail)) {
          cc = cc_ptr;
          if (cc_ptr->content_model.has_value())
            has_card =
                has_cardinality_constraints(cc_ptr->content_model.value());
        }
      }

      if (ct.assertions().empty() && !has_sc_facets && !has_card)
        return std::nullopt;

      std::string struct_name = resolver.type_name(ct.name().local_name());
      xpath_context ctx{"value.", resolver.is_wrapped()};

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
          fn.parameters = "[[maybe_unused]] " + fn.parameters;
          fn.body = "  // WARNING: unsupported assertion: '" + a.test + "'\n";
          fn.body += "  return true;\n";
          return fn;
        }
        if (!first) body += "\n      && ";
        body += translated.value();
        first = false;
      }

      // Add facet checks for simple content
      if (sc && has_sc_facets) {
        std::string cpp_type = resolver.resolve(sc->base_type_name);
        for (const auto& check : generate_facet_checks(
                 sc->facets, resolver.field_access("value", "value"),
                 cpp_type)) {
          if (!first) body += "\n      && ";
          body += check;
          first = false;
        }
      }

      // Add cardinality checks for element_only/mixed content
      if (cc && cc->content_model.has_value()) {
        for (const auto& check : collect_cardinality_checks(
                 cc->content_model.value(), "value.", &resolver, struct_name)) {
          if (!first) body += "\n      && ";
          body += check;
          first = false;
        }
      }

      body += ";\n";
      fn.body = body;
      return fn;
    }

    // Generate a validate_<type>() function for a simple type with assertions
    // and/or constraining facets. Returns nullopt if neither is present.
    std::optional<cpp_function>
    generate_simple_validate_function(const simple_type& st,
                                      const type_resolver& resolver) {
      bool has_facets = has_constraining_facets(st.facets());
      if (st.assertions().empty() && !has_facets) return std::nullopt;

      std::string type_name = resolver.type_name(st.name().local_name());
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
          fn.parameters = "[[maybe_unused]] " + fn.parameters;
          fn.body = "  // WARNING: unsupported assertion: '" + a.test + "'\n";
          fn.body += "  return true;\n";
          return fn;
        }
        if (!first) body += "\n      && ";
        body += translated.value();
        first = false;
      }

      for (const auto& check :
           generate_facet_checks(st.facets(), "value", cpp_type)) {
        if (!first) body += "\n      && ";
        body += check;
        first = false;
      }

      body += ";\n";
      fn.body = body;
      return fn;
    }

    // Merge files that share the same filename and kind.
    // When multiple schemas share a namespace, each produces a file with
    // the same name. Merging combines their declarations and re-orders
    // type declarations to resolve cross-schema dependencies.
    std::vector<cpp_file>
    merge_same_name_files(std::vector<cpp_file> files,
                          const std::set<std::string>& cycle_names = {}) {
      std::vector<std::string> key_order;
      std::map<std::string, cpp_file> by_key;

      for (auto& f : files) {
        std::string key =
            f.filename + (f.kind == file_kind::header ? ":h" : ":s");
        auto it = by_key.find(key);
        if (it == by_key.end()) {
          key_order.push_back(key);
          by_key.emplace(key, std::move(f));
        } else {
          // Merge includes (deduplicated)
          std::set<std::string> existing;
          for (const auto& inc : it->second.includes)
            existing.insert(inc.path);
          for (const auto& inc : f.includes) {
            if (!existing.count(inc.path)) {
              it->second.includes.push_back(inc);
              existing.insert(inc.path);
            }
          }
          // Merge namespaces: combine declarations for same-named namespaces
          for (auto& ns : f.namespaces) {
            bool found = false;
            for (auto& existing_ns : it->second.namespaces) {
              if (existing_ns.name == ns.name) {
                for (auto& decl : ns.declarations)
                  existing_ns.declarations.push_back(std::move(decl));
                found = true;
                break;
              }
            }
            if (!found) it->second.namespaces.push_back(std::move(ns));
          }
        }
      }

      // Re-order type declarations and deduplicate functions
      for (auto& [key, file] : by_key) {
        for (auto& ns : file.namespaces) {
          std::vector<cpp_decl> types;
          std::vector<cpp_decl> functions;
          std::set<std::string> seen_fn_sigs;
          for (auto& decl : ns.declarations) {
            if (auto* fn = std::get_if<cpp_function>(&decl)) {
              // Deduplicate functions by name + parameters
              std::string sig = fn->name + "(" + fn->parameters + ")";
              if (seen_fn_sigs.insert(sig).second)
                functions.push_back(std::move(decl));
            } else {
              types.push_back(std::move(decl));
            }
          }

          auto ordered = order_declarations(std::move(types), cycle_names);

          ns.declarations.clear();
          ns.declarations.reserve(ordered.size() + functions.size() * 2);
          for (auto& d : ordered)
            ns.declarations.push_back(std::move(d));

          // When cycle types exist, inline functions may be mutually
          // recursive (e.g. read_A calls read_B and vice versa).
          // Emit forward declarations for all inline functions so any
          // call order works. Use cpp_raw_text to avoid interfering
          // with find_function lookups on the IR.
          if (!cycle_names.empty()) {
            std::string fwd_block;
            for (const auto& decl : functions) {
              auto* fn = std::get_if<cpp_function>(&decl);
              if (!fn || !fn->is_inline) continue;
              fwd_block += fn->return_type + " " + fn->name + "(" +
                           fn->parameters + ");\n";
            }
            if (!fwd_block.empty()) {
              fwd_block += "\n";
              ns.declarations.push_back(cpp_raw_text{std::move(fwd_block)});
            }
          }

          for (auto& d : functions)
            ns.declarations.push_back(std::move(d));
        }
      }

      std::vector<cpp_file> result;
      result.reserve(key_order.size());
      for (const auto& key : key_order)
        result.push_back(std::move(by_key[key]));
      return result;
    }

  } // namespace

  std::vector<cpp_file>
  codegen::generate() const {
    std::vector<cpp_file> files;

    // Track union variant deduplication across all schemas so that
    // read functions in one schema can find parse functions from another.
    std::map<std::string, std::string> union_variant_map;

    // Pre-compute types in dependency cycles for unique_ptr cycle-breaking
    auto cycle_types = find_cycle_types(schemas_);

    // Convert cycle qnames to C++ identifier names (base naming — without
    // per-schema rename).  Used by merge_same_name_files after the loop.
    std::set<std::string> cycle_type_cpp_names_base;
    for (const auto& ct : cycle_types)
      cycle_type_cpp_names_base.insert(apply_naming(
          ct.local_name(), naming_category::type_, options_.naming));

    // Collect all namespace URIs for default_namespace_for() disambiguation
    std::set<std::string> all_namespaces;
    for (const auto& s : schemas_.schemas())
      if (!s.target_namespace().empty())
        all_namespaces.insert(s.target_namespace());

    for (const auto& s : schemas_.schemas()) {
      std::set<std::string> referenced_namespaces;

      // Collect all type names (C++ identifiers) in this schema so that
      // field names that would shadow a sibling type can be disambiguated.
      // Also detect and rename C++ name collisions between types whose XML
      // names differ but whose C++ names (after naming style conversion)
      // are identical (e.g. "anyType" and "any_type" both → "any_type").
      std::set<std::string> schema_type_names;
      std::map<std::string, std::string> type_rename;
      auto register_type_name = [&](const std::string& xml_local) {
        std::string cpp =
            apply_naming(xml_local, naming_category::type_, options_.naming);
        if (!schema_type_names.insert(cpp).second) {
          // Collision — rename the later one
          unsigned suffix = 2;
          while (schema_type_names.count(cpp + "_" + std::to_string(suffix)))
            ++suffix;
          std::string renamed = cpp + "_" + std::to_string(suffix);
          schema_type_names.insert(renamed);
          type_rename[xml_local] = renamed;
        }
      };
      for (const auto& st : s.simple_types())
        register_type_name(st.name().local_name());
      for (const auto& ct : s.complex_types())
        register_type_name(ct.name().local_name());

      type_resolver resolver{schemas_,
                             types_,
                             options_,
                             s.target_namespace(),
                             referenced_namespaces,
                             &union_variant_map,
                             &cycle_types,
                             &all_namespaces,
                             &schema_type_names,
                             &type_rename};

      // Convert cycle qnames to C++ names using the rename map
      std::set<std::string> cycle_type_cpp_names;
      for (const auto& ct : cycle_types) {
        std::string cpp = resolver.type_name(ct.local_name());
        cycle_type_cpp_names.insert(cpp);
      }

      std::vector<cpp_decl> declarations;
      std::set<std::string> seen_variant_types;

      for (const auto& st : s.simple_types()) {
        declarations.push_back(
            translate_simple_type(st, resolver, union_variant_map));
        if (auto fmt =
                generate_format_function(st, resolver, seen_variant_types))
          declarations.push_back(std::move(*fmt));
        if (auto pfn =
                generate_parse_function(st, resolver, seen_variant_types))
          declarations.push_back(std::move(*pfn));
      }

      std::unordered_map<std::string, field_plan> type_plans;
      for (const auto& ct : s.complex_types()) {
        field_plan plan;
        declarations.push_back(translate_complex_type(ct, resolver, s, &plan));
        if (!plan.empty()) {
          std::string name = resolver.type_name(ct.name().local_name());
          type_plans[name] = std::move(plan);
        }
      }

      // Order type declarations first (structs, enums, aliases, forward decls)
      auto ordered_types =
          order_declarations(std::move(declarations), cycle_type_cpp_names);

      // Build a map from type name -> complex_type for ordered generation
      std::unordered_map<std::string, const complex_type*> ct_by_name;
      for (const auto& ct : s.complex_types())
        ct_by_name[apply_naming(ct.name().local_name(), naming_category::type_,
                                options_.naming)] = &ct;

      // In wrapped mode, insert raw structs before their corresponding
      // classes (in the same namespace, no detail:: wrapper).
      bool is_wrapped = options_.encapsulation == encapsulation_mode::wrapped;
      if (is_wrapped) {
        std::vector<cpp_decl> expanded;
        for (auto& decl : ordered_types) {
          if (auto* cls = std::get_if<cpp_class>(&decl)) {
            cpp_struct raw;
            raw.name = cls->raw_struct_name;
            raw.fields = cls->fields;
            raw.generate_equality = true;
            expanded.push_back(std::move(raw));
          }
          expanded.push_back(std::move(decl));
        }
        ordered_types = std::move(expanded);
      }

      // Generate read_/write_ functions in dependency order
      std::vector<const complex_type*> ordered_cts;
      for (const auto& decl : ordered_types) {
        auto n = decl_name(decl);
        if (n.empty()) continue;
        auto it = ct_by_name.find(n);
        if (it != ct_by_name.end()) ordered_cts.push_back(it->second);
      }
      for (const auto* ct_ptr : ordered_cts) {
        std::string ct_name = resolver.type_name(ct_ptr->name().local_name());
        const field_plan* ct_plan = nullptr;
        auto plan_it = type_plans.find(ct_name);
        if (plan_it != type_plans.end()) ct_plan = &plan_it->second;
        auto read_fn = generate_read_function(*ct_ptr, resolver, s, ct_plan);
        auto write_fn = generate_write_function(*ct_ptr, resolver, s, ct_plan);

        if (is_wrapped) {
          auto type_name = resolver.type_name(ct_ptr->name().local_name());
          auto raw_name = type_name + "_data";

          // Read: use raw struct, then construct class from it
          auto& rb = read_fn.body;
          rb.replace(0, rb.find('\n'), "  " + raw_name + " result;");
          auto ret_pos = rb.rfind("return result;");
          if (ret_pos != std::string::npos) {
            rb.replace(ret_pos, 14,
                       "return " + type_name + "(std::move(result));");
          }
          // Write: field access already uses getter syntax via resolver
        }

        ordered_types.push_back(std::move(read_fn));
        ordered_types.push_back(std::move(write_fn));
      }

      // Generate validate_ functions (unless validation is disabled)
      if (options_.validation != validation_mode::none) {
        for (const auto* ct_ptr : ordered_cts) {
          if (auto vf = generate_validate_function(*ct_ptr, resolver))
            ordered_types.push_back(std::move(*vf));
        }
        for (const auto& st : s.simple_types()) {
          if (auto vf = generate_simple_validate_function(st, resolver))
            ordered_types.push_back(std::move(*vf));
        }
      }

      // Generate JSON to_json/from_json functions when enabled
      if (options_.json == json_mode::enabled) {
        std::vector<cpp_decl> json_decls;
        std::set<std::string> json_seen_variants;

        // Enums and unions
        for (const auto& st : s.simple_types()) {
          if (!st.facets().enumeration.empty()) {
            cpp_enum e;
            e.name = resolver.type_name(st.name().local_name());
            if (auto tj = generate_enum_to_json(e))
              json_decls.push_back(std::move(*tj));
            if (auto fj = generate_enum_from_json(e))
              json_decls.push_back(std::move(*fj));
          }
          if (auto tj =
                  generate_union_to_json(st, resolver, json_seen_variants))
            json_decls.push_back(std::move(*tj));
          if (auto fj =
                  generate_union_from_json(st, resolver, json_seen_variants))
            json_decls.push_back(std::move(*fj));
        }

        // Complex types (structs or classes)
        for (const auto& decl : ordered_types) {
          if (auto* st_decl = std::get_if<cpp_struct>(&decl)) {
            json_decls.push_back(generate_to_json_function(*st_decl));
            json_decls.push_back(generate_from_json_function(*st_decl));
          } else if (auto* cls_decl = std::get_if<cpp_class>(&decl)) {
            json_decls.push_back(generate_to_json_function(*cls_decl));
            json_decls.push_back(generate_from_json_function(*cls_decl));
          }
        }

        for (auto& d : json_decls)
          ordered_types.push_back(std::move(d));
      }

      // In split mode, mark functions and class methods as non-inline
      if (options_.mode == output_mode::split ||
          options_.mode == output_mode::file_per_type) {
        for (auto& decl : ordered_types) {
          if (auto* fn = std::get_if<cpp_function>(&decl))
            fn->is_inline = false;
          else if (auto* cls = std::get_if<cpp_class>(&decl))
            cls->inline_methods = false;
        }
      }

      std::string ns_name =
          options_.ns_style == namespace_style::short_name
              ? default_namespace_for(s.target_namespace(), all_namespaces,
                                      options_)
              : cpp_namespace_for(s.target_namespace(), options_);
      std::string stem = stem_for_namespace(s.target_namespace());
      std::string header_filename = stem + options_.header_suffix;

      if (options_.mode == output_mode::header_only) {
        // Single header file per namespace (current behavior)
        cpp_namespace ns;
        ns.name = ns_name;
        ns.declarations = std::move(ordered_types);

        auto includes = compute_includes(
            referenced_namespaces, schemas_.schemas(), ns.declarations,
            file_kind::header, "", options_.header_suffix);

        cpp_file file;
        file.filename = header_filename;
        file.kind = file_kind::header;
        file.includes = std::move(includes);
        file.namespaces.push_back(std::move(ns));

        files.push_back(std::move(file));
      } else if (options_.mode == output_mode::split) {
        // Header + source file per namespace

        // Extract forward declarations into a separate file if requested
        std::vector<cpp_decl> fwd_decls;
        if (options_.separate_fwd_header) {
          std::vector<cpp_decl> non_fwd;
          for (auto& decl : ordered_types) {
            if (std::holds_alternative<cpp_forward_decl>(decl))
              fwd_decls.push_back(std::move(decl));
            else
              non_fwd.push_back(std::move(decl));
          }
          ordered_types = std::move(non_fwd);
        }

        cpp_namespace ns;
        ns.name = ns_name;
        ns.declarations = std::move(ordered_types);

        auto header_includes = compute_includes(
            referenced_namespaces, schemas_.schemas(), ns.declarations,
            file_kind::header, "", options_.header_suffix);

        // If we extracted forward declarations, emit a _fwd header
        if (!fwd_decls.empty()) {
          std::string fwd_filename = stem + "_fwd" + options_.header_suffix;

          cpp_namespace fwd_ns;
          fwd_ns.name = ns_name;
          fwd_ns.declarations = std::move(fwd_decls);

          cpp_file fwd_file;
          fwd_file.filename = fwd_filename;
          fwd_file.kind = file_kind::header;
          fwd_file.namespaces.push_back(std::move(fwd_ns));

          files.push_back(std::move(fwd_file));

          // Main header includes the fwd file
          header_includes.insert(header_includes.begin(),
                                 cpp_include{"\"" + fwd_filename + "\""});
        }

        auto source_includes = compute_includes(
            referenced_namespaces, schemas_.schemas(), ns.declarations,
            file_kind::source, header_filename, options_.header_suffix);

        cpp_file header;
        header.filename = header_filename;
        header.kind = file_kind::header;
        header.includes = std::move(header_includes);
        header.namespaces.push_back(ns); // copy — shared with source

        cpp_file source;
        source.filename = stem + options_.source_suffix;
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

        // In wrapped mode, raw structs (X_data) immediately precede
        // their cpp_class. Buffer them to group together.
        std::vector<cpp_decl> raw_buffer;

        for (auto& decl : ordered_types) {
          if (auto* st = std::get_if<cpp_struct>(&decl)) {
            // In wrapped mode, check if this struct is the raw companion
            // for an upcoming class (name ends with _data).
            if (is_wrapped) {
              raw_buffer.push_back(std::move(decl));
            } else {
              groups.push_back({st->name, {std::move(decl)}});
            }
          } else if (auto* cls = std::get_if<cpp_class>(&decl)) {
            // Flush buffered raw struct into this class's group
            auto class_name = cls->name; // capture before move
            std::vector<cpp_decl> group_decls;
            for (auto& d : raw_buffer)
              group_decls.push_back(std::move(d));
            raw_buffer.clear();
            group_decls.push_back(std::move(decl));
            groups.push_back({class_name, std::move(group_decls)});
            // Class method definitions go in the source file
            function_decls.push_back(
                cpp_class(std::get<cpp_class>(groups.back().decls.back())));
          } else if (auto* en = std::get_if<cpp_enum>(&decl)) {
            groups.push_back({en->name, {std::move(decl)}});
          } else if (auto* alias = std::get_if<cpp_type_alias>(&decl)) {
            groups.push_back({alias->name, {std::move(decl)}});
          } else if (auto* fwd = std::get_if<cpp_forward_decl>(&decl)) {
            // In file-per-type mode, forward declarations for a type are
            // not needed in the type's own file (the full definition is
            // there).  They're added to referencing files via
            // unique_ptr_dependencies below.  Keep unmatched forward decls
            // as standalone groups for types not yet seen.
            bool has_group = false;
            for (const auto& g : groups) {
              if (g.type_name == fwd->name) {
                has_group = true;
                break;
              }
            }
            if (!has_group) groups.push_back({fwd->name, {std::move(decl)}});
          } else if (std::holds_alternative<cpp_function>(decl)) {
            // Keep the function for source file generation
            function_decls.push_back(std::move(decl));
          }
        }

        // Add function declarations to per-type groups so they appear
        // in the per-type header files.
        for (const auto& fd : function_decls) {
          if (auto* fn = std::get_if<cpp_function>(&fd)) {
            for (auto& g : groups) {
              if (fn->name == "read_" + g.type_name ||
                  fn->name == "write_" + g.type_name ||
                  fn->name == "validate_" + g.type_name) {
                g.decls.push_back(*fn);
                break;
              }
            }
          }
        }

        // Emit per-type header files
        std::vector<std::string> per_type_filenames;
        for (const auto& group : groups) {
          std::string type_filename =
              stem + "_" + group.type_name + options_.header_suffix;
          per_type_filenames.push_back(type_filename);

          // Compute includes for this type's declarations
          std::set<std::string> empty_ns_refs;
          auto type_includes =
              compute_includes(empty_ns_refs, schemas_.schemas(), group.decls,
                               file_kind::header, "", options_.header_suffix);

          // Add includes for cross-type dependencies within the namespace
          std::set<std::string> deps;
          for (const auto& d : group.decls) {
            auto d_deps = decl_dependencies(d);
            deps.insert(d_deps.begin(), d_deps.end());
          }
          for (const auto& dep_name : deps) {
            for (const auto& other : groups) {
              if (other.type_name == dep_name &&
                  other.type_name != group.type_name) {
                type_includes.push_back({"\"" + stem + "_" + other.type_name +
                                         options_.header_suffix + "\""});
                break;
              }
            }
          }

          // Add forward declarations for cycle types referenced via
          // unique_ptr (these types can't be #included due to circular
          // deps, so they need forward declarations instead).
          std::vector<cpp_decl> fwd_decls;
          std::set<std::string> fwd_seen;
          for (const auto& d : group.decls) {
            for (const auto& uptr_dep : unique_ptr_dependencies(d)) {
              if (uptr_dep != group.type_name &&
                  fwd_seen.insert(uptr_dep).second) {
                bool is_local = false;
                for (const auto& other : groups) {
                  if (other.type_name == uptr_dep) {
                    is_local = true;
                    break;
                  }
                }
                if (is_local)
                  fwd_decls.push_back(cpp_forward_decl{uptr_dep, is_wrapped});
              }
            }
          }

          cpp_namespace type_ns;
          type_ns.name = ns_name;
          for (auto& fd : fwd_decls)
            type_ns.declarations.push_back(std::move(fd));
          for (auto& d : group.decls)
            type_ns.declarations.push_back(d);

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
                                           schemas_.schemas(),
                                           options_.header_suffix);
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

        if (is_wrapped) {
          // Wrapped mode: per-type source files. Associate functions
          // with their type groups by name prefix.
          std::map<std::string, std::size_t> type_to_group;
          for (std::size_t i = 0; i < groups.size(); ++i)
            type_to_group[groups[i].type_name] = i;

          std::vector<std::vector<cpp_decl>> per_type_src(groups.size());
          std::vector<cpp_decl> unassigned_fns;

          for (auto& fd : function_decls) {
            bool assigned = false;
            if (auto* cls = std::get_if<cpp_class>(&fd)) {
              auto it = type_to_group.find(cls->name);
              if (it != type_to_group.end()) {
                per_type_src[it->second].push_back(std::move(fd));
                assigned = true;
              }
            } else if (auto* fn = std::get_if<cpp_function>(&fd)) {
              for (const auto& [tname, idx] : type_to_group) {
                if (fn->name == "read_" + tname ||
                    fn->name == "write_" + tname ||
                    fn->name == "validate_" + tname) {
                  per_type_src[idx].push_back(std::move(fd));
                  assigned = true;
                  break;
                }
                if (fn->name == "to_json" || fn->name == "from_json") {
                  if (fn->parameters.find(tname) != std::string::npos) {
                    per_type_src[idx].push_back(std::move(fd));
                    assigned = true;
                    break;
                  }
                }
              }
            }
            if (!assigned) unassigned_fns.push_back(std::move(fd));
          }

          // Emit per-type source files
          for (std::size_t i = 0; i < groups.size(); ++i) {
            if (per_type_src[i].empty()) continue;

            std::string type_src_filename =
                stem + "_" + groups[i].type_name + options_.source_suffix;
            std::string type_hdr_filename = per_type_filenames[i];

            cpp_namespace src_ns;
            src_ns.name = ns_name;
            src_ns.declarations = std::move(per_type_src[i]);

            // Include the umbrella header so all type definitions and
            // sibling read_/write_ declarations are visible.
            auto src_includes = compute_includes(
                referenced_namespaces, schemas_.schemas(), src_ns.declarations,
                file_kind::source, header_filename, options_.header_suffix);

            cpp_file src_file;
            src_file.filename = type_src_filename;
            src_file.kind = file_kind::source;
            src_file.includes = std::move(src_includes);
            src_file.namespaces.push_back(std::move(src_ns));

            files.push_back(std::move(src_file));
          }

          // Any unassigned functions go in the main source file
          if (!unassigned_fns.empty()) {
            cpp_namespace fn_ns;
            fn_ns.name = ns_name;
            fn_ns.declarations = std::move(unassigned_fns);

            auto source_includes = compute_includes(
                referenced_namespaces, schemas_.schemas(), fn_ns.declarations,
                file_kind::source, header_filename, options_.header_suffix);

            cpp_file source;
            source.filename = stem + options_.source_suffix;
            source.kind = file_kind::source;
            source.includes = std::move(source_includes);
            source.namespaces.push_back(std::move(fn_ns));

            files.push_back(std::move(source));
          }
        } else {
          // Raw struct mode: single source file with all functions
          cpp_namespace fn_ns;
          fn_ns.name = ns_name;
          fn_ns.declarations = std::move(function_decls);

          auto source_includes = compute_includes(
              referenced_namespaces, schemas_.schemas(), fn_ns.declarations,
              file_kind::source, header_filename, options_.header_suffix);

          cpp_file source;
          source.filename = stem + options_.source_suffix;
          source.kind = file_kind::source;
          source.includes = std::move(source_includes);
          source.namespaces.push_back(std::move(fn_ns));

          files.push_back(std::move(source));
        }
      }
    }

    auto result =
        merge_same_name_files(std::move(files), cycle_type_cpp_names_base);

    // When separate_fwd_header is enabled, strip any forward declarations
    // that merge_same_name_files may have re-introduced via order_declarations.
    if (options_.separate_fwd_header) {
      for (auto& file : result) {
        // Only strip from non-fwd files
        if (file.filename.find("_fwd") != std::string::npos) continue;
        for (auto& ns : file.namespaces) {
          std::erase_if(ns.declarations, [](const cpp_decl& d) {
            return std::holds_alternative<cpp_forward_decl>(d);
          });
        }
      }
    }

    return result;
  }

} // namespace xb
