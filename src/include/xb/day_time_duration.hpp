#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class day_time_duration {
    bool negative_ = false;
    int64_t total_seconds_ = 0;
    int32_t nanoseconds_ = 0;

  public:
    day_time_duration() = default;
    explicit day_time_duration(std::string_view str);

    std::string
    to_string() const;
    bool
    is_zero() const;
    bool
    is_negative() const;
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

    day_time_duration
    operator-() const;

    friend day_time_duration
    operator+(const day_time_duration& a, const day_time_duration& b);
    friend day_time_duration
    operator-(const day_time_duration& a, const day_time_duration& b);
    friend day_time_duration
    operator*(const day_time_duration& a, int64_t n);
    friend day_time_duration
    operator*(int64_t n, const day_time_duration& a);

    day_time_duration&
    operator+=(const day_time_duration& other);
    day_time_duration&
    operator-=(const day_time_duration& other);
    day_time_duration&
    operator*=(int64_t n);

    std::strong_ordering
    operator<=>(const day_time_duration& other) const;
    bool
    operator==(const day_time_duration& other) const;

    explicit
    operator std::chrono::nanoseconds() const;
    explicit day_time_duration(std::chrono::nanoseconds ns);

    friend std::ostream&
    operator<<(std::ostream& os, const day_time_duration& d) {
      return os << d.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::day_time_duration> {
  std::size_t
  operator()(const xb::day_time_duration& d) const noexcept {
    std::size_t seed = std::hash<bool>{}(d.is_negative());
    seed ^= std::hash<int64_t>{}(d.days() * 86400 + d.hours() * 3600 +
                                 d.minutes() * 60 + d.seconds()) +
            0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int32_t>{}(d.nanoseconds()) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};
