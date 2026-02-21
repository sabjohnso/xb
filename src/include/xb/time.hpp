#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class time {
    uint8_t hour_ = 0;
    uint8_t minute_ = 0;
    uint8_t second_ = 0;
    int32_t nanosecond_ = 0;
    std::optional<int16_t> tz_offset_minutes_;

  public:
    time() = default;
    explicit time(std::string_view str);

    std::string
    to_string() const;
    uint8_t
    hour() const;
    uint8_t
    minute() const;
    uint8_t
    second() const;
    int32_t
    nanosecond() const;
    bool
    has_timezone() const;
    std::optional<int16_t>
    tz_offset_minutes() const;

    bool
    operator==(const time& other) const;

    friend std::ostream&
    operator<<(std::ostream& os, const time& t) {
      return os << t.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::time> {
  std::size_t
  operator()(const xb::time& t) const noexcept {
    return std::hash<std::string>{}(t.to_string());
  }
};
