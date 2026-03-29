#pragma once

#include <xb/wire/encoding.hpp>

#include <string>
#include <vector>

namespace xb::wire {

  struct bes_test_value {
    std::string xml_text;
    std::string reason;
  };

  struct bes_test_value_set {
    std::string field_name;
    std::vector<bes_test_value> values;
  };

  class bes_value_generator {
  public:
    // Generate wire-level boundary values for a BES field.
    static bes_test_value_set
    generate(const bes::field_type& field);
  };

} // namespace xb::wire
