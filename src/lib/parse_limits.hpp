#pragma once

#include <cstddef>

namespace xb {

  /// Maximum decimal-digit count permitted when parsing arbitrary-precision
  /// numeric values from strings.  Exceeding this raises @c std::length_error
  /// before any limb storage is allocated.
  ///
  /// The cap is conservatively large — 4096 digits is roughly a 13 600-bit
  /// integer, well beyond any legitimate XSD value while still bounding the
  /// memory and CPU a single hostile element body can consume.
  inline constexpr std::size_t max_decimal_digits = 4096;

} // namespace xb
