#pragma once

#include <xb/schema.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class schema_parser {
  public:
    // Advances through the reader to find the <xs:schema> root element,
    // then parses it.
    schema
    parse(xml_reader& reader);

    // Parses from a reader already positioned at an <xs:schema> start element.
    // Use this for mid-stream parsing (e.g., embedded schemas in WSDL <types>).
    schema
    parse_at_element(xml_reader& reader);
  };

} // namespace xb
