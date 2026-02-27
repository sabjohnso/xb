#pragma once

#include <xb/rng_pattern.hpp>
#include <xb/xml_reader.hpp>

namespace xb {

  class rng_xml_parser {
  public:
    rng::pattern
    parse(xml_reader& reader);
  };

} // namespace xb
