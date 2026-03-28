#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>

#include <cstddef>
#include <iosfwd>

namespace xb {

  struct coverage_report {
    std::size_t total_types = 0;
    std::size_t covered_types = 0;
    std::size_t total_choice_alts = 0;
    std::size_t covered_choice_alts = 0;
    std::size_t total_enum_values = 0;
    std::size_t covered_enum_values = 0;
    std::size_t total_boundary_values = 0;
    std::size_t covered_boundary_values = 0;
    std::size_t total_optional_combos = 0;
    std::size_t covered_optional_combos = 0;
  };

  class coverage_analyzer {
    const schema_set& schemas_;

  public:
    explicit coverage_analyzer(const schema_set& schemas);

    coverage_report
    analyze(const qname& element_name) const;

    static void
    print(const coverage_report& report, std::ostream& out);
  };

} // namespace xb
