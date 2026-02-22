#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xb {

  struct facet_set {
    std::vector<std::string> enumeration;
    std::optional<std::string> pattern;
    std::optional<std::string> min_inclusive;
    std::optional<std::string> max_inclusive;
    std::optional<std::string> min_exclusive;
    std::optional<std::string> max_exclusive;
    std::optional<std::size_t> length;
    std::optional<std::size_t> min_length;
    std::optional<std::size_t> max_length;
    std::optional<std::size_t> total_digits;
    std::optional<std::size_t> fraction_digits;

    bool
    operator==(const facet_set&) const = default;
  };

} // namespace xb
