#pragma once

#include <xb/rng_pattern.hpp>
#include <xb/schema_set.hpp>

namespace xb {

  rng::pattern
  xsd_to_rng(const schema_set& schemas);

} // namespace xb
