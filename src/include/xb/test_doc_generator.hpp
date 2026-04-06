#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/test_vector_generator.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace xb {

  class test_doc_generator {
    const schema_set& schemas_;

  public:
    explicit test_doc_generator(const schema_set& schemas);

    void
    generate(
        const qname& element_name,
        std::function<void(const std::string& xml, const std::string& label)>
            sink,
        const test_vector_options& opts = {}) const;

    std::size_t
    count(const qname& element_name,
          const test_vector_options& opts = {}) const;
  };

} // namespace xb
