#pragma once

#include <cstddef>
#include <span>

namespace xb::wire {

  // --- validation_level: compile-time validation policy --------------------
  //
  // Used as a non-type template parameter on buffer-backed types.
  // Ordered so that comparisons (>=, <) select the appropriate checks:
  //
  //   if constexpr (V >= validation_level::structural) { ... }
  //
  // Dead branches are eliminated by the language (if constexpr), not the
  // optimizer.

  enum class validation_level : int {
    discriminant = 0, // minimum: identify message type only
    structural = 1,   // type + cardinality checks
    full = 2,         // all XSD facets enforced
  };

  // --- buffer type aliases -------------------------------------------------

  using const_buffer = std::span<const std::byte>;
  using mutable_buffer = std::span<std::byte>;

} // namespace xb::wire
