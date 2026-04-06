#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/test_vector_generator.hpp>

#include <iosfwd>

namespace xb {

  class test_harness_generator {
    const schema_set& schemas_;

  public:
    explicit test_harness_generator(const schema_set& schemas);

    void
    generate(const qname& element_name, std::ostream& out,
             const test_vector_options& opts = {}) const;
  };

} // namespace xb
