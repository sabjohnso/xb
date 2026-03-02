#pragma once

#include <xb/wsdl_model.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class wsdl_parser {
  public:
    wsdl::document
    parse(xml_reader& reader);
  };

} // namespace xb
