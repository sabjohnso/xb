#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace xb {

  class integer {
  public:
    enum class sign_type : uint8_t { positive, negative };

  private:
    sign_type sign_ = sign_type::positive;
    std::vector<uint32_t> magnitude_;

  public:
    integer() = default;
    explicit integer(int64_t value);
    explicit integer(uint64_t value);
    explicit integer(std::string_view str);

    std::string
    to_string() const;
    bool
    is_zero() const;
    sign_type
    sign() const;

    integer
    operator-() const;
    integer
    operator+() const;

    friend integer
    operator+(const integer& a, const integer& b);
    friend integer
    operator-(const integer& a, const integer& b);
    friend integer
    operator*(const integer& a, const integer& b);
    friend integer
    operator/(const integer& a, const integer& b);
    friend integer
    operator%(const integer& a, const integer& b);

    integer&
    operator+=(const integer& other);
    integer&
    operator-=(const integer& other);
    integer&
    operator*=(const integer& other);
    integer&
    operator/=(const integer& other);
    integer&
    operator%=(const integer& other);

    std::strong_ordering
    operator<=>(const integer& other) const;
    bool
    operator==(const integer& other) const;

    explicit
    operator int64_t() const;
    explicit
    operator uint64_t() const;
    explicit
    operator double() const;

    friend std::ostream&
    operator<<(std::ostream& os, const integer& i) {
      return os << i.to_string();
    }
  };

} // namespace xb

template <>
struct std::hash<xb::integer> {
  std::size_t
  operator()(const xb::integer& i) const noexcept {
    std::size_t seed = std::hash<int>{}(static_cast<int>(i.sign()));
    std::string s = i.to_string();
    seed ^=
        std::hash<std::string>{}(s) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};
