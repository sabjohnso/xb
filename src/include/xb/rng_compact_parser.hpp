#pragma once

#include <xb/rng_pattern.hpp>

#include <string>

namespace xb {

  class rng_compact_parser {
  public:
    rng::pattern
    parse(const std::string& source);
  };

} // namespace xb
