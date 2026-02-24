#pragma once

#include <string>

namespace xb {

  struct assertion {
    std::string test;

    bool
    operator==(const assertion&) const = default;
  };

} // namespace xb
