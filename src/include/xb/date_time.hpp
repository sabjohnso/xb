#pragma once

#include <xb/date.hpp>
#include <xb/time.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class date_time {
    int32_t year_ = 1;
    uint8_t month_ = 1;
    uint8_t day_ = 1;
    uint8_t hour_ = 0;
    uint8_t minute_ = 0;
    uint8_t second_ = 0;
    int32_t nanosecond_ = 0;
    std::optional<int16_t> tz_offset_minutes_;

  public:
    date_time() = default;
    explicit date_time(std::string_view str);

    std::string
    to_string() const;
    int32_t
    year() const;
    uint8_t
    month() const;
    uint8_t
    day() const;
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

    xb::date
    date_part() const;
    xb::time
    time_part() const;

    bool
    operator==(const date_time& other) const;

    friend std::ostream&
    operator<<(std::ostream& os, const date_time& dt) {
      return os << dt.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::date_time> {
  std::size_t
  operator()(const xb::date_time& dt) const noexcept {
    return std::hash<std::string>{}(dt.to_string());
  }
};
