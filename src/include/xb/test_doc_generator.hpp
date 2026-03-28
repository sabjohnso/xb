#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace xb {

  class test_doc_generator {
    const schema_set& schemas_;

  public:
    explicit test_doc_generator(const schema_set& schemas);

    // Generate all test vectors as XML documents.
    // The callback receives (xml_document_string, label) for each vector.
    void
    generate(
        const qname& element_name,
        std::function<void(const std::string& xml, const std::string& label)>
            sink) const;

    // Return the number of vectors that would be generated.
    std::size_t
    count(const qname& element_name) const;
  };

} // namespace xb
