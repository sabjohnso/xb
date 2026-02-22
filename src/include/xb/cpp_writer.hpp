#pragma once

#include <xb/cpp_code.hpp>

#include <string>

namespace xb {

  class cpp_writer {
  public:
    std::string
    write(const cpp_file& file) const;
  };

} // namespace xb
