#pragma once

#include <xb/schema.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class schema_parser {
  public:
    schema
    parse(xml_reader& reader);
  };

} // namespace xb
