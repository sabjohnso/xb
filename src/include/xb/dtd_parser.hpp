#pragma once

#include <xb/dtd_model.hpp>

#include <string>

namespace xb {

  class dtd_parser {
  public:
    dtd::document
    parse(const std::string& source);
  };

} // namespace xb
