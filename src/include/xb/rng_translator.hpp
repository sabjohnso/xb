#pragma once

#include <xb/rng_pattern.hpp>
#include <xb/schema_set.hpp>

namespace xb {

  schema_set
  rng_translate(const rng::pattern& simplified);

} // namespace xb
