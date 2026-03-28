#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>

#include <string>
#include <vector>

namespace xb {

  struct test_value {
    std::string xml_text;
    std::string reason;
  };

  struct test_value_set {
    qname type_name;
    std::vector<test_value> values;
  };

  class test_value_generator {
    const schema_set& schemas_;

  public:
    explicit test_value_generator(const schema_set& schemas);

    test_value_set
    generate(const qname& type_name) const;
    test_value_set
    generate(const simple_type& st) const;
  };

} // namespace xb
