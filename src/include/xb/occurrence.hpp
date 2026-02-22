#pragma once

#include <xb/schema_fwd.hpp>

#include <cstddef>

namespace xb {

  struct occurrence {
    std::size_t min_occurs = 1;
    std::size_t max_occurs = 1;

    bool
    is_unbounded() const {
      return max_occurs == unbounded;
    }

    bool
    operator==(const occurrence&) const = default;
  };

} // namespace xb
