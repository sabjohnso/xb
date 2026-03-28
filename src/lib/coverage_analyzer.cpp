#include <xb/coverage_analyzer.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group.hpp>
#include <xb/simple_type.hpp>
#include <xb/test_value_generator.hpp>
#include <xb/test_vector_generator.hpp>

#include <iomanip>
#include <ostream>
#include <set>
#include <string>

namespace xb {

  namespace {

    const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema";

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

    const element_decl*
    resolve_element(const particle& p, const schema_set& schemas) {
      if (auto* elem = std::get_if<element_decl>(&p.term)) return elem;
      if (auto* ref = std::get_if<element_ref>(&p.term))
        return schemas.find_element(ref->ref);
      return nullptr;
    }

    // Count schema-level totals by walking the type.
    void
    count_schema_totals(const schema_set& schemas, const qname& type_name,
                        coverage_report& report, std::set<qname>& visited) {
      if (visited.count(type_name)) return;
      visited.insert(type_name);

      // Check for enumeration on simple type
      if (type_name.namespace_uri() != xsd_ns) {
        const auto* st = schemas.find_simple_type(type_name);
        if (st && !st->facets().enumeration.empty()) {
          report.total_enum_values += st->facets().enumeration.size();
        }
      }

      // Count boundary values for this type
      test_value_generator vg(schemas);
      auto vs = vg.generate(type_name);
      report.total_boundary_values += vs.values.size();

      // Walk complex type content
      const auto* ct = schemas.find_complex_type(type_name);
      if (!ct) return;

      report.total_types++;

      const auto* mg = get_model_group(*ct);
      if (!mg) return;

      if (mg->compositor() == compositor_kind::choice) {
        report.total_choice_alts += mg->particles().size();
      }

      for (const auto& p : mg->particles()) {
        // Count optional elements
        if (p.occurs.min_occurs == 0 && p.occurs.max_occurs <= 1) {
          report.total_optional_combos += 2; // present + absent
        }

        // Recurse into element types
        const auto* elem = resolve_element(p, schemas);
        if (elem) {
          count_schema_totals(schemas, elem->type_name(), report, visited);
        }
      }

      // Count optional attributes
      for (const auto& attr : ct->attributes()) {
        if (!attr.required) {
          report.total_optional_combos += 2; // present + absent
        }
      }
    }

    // Count what the vector generator actually covers.
    void
    count_covered(const schema_set& schemas, const qname& element_name,
                  coverage_report& report) {
      test_value_generator vg(schemas);
      test_vector_generator vec_gen(schemas, vg);
      auto vectors = vec_gen.generate(element_name);

      if (vectors.empty()) return;

      // Covered types: we generated vectors, so the root type is covered
      const auto* elem = schemas.find_element(element_name);
      if (!elem) return;
      const auto* ct = schemas.find_complex_type(elem->type_name());
      if (ct) report.covered_types++;

      // Covered boundary values = total (we generate all of them)
      report.covered_boundary_values = report.total_boundary_values;

      // Covered enum values = total (we generate all of them)
      report.covered_enum_values = report.total_enum_values;

      // Count covered choice alternatives from vector labels
      std::set<std::string> seen_alts;
      for (const auto& vec : vectors) {
        if (vec.label.find("choice-alt-") != std::string::npos) {
          // Extract the alt identifier (e.g., "choice-alt-1-of-3")
          auto end = vec.label.find(':', vec.label.find("choice-alt-"));
          std::string alt_id =
              (end != std::string::npos) ? vec.label.substr(0, end) : vec.label;
          seen_alts.insert(alt_id);
        }
      }
      report.covered_choice_alts = seen_alts.size();

      // Count covered optional combos from vector labels
      std::set<std::string> seen_opt;
      for (const auto& vec : vectors) {
        auto check = [&](const std::string& prefix) {
          if (vec.label.find(prefix) != std::string::npos) {
            seen_opt.insert(vec.label);
          }
        };
        check("optional-absent:");
        check("optional-present:");
        check("attr-absent:");
        check("attr-present:");
      }
      report.covered_optional_combos = seen_opt.size();
    }

    double
    percentage(std::size_t covered, std::size_t total) {
      if (total == 0) return 100.0;
      return 100.0 * static_cast<double>(covered) / static_cast<double>(total);
    }

  } // namespace

  coverage_analyzer::coverage_analyzer(const schema_set& schemas)
      : schemas_(schemas) {}

  coverage_report
  coverage_analyzer::analyze(const qname& element_name) const {
    coverage_report report{};

    const auto* elem = schemas_.find_element(element_name);
    if (!elem) return report;

    // Count totals from schema
    std::set<qname> visited;
    count_schema_totals(schemas_, elem->type_name(), report, visited);

    // Count what we actually cover
    count_covered(schemas_, element_name, report);

    return report;
  }

  void
  coverage_analyzer::print(const coverage_report& report, std::ostream& out) {
    out << "Test Vector Coverage Report\n";
    out << "===========================\n";

    auto line = [&](const char* label, std::size_t covered, std::size_t total) {
      out << std::left << std::setw(16) << label << std::right << std::setw(4)
          << covered << " / " << std::setw(4) << total << "  (" << std::fixed
          << std::setprecision(1) << percentage(covered, total) << "%)\n";
    };

    line("Types:", report.covered_types, report.total_types);
    line("CHOICE alts:", report.covered_choice_alts, report.total_choice_alts);
    line("Enum values:", report.covered_enum_values, report.total_enum_values);
    line("Boundary vals:", report.covered_boundary_values,
         report.total_boundary_values);
    line("Optional combos:", report.covered_optional_combos,
         report.total_optional_combos);
  }

} // namespace xb
