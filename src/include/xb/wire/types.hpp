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

  // --- parse_result: frame parser outcome ----------------------------------

  enum class parse_result {
    ok,        ///< All layers parsed, all messages delivered.
    truncated, ///< Buffer too small for next layer or message.
    mismatch,  ///< Match condition failed (wrong protocol).
    empty,     ///< No messages (e.g., message_count == 0).
  };

  struct parse_error {
    parse_result result;
    unsigned layer_index;
    std::size_t byte_offset;
  };

} // namespace xb::wire
