#pragma once

#include <xb/day_time_duration.hpp>
#include <xb/year_month_duration.hpp>

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class duration {
    bool negative_ = false;
    int32_t total_months_ = 0;
    int64_t total_seconds_ = 0;
    int32_t nanoseconds_ = 0;

  public:
    duration() = default;
    explicit duration(std::string_view str);

    std::string
    to_string() const;
    bool
    is_zero() const;
    bool
    is_negative() const;

    year_month_duration
    year_month_part() const;
    day_time_duration
    day_time_part() const;

    int32_t
    years() const;
    int32_t
    months() const;
    int64_t
    days() const;
    int32_t
    hours() const;
    int32_t
    minutes() const;
    int32_t
    seconds() const;
    int32_t
    nanoseconds() const;

    duration
    operator-() const;

    bool
    operator==(const duration& other) const;

    friend std::ostream&
    operator<<(std::ostream& os, const duration& d) {
      return os << d.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::duration> {
  std::size_t
  operator()(const xb::duration& d) const noexcept {
    return std::hash<std::string>{}(d.to_string());
  }
};
