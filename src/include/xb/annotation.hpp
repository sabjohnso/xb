#pragma once

#include <string>

namespace xb {

  struct annotation {
    std::string documentation;

    bool
    operator==(const annotation&) const = default;
  };

} // namespace xb
