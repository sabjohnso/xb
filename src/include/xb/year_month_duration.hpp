#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class year_month_duration {
    bool negative_ = false;
    int32_t total_months_ = 0;

  public:
    year_month_duration() = default;
    explicit year_month_duration(std::string_view str);
    year_month_duration(int32_t years, int32_t months);

    std::string
    to_string() const;
    bool
    is_zero() const;
    bool
    is_negative() const;
    int32_t
    years() const;
    int32_t
    months() const;
    int32_t
    total_months() const;

    year_month_duration
    operator-() const;

    friend year_month_duration
    operator+(const year_month_duration& a, const year_month_duration& b);
    friend year_month_duration
    operator-(const year_month_duration& a, const year_month_duration& b);
    friend year_month_duration
    operator*(const year_month_duration& a, int32_t n);
    friend year_month_duration
    operator*(int32_t n, const year_month_duration& a);

    year_month_duration&
    operator+=(const year_month_duration& other);
    year_month_duration&
    operator-=(const year_month_duration& other);
    year_month_duration&
    operator*=(int32_t n);

    std::strong_ordering
    operator<=>(const year_month_duration& other) const;
    bool
    operator==(const year_month_duration& other) const;

    explicit
    operator std::chrono::months() const;
    explicit year_month_duration(std::chrono::months m);

    friend std::ostream&
    operator<<(std::ostream& os, const year_month_duration& d) {
      return os << d.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::year_month_duration> {
  std::size_t
  operator()(const xb::year_month_duration& d) const noexcept {
    std::size_t seed = std::hash<bool>{}(d.is_negative());
    seed ^= std::hash<int32_t>{}(d.total_months()) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};
