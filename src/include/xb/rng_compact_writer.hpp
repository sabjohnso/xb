#pragma once

#include <xb/rng_pattern.hpp>

#include <string>

namespace xb {

  std::string
  rng_compact_write(const rng::pattern& p);

  std::string
  rng_compact_write(const rng::pattern& p, int indent);

} // namespace xb
