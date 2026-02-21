#pragma once

#include <xb/integer.hpp>

#include <compare>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace xb {

  class decimal {
    integer significand_;
    int exponent_ = 0;

  public:
    static constexpr int default_division_precision = 28;

    decimal() = default;
    explicit decimal(std::string_view str);
    explicit decimal(double value);

    std::string
    to_string() const;
    bool
    is_zero() const;

    decimal
    operator-() const;

    friend decimal
    operator+(const decimal& a, const decimal& b);
    friend decimal
    operator-(const decimal& a, const decimal& b);
    friend decimal
    operator*(const decimal& a, const decimal& b);
    friend decimal
    operator/(const decimal& a, const decimal& b);

    decimal&
    operator+=(const decimal& other);
    decimal&
    operator-=(const decimal& other);
    decimal&
    operator*=(const decimal& other);
    decimal&
    operator/=(const decimal& other);

    std::strong_ordering
    operator<=>(const decimal& other) const;
    bool
    operator==(const decimal& other) const;

    explicit
    operator double() const;

    friend std::ostream&
    operator<<(std::ostream& os, const decimal& d) {
      return os << d.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::decimal> {
  std::size_t
  operator()(const xb::decimal& d) const noexcept {
    return std::hash<std::string>{}(d.to_string());
  }
};
