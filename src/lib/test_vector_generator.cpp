#include <xb/test_vector_generator.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group.hpp>

#include <string>

namespace xb {

  namespace {

    const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema";

    // Pick the first value from a test_value_set (the "default"
    // representative).
    std::string
    first_value(const test_value_generator& vg, const qname& type_name) {
      auto vs = vg.generate(type_name);
      if (vs.values.empty()) return "";
      return vs.values.front().xml_text;
    }

    // A field assignment plan for one particle.
    struct field_plan {
      qname field_name;
      qname type_name;
      std::size_t min_occurs;
      std::size_t max_occurs;
      bool is_complex = false;
    };

    // Resolve element_ref to element_decl.
    const element_decl*
    resolve_element(const particle& p, const schema_set& schemas) {
      if (auto* elem = std::get_if<element_decl>(&p.term)) return elem;
      if (auto* ref = std::get_if<element_ref>(&p.term))
        return schemas.find_element(ref->ref);
      return nullptr;
    }

    bool
    type_is_complex(const schema_set& schemas, const qname& type_name) {
      if (type_name.namespace_uri() == xsd_ns) return false;
      return schemas.find_complex_type(type_name) != nullptr;
    }

    // Get the model group from a complex type, if it has one.
    const model_group*
    get_model_group(const complex_type& ct) {
      const auto& content = ct.content();
      if (content.kind != content_kind::element_only &&
          content.kind != content_kind::mixed)
        return nullptr;
      const auto* cc = std::get_if<complex_content>(&content.detail);
      if (!cc || !cc->content_model) return nullptr;
      return &*cc->content_model;
    }

    // Build field plans from a model group's particles.
    std::vector<field_plan>
    collect_field_plans(const model_group& mg, const schema_set& schemas) {
      std::vector<field_plan> plans;
      for (const auto& p : mg.particles()) {
        const auto* elem = resolve_element(p, schemas);
        if (!elem) continue;
        field_plan fp;
        fp.field_name = elem->name();
        fp.type_name = elem->type_name();
        fp.min_occurs = p.occurs.min_occurs;
        fp.max_occurs = p.occurs.max_occurs;
        fp.is_complex = type_is_complex(schemas, elem->type_name());
        plans.push_back(std::move(fp));
      }
      return plans;
    }

    // Generate a single field value using the first available test value.
    test_field_value
    make_simple_field(const test_value_generator& vg, const field_plan& plan) {
      return {plan.field_name, first_value(vg, plan.type_name)};
    }

    // Generate a "baseline" vector: all required fields with their first
    // values.
    std::vector<test_field_value>
    make_baseline_fields(const test_value_generator& vg,
                         const std::vector<field_plan>& plans) {
      std::vector<test_field_value> fields;
      for (const auto& plan : plans) {
        if (plan.min_occurs == 0) continue;
        if (plan.max_occurs > 1) {
          // Repeated: use min_occurs instances
          std::vector<std::vector<test_field_value>> repeated;
          for (std::size_t i = 0; i < plan.min_occurs; ++i) {
            repeated.push_back(
                {{plan.field_name, first_value(vg, plan.type_name)}});
          }
          fields.push_back({plan.field_name, std::move(repeated)});
        } else {
          fields.push_back(make_simple_field(vg, plan));
        }
      }
      return fields;
    }

