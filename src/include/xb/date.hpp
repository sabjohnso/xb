#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class date {
    int32_t year_ = 1;
    uint8_t month_ = 1;
    uint8_t day_ = 1;
    std::optional<int16_t> tz_offset_minutes_;

  public:
    date() = default;
    explicit date(std::string_view str);
    date(int32_t year, uint8_t month, uint8_t day,
         std::optional<int16_t> tz_offset_minutes = std::nullopt);

    std::string
    to_string() const;
    int32_t
    year() const;
    uint8_t
    month() const;
    uint8_t
    day() const;
    bool
    has_timezone() const;
    std::optional<int16_t>
    tz_offset_minutes() const;

    bool
    operator==(const date& other) const;

    explicit
    operator std::chrono::year_month_day() const;
    explicit date(std::chrono::year_month_day ymd);

    friend std::ostream&
    operator<<(std::ostream& os, const date& d) {
      return os << d.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::date> {
  std::size_t
  operator()(const xb::date& d) const noexcept {
    return std::hash<std::string>{}(d.to_string());
  }
};
