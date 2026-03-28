#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>

#include <iosfwd>

namespace xb {

  class test_harness_generator {
    const schema_set& schemas_;

  public:
    explicit test_harness_generator(const schema_set& schemas);

    // Generate a Catch2 test source file for the given root element.
    void
    generate(const qname& element_name, std::ostream& out) const;
  };

} // namespace xb