    // Generate vectors for a sequence compositor.
    std::vector<test_vector>
    generate_sequence_vectors(const qname& type_name, const model_group& mg,
                              const std::vector<attribute_use>& attrs,
                              const schema_set& schemas,
                              const test_value_generator& vg) {
      std::vector<test_vector> result;
      auto plans = collect_field_plans(mg, schemas);

      // Baseline: all required fields with first boundary value.
      auto baseline = make_baseline_fields(vg, plans);

      // Add required attribute fields to baseline.
      for (const auto& attr : attrs) {
        if (attr.required) {
          baseline.push_back({attr.name, first_value(vg, attr.type_name)});
        }
      }

      result.push_back({type_name, "baseline", baseline});

      // For each field, generate vectors that vary its value across boundaries.
      for (const auto& plan : plans) {
        if (plan.min_occurs == 0 && plan.max_occurs <= 1) {
          // Optional element: generate absent and present vectors.
          // Absent: baseline without this field (it's already absent).
          result.push_back({type_name,
                            "optional-absent:" + plan.field_name.local_name(),
                            baseline});

          // Present: baseline + this field.
          auto with_field = baseline;
          with_field.push_back(make_simple_field(vg, plan));
          result.push_back({type_name,
                            "optional-present:" + plan.field_name.local_name(),
                            std::move(with_field)});
        } else if (plan.max_occurs > 1) {
          // Repeating element: generate vectors at boundary counts.
          std::vector<std::size_t> counts;
          if (plan.min_occurs == 0) counts.push_back(0);
          counts.push_back(plan.min_occurs > 0 ? plan.min_occurs : 1);
          if (plan.max_occurs != xb::unbounded) {
            if (plan.max_occurs > 1) counts.push_back(plan.max_occurs);
          } else {
            counts.push_back(2); // small representative for unbounded
            counts.push_back(5); // larger representative for unbounded
          }

          for (std::size_t count : counts) {
            auto fields = make_baseline_fields(vg, {});
            // Add required attribute fields.
            for (const auto& attr : attrs) {
              if (attr.required)
                fields.push_back({attr.name, first_value(vg, attr.type_name)});
            }
            // Add other required fields (excluding the varying one).
            for (const auto& other : plans) {
              if (other.field_name == plan.field_name) continue;
              if (other.min_occurs == 0) continue;
              fields.push_back(make_simple_field(vg, other));
            }

            if (count == 0) {
              // No items: field is absent.
              result.push_back(
                  {type_name, "repeat-count-0:" + plan.field_name.local_name(),
                   std::move(fields)});
            } else {
              std::vector<std::vector<test_field_value>> repeated;
              auto vs = vg.generate(plan.type_name);
              for (std::size_t i = 0; i < count; ++i) {
                std::string val = (i < vs.values.size())
                                      ? vs.values[i].xml_text
                                      : first_value(vg, plan.type_name);
                repeated.push_back({{plan.field_name, val}});
              }
              fields.push_back({plan.field_name, std::move(repeated)});
              result.push_back({type_name,
                                "repeat-count-" + std::to_string(count) + ":" +
                                    plan.field_name.local_name(),
                                std::move(fields)});
            }
          }
        } else {
          // Required scalar: vary its boundary values.
          auto vs = vg.generate(plan.type_name);
          for (const auto& val : vs.values) {
            auto fields = baseline;
            // Replace this field's value.
            for (auto& f : fields) {
              if (f.field_name == plan.field_name) {
                f.content = val.xml_text;
                break;
              }
            }
            result.push_back(
                {type_name,
                 "boundary:" + plan.field_name.local_name() + ":" + val.reason,
                 std::move(fields)});
          }
        }
      }

      // Optional attributes: absent and present.
      for (const auto& attr : attrs) {
        if (attr.required) continue;

        // Absent: baseline (no optional attr).
        result.push_back(
            {type_name, "attr-absent:" + attr.name.local_name(), baseline});

        // Present: baseline + attr.
        auto with_attr = baseline;
        with_attr.push_back({attr.name, first_value(vg, attr.type_name)});
        result.push_back({type_name, "attr-present:" + attr.name.local_name(),
                          std::move(with_attr)});
      }

      return result;
    }

    // Generate vectors for a choice compositor.
    std::vector<test_vector>
    generate_choice_vectors(const qname& type_name, const model_group& mg,
                            const std::vector<attribute_use>& attrs,
                            const schema_set& schemas,
                            const test_value_generator& vg) {
      std::vector<test_vector> result;
      std::size_t alt_index = 0;

      for (const auto& p : mg.particles()) {
        ++alt_index;
        const auto* elem = resolve_element(p, schemas);
        if (!elem) continue;

        std::string alt_label = "choice-alt-" + std::to_string(alt_index) +
                                "-of-" + std::to_string(mg.particles().size());

        auto vs = vg.generate(elem->type_name());
        if (vs.values.empty()) {
          // Still generate one vector with an empty value.
          std::vector<test_field_value> fields;
          fields.push_back({elem->name(), std::string("")});
          for (const auto& attr : attrs) {
            if (attr.required)
              fields.push_back({attr.name, first_value(vg, attr.type_name)});
          }
          result.push_back({type_name, alt_label, std::move(fields)});
        } else {
          // Generate one vector per boundary value for this alternative.
          for (const auto& val : vs.values) {
            std::vector<test_field_value> fields;
            fields.push_back({elem->name(), val.xml_text});
            for (const auto& attr : attrs) {
              if (attr.required)
                fields.push_back({attr.name, first_value(vg, attr.type_name)});
            }
            result.push_back(
                {type_name, alt_label + ":" + val.reason, std::move(fields)});
          }
        }
      }

      return result;
    }

