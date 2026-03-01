#pragma once

#include <xb/schematron_model.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class schematron_parser {
  public:
    schematron::schema
    parse(xml_reader& reader);
  };

} // namespace xb
