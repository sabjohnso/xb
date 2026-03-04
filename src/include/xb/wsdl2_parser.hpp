#pragma once

#include <xb/wsdl2_model.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class wsdl2_parser {
  public:
    wsdl2::description
    parse(xml_reader& reader);
  };

} // namespace xb