    // Generate exhaustive cross-product vectors for sequence types.
    std::vector<test_vector>
    generate_exhaustive_vectors(const qname& type_name, const model_group& mg,
                                const std::vector<attribute_use>& attrs,
                                const schema_set& schemas,
                                const test_value_generator& vg) {
      auto plans = collect_field_plans(mg, schemas);

      // Collect value sets for each required scalar field.
      struct field_values {
        field_plan plan;
        std::vector<std::string> values;
      };

      std::vector<field_values> fields;
      for (const auto& p : plans) {
        if (p.min_occurs == 0 || p.max_occurs > 1 || p.is_complex) continue;
        auto vs = vg.generate(p.type_name);
        std::vector<std::string> vals;
        for (const auto& v : vs.values)
          vals.push_back(v.xml_text);
        if (vals.empty()) vals.push_back("");
        fields.push_back({p, std::move(vals)});
      }

      // Fall back to boundary if there are no required scalar fields
      // (all fields are optional, complex, or repeating).
      if (fields.empty())
        return generate_sequence_vectors(type_name, mg, attrs, schemas, vg);

      // Cross-product: iterate through all combinations.
      std::vector<std::size_t> indices(fields.size(), 0);
      std::vector<test_vector> result;

      for (;;) {
        // Build a vector from the current index combination.
        std::vector<test_field_value> fv;
        std::string label = "exhaustive:";
        for (std::size_t i = 0; i < fields.size(); ++i) {
          if (i > 0) label += ",";
          label += fields[i].plan.field_name.local_name() + "=" +
                   fields[i].values[indices[i]];
          fv.push_back(
              {fields[i].plan.field_name, fields[i].values[indices[i]]});
        }
        // Add required attributes.
        for (const auto& attr : attrs) {
          if (attr.required)
            fv.push_back({attr.name, first_value(vg, attr.type_name)});
        }
        result.push_back({type_name, label, std::move(fv)});

        // Advance the multi-index.
        bool done = true;
        for (std::size_t i = fields.size(); i-- > 0;) {
          if (++indices[i] < fields[i].values.size()) {
            done = false;
            break;
          }
          indices[i] = 0;
        }
        if (done) break;
      }

      return result;
    }

    // Generate pairwise (2-way covering) vectors for sequence types.
    // Uses a greedy algorithm: for each uncovered pair of (field_i=val_a,
    // field_j=val_b), find or extend a vector that covers it.
    std::vector<test_vector>
    generate_pairwise_vectors(const qname& type_name, const model_group& mg,
                              const std::vector<attribute_use>& attrs,
                              const schema_set& schemas,
                              const test_value_generator& vg) {
      auto plans = collect_field_plans(mg, schemas);

      struct field_values {
        field_plan plan;
        std::vector<std::string> values;
      };

      std::vector<field_values> fields;
      for (const auto& p : plans) {
        if (p.min_occurs == 0 || p.max_occurs > 1 || p.is_complex) continue;
        auto vs = vg.generate(p.type_name);
        std::vector<std::string> vals;
        for (const auto& v : vs.values)
          vals.push_back(v.xml_text);
        if (vals.empty()) vals.push_back("");
        fields.push_back({p, std::move(vals)});
      }

      std::size_t n = fields.size();
      if (n < 2)
        return generate_sequence_vectors(type_name, mg, attrs, schemas, vg);

      // Build the set of all pairs that need covering.
      // A pair is (field_i, val_a, field_j, val_b) where i < j.
      struct pair_key {
        std::size_t fi, vi, fj, vj;
      };

      std::vector<pair_key> all_pairs;
      for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
          for (std::size_t vi = 0; vi < fields[i].values.size(); ++vi) {
            for (std::size_t vj = 0; vj < fields[j].values.size(); ++vj) {
              all_pairs.push_back({i, vi, j, vj});
            }
          }
        }
      }

      // Greedy: each row is a vector of value indices (one per field).
      // "don't care" slots are represented by SIZE_MAX.
      constexpr auto DONTCARE = std::size_t(-1);
      std::vector<std::vector<std::size_t>> rows;

