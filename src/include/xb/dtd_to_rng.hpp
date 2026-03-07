#pragma once

#include <xb/dtd_model.hpp>
#include <xb/rng_pattern.hpp>

namespace xb {

  rng::pattern
  dtd_to_rng(const dtd::document& doc);

} // namespace xb
