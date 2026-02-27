#pragma once

#include <xb/rng_pattern.hpp>

#include <functional>
#include <string>

namespace xb {

  using rng_file_resolver = std::function<std::string(const std::string& href)>;

  rng::pattern
  rng_simplify(rng::pattern input, const rng_file_resolver& resolver = {});

} // namespace xb