      // Mark pairs as covered.
      std::vector<bool> covered(all_pairs.size(), false);

      while (true) {
        // Find an uncovered pair.
        std::size_t best_pair = all_pairs.size();
        for (std::size_t p = 0; p < all_pairs.size(); ++p) {
          if (!covered[p]) {
            best_pair = p;
            break;
          }
        }
        if (best_pair == all_pairs.size()) break; // all covered

        auto [fi, vi, fj, vj] = all_pairs[best_pair];

        // Try to extend an existing row.
        bool extended = false;
        for (auto& row : rows) {
          bool fits = (row[fi] == vi || row[fi] == DONTCARE) &&
                      (row[fj] == vj || row[fj] == DONTCARE);
          if (fits) {
            row[fi] = vi;
            row[fj] = vj;
            extended = true;
            break;
          }
        }

        if (!extended) {
          // Start a new row.
          std::vector<std::size_t> row(n, DONTCARE);
          row[fi] = vi;
          row[fj] = vj;
          rows.push_back(std::move(row));
        }

        // Mark all pairs covered by this update.
        for (std::size_t p = 0; p < all_pairs.size(); ++p) {
          if (covered[p]) continue;
          auto [pi, pvi, pj, pvj] = all_pairs[p];
          // Check all rows.
          for (const auto& row : rows) {
            if (row[pi] == pvi && row[pj] == pvj) {
              covered[p] = true;
              break;
            }
          }
        }
      }

      // Fill don't-care slots with the first value.
      for (auto& row : rows) {
        for (std::size_t i = 0; i < n; ++i) {
          if (row[i] == DONTCARE) row[i] = 0;
        }
      }

      // Convert rows to test_vector objects.
      std::vector<test_vector> result;
      for (std::size_t r = 0; r < rows.size(); ++r) {
        std::vector<test_field_value> fv;
        for (std::size_t i = 0; i < n; ++i) {
          fv.push_back(
              {fields[i].plan.field_name, fields[i].values[rows[r][i]]});
        }
        for (const auto& attr : attrs) {
          if (attr.required)
            fv.push_back({attr.name, first_value(vg, attr.type_name)});
        }
        result.push_back(
            {type_name, "pairwise-" + std::to_string(r + 1), std::move(fv)});
      }

      return result;
    }

  } // namespace

  test_vector_generator::test_vector_generator(
      const schema_set& schemas, const test_value_generator& value_gen)
      : schemas_(schemas), value_gen_(value_gen) {}

  std::vector<test_vector>
  test_vector_generator::generate(const qname& element_name,
                                  const test_vector_options& opts) const {
    const auto* elem = schemas_.find_element(element_name);
    if (!elem) return {};

    const auto* ct = schemas_.find_complex_type(elem->type_name());
    if (!ct) {
      auto vs = value_gen_.generate(elem->type_name());
      std::vector<test_vector> result;
      for (const auto& val : vs.values) {
        result.push_back({elem->type_name(),
                          "simple:" + val.reason,
                          {{elem->name(), val.xml_text}}});
      }
      if (opts.max_vectors && result.size() > *opts.max_vectors)
        result.resize(*opts.max_vectors);
      return result;
    }

    const auto* mg = get_model_group(*ct);
    if (!mg) return {};

    const auto& attrs = ct->attributes();
    std::vector<test_vector> result;

    switch (mg->compositor()) {
      case compositor_kind::choice:
        result = generate_choice_vectors(ct->name(), *mg, attrs, schemas_,
                                         value_gen_);
        break;

      case compositor_kind::sequence:
      case compositor_kind::all:
      case compositor_kind::interleave:
        if (opts.coverage == coverage_level::exhaustive) {
          result = generate_exhaustive_vectors(ct->name(), *mg, attrs, schemas_,
                                               value_gen_);
        } else if (opts.coverage == coverage_level::pairwise) {
          result = generate_pairwise_vectors(ct->name(), *mg, attrs, schemas_,
                                             value_gen_);
        } else {
          result = generate_sequence_vectors(ct->name(), *mg, attrs, schemas_,
                                             value_gen_);
        }
        break;
    }

    if (opts.max_vectors && result.size() > *opts.max_vectors)
      result.resize(*opts.max_vectors);

    return result;
  }

} // namespace xb
