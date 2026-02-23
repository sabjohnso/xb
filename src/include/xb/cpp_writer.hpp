#pragma once

#include <xb/cpp_code.hpp>

#include <string>

namespace xb {

  struct write_options {
    file_kind kind = file_kind::header;
  };

  class cpp_writer {
  public:
    std::string
    write(const cpp_file& file) const;

    std::string
    write(const cpp_file& file, write_options opts) const;
  };

} // namespace xb
