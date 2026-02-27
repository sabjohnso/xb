#pragma once

#include <cstddef>
#include <cstdint>

namespace xb {

  enum class compositor_kind { sequence, choice, all, interleave };

  enum class derivation_method { restriction, extension };

  enum class simple_type_variety { atomic, list, union_type };

  enum class content_kind { empty, simple, element_only, mixed };

  enum class wildcard_ns_constraint { any, other, enumerated };

  enum class process_contents { strict, lax, skip };

  enum class open_content_mode { none, interleave, suffix };

  inline constexpr std::size_t unbounded = SIZE_MAX;

} // namespace xb
