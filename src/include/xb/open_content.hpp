#pragma once

#include <xb/schema_fwd.hpp>
#include <xb/wildcard.hpp>

namespace xb {

  struct open_content {
    open_content_mode mode = open_content_mode::interleave;
    wildcard wc;

    bool
    operator==(const open_content&) const = default;
  };

} // namespace xb
