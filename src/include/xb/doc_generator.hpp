#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/xml_writer.hpp>

#include <cstddef>

namespace xb {

  struct doc_generator_options {
    bool populate_optional = false;
    std::size_t max_depth = 20;
  };

  class doc_generator {
    const schema_set& schemas_;
    doc_generator_options options_;

  public:
    explicit doc_generator(const schema_set& schemas,
                           doc_generator_options options = {});

    void
    generate(const qname& element_name, xml_writer& writer) const;
  };

} // namespace xb
