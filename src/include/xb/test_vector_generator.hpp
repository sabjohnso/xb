#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/test_value_generator.hpp>

#include <string>
#include <variant>
#include <vector>

namespace xb {

  struct test_field_value {
    qname field_name;
    std::variant<std::string,                               // simple value
                 std::vector<test_field_value>,             // nested complex
                 std::vector<std::vector<test_field_value>> // repeated
                 >
        content;
  };

  struct test_vector {
    qname type_name;
    std::string label;
    std::vector<test_field_value> fields;
  };

  class test_vector_generator {
    const schema_set& schemas_;
    const test_value_generator& value_gen_;

  public:
    test_vector_generator(const schema_set& schemas,
                          const test_value_generator& value_gen);

    std::vector<test_vector>
    generate(const qname& element_name) const;
  };

} // namespace xb
