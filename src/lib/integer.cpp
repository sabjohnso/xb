#include <xb/integer.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace xb {

  namespace {

    // Multiply magnitude by 10 and add a digit.
    void
    magnitude_mul10_add(std::vector<uint32_t>& mag, uint32_t digit) {
      uint64_t carry = digit;
      for (auto& limb : mag) {
        uint64_t product = static_cast<uint64_t>(limb) * 10 + carry;
        limb = static_cast<uint32_t>(product);
        carry = product >> 32;
      }
      if (carry != 0) { mag.push_back(static_cast<uint32_t>(carry)); }
    }

    // Add two magnitudes, return result.
    std::vector<uint32_t>
    magnitude_add(const std::vector<uint32_t>& a,
                  const std::vector<uint32_t>& b) {
      std::vector<uint32_t> result;
      std::size_t len = std::max(a.size(), b.size());
      result.reserve(len + 1);
      uint64_t carry = 0;
      for (std::size_t i = 0; i < len; ++i) {
        uint64_t sum = carry;
        if (i < a.size()) { sum += a[i]; }
        if (i < b.size()) { sum += b[i]; }
        result.push_back(static_cast<uint32_t>(sum));
        carry = sum >> 32;
      }
      if (carry != 0) { result.push_back(static_cast<uint32_t>(carry)); }
      return result;
    }

    // Compare two magnitudes (unsigned).
    std::strong_ordering
    magnitude_compare(const std::vector<uint32_t>& a,
                      const std::vector<uint32_t>& b) {
      if (a.size() != b.size()) { return a.size() <=> b.size(); }
      for (auto i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) { return a[i] <=> b[i]; }
      }
      return std::strong_ordering::equal;
    }

    // Subtract b from a where |a| >= |b|.  Caller must ensure this.
    std::vector<uint32_t>
    magnitude_sub(const std::vector<uint32_t>& a,
                  const std::vector<uint32_t>& b) {
      std::vector<uint32_t> result;
      result.reserve(a.size());
      uint64_t borrow = 0;
      for (std::size_t i = 0; i < a.size(); ++i) {
        uint64_t ai = a[i];
        uint64_t bi = (i < b.size()) ? b[i] : 0;
        uint64_t diff = ai - bi - borrow;
        result.push_back(static_cast<uint32_t>(diff));
        borrow = (ai < bi + borrow) ? 1 : 0;
      }
      // Remove trailing zeros.
      while (!result.empty() && result.back() == 0) {
        result.pop_back();
      }
      return result;
    }

    // Schoolbook O(n*m) multiplication.
    std::vector<uint32_t>
    magnitude_mul(const std::vector<uint32_t>& a,
                  const std::vector<uint32_t>& b) {
      if (a.empty() || b.empty()) { return {}; }
      std::vector<uint32_t> result(a.size() + b.size(), 0);
      for (std::size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
          uint64_t product =
              static_cast<uint64_t>(a[i]) * b[j] + result[i + j] + carry;
          result[i + j] = static_cast<uint32_t>(product);
          carry = product >> 32;
        }
        result[i + b.size()] += static_cast<uint32_t>(carry);
      }
      while (!result.empty() && result.back() == 0) {
        result.pop_back();
      }
      return result;
    }

    // Shift-and-subtract binary long division.
    // Returns {quotient, remainder}.
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
    magnitude_divmod(const std::vector<uint32_t>& a,
                     const std::vector<uint32_t>& b) {
      if (b.empty()) { throw std::domain_error("integer: division by zero"); }

      auto cmp = magnitude_compare(a, b);
      if (cmp == std::strong_ordering::less) { return {{}, a}; }
      if (cmp == std::strong_ordering::equal) { return {{1}, {}}; }

      // Total bits in dividend.
      std::size_t a_bits = (a.size() - 1) * 32;
      uint32_t top = a.back();
      while (top != 0) {
        ++a_bits;
        top >>= 1;
      }

      std::vector<uint32_t> quotient;
      std::vector<uint32_t> remainder;

      for (auto i = a_bits; i-- > 0;) {
        // Left-shift remainder by 1 bit.
        uint32_t carry = 0;
        for (auto& limb : remainder) {
          uint32_t new_carry = limb >> 31;
          limb = (limb << 1) | carry;
          carry = new_carry;
        }
        if (carry != 0) { remainder.push_back(carry); }

        // Set the lowest bit of remainder to bit i of a.
        uint32_t a_bit = (a[i / 32] >> (i % 32)) & 1;
        if (remainder.empty() && a_bit != 0) {
          remainder.push_back(a_bit);
        } else if (!remainder.empty()) {
          remainder[0] |= a_bit;
        }

        // If remainder >= b, subtract b from remainder and set quotient bit.
        if (magnitude_compare(remainder, b) != std::strong_ordering::less) {
          remainder = magnitude_sub(remainder, b);
          std::size_t q_limb = i / 32;
          if (quotient.size() <= q_limb) { quotient.resize(q_limb + 1, 0); }
          quotient[q_limb] |= (uint32_t{1} << (i % 32));
        }
      }

      while (!quotient.empty() && quotient.back() == 0) {
        quotient.pop_back();
      }
      return {quotient, remainder};
    }

  } // namespace

  integer::integer(std::string_view str) {
    if (str.empty()) { throw std::invalid_argument("integer: empty string"); }

    std::size_t pos = 0;
    if (str[0] == '-' || str[0] == '+') {
      if (str[0] == '-') { sign_ = sign_type::negative; }
      pos = 1;
    }

    if (pos == str.size()) {
      throw std::invalid_argument("integer: no digits in '" + std::string(str) +
                                  "'");
    }

    for (std::size_t i = pos; i < str.size(); ++i) {
      if (str[i] < '0' || str[i] > '9') {
        throw std::invalid_argument("integer: invalid character in '" +
                                    std::string(str) + "'");
      }
      magnitude_mul10_add(magnitude_, static_cast<uint32_t>(str[i] - '0'));
    }

    // Remove trailing zero limbs (from leading zeros in input).
    while (!magnitude_.empty() && magnitude_.back() == 0) {
      magnitude_.pop_back();
    }

    // Normalize negative zero.
    if (magnitude_.empty()) { sign_ = sign_type::positive; }
  }

  integer::integer(uint64_t value) {
    if (value == 0) { return; }
    magnitude_.push_back(static_cast<uint32_t>(value));
    if (uint32_t high = static_cast<uint32_t>(value >> 32)) {
      magnitude_.push_back(high);
    }
  }

  integer::integer(int64_t value) {
    if (value == 0) { return; }

    if (value < 0) {
      sign_ = sign_type::negative;
      // Handle INT64_MIN: cast to uint64_t before negating to avoid UB.
      uint64_t abs_val = static_cast<uint64_t>(-(value + 1)) + 1;
      magnitude_.push_back(static_cast<uint32_t>(abs_val));
      if (uint32_t high = static_cast<uint32_t>(abs_val >> 32)) {
        magnitude_.push_back(high);
      }
    } else {
      auto uval = static_cast<uint64_t>(value);
      magnitude_.push_back(static_cast<uint32_t>(uval));
      if (uint32_t high = static_cast<uint32_t>(uval >> 32)) {
        magnitude_.push_back(high);
      }
    }
  }

  std::string
  integer::to_string() const {
    if (magnitude_.empty()) { return "0"; }

    // Repeated divide-by-10 on a copy of the magnitude.
    auto mag = magnitude_;
    std::string digits;

    while (!mag.empty()) {
      uint64_t remainder = 0;
      for (auto it = mag.rbegin(); it != mag.rend(); ++it) {
        uint64_t cur = (remainder << 32) | *it;
        *it = static_cast<uint32_t>(cur / 10);
        remainder = cur % 10;
      }
      digits.push_back(static_cast<char>('0' + remainder));
      while (!mag.empty() && mag.back() == 0) {
        mag.pop_back();
      }
    }

    if (sign_ == sign_type::negative) { digits.push_back('-'); }
    std::reverse(digits.begin(), digits.end());
    return digits;
  }

  bool
  integer::is_zero() const {
    return magnitude_.empty();
  }

  integer::sign_type
  integer::sign() const {
    return sign_;
  }

  integer
  integer::operator-() const {
    if (magnitude_.empty()) { return *this; }
    integer result = *this;
    result.sign_ = (sign_ == sign_type::positive) ? sign_type::negative
                                                  : sign_type::positive;
    return result;
  }

  integer
  integer::operator+() const {
    return *this;
  }

  std::strong_ordering
  integer::operator<=>(const integer& other) const {
    // Both zero.
    if (magnitude_.empty() && other.magnitude_.empty()) {
      return std::strong_ordering::equal;
    }
    // Different signs.
    if (sign_ != other.sign_) {
      return sign_ == sign_type::positive ? std::strong_ordering::greater
                                          : std::strong_ordering::less;
    }
    // Same sign: compare magnitudes (reversed for negative).
    auto mag_cmp = magnitude_compare(magnitude_, other.magnitude_);
    if (sign_ == sign_type::negative) {
      if (mag_cmp == std::strong_ordering::less) {
        return std::strong_ordering::greater;
      }
      if (mag_cmp == std::strong_ordering::greater) {
        return std::strong_ordering::less;
      }
    }
    return mag_cmp;
  }

  bool
  integer::operator==(const integer& other) const {
    return sign_ == other.sign_ && magnitude_ == other.magnitude_;
  }

  integer
  operator+(const integer& a, const integer& b) {
    using sign_type = integer::sign_type;

    // Same sign: add magnitudes, keep sign.
    if (a.sign_ == b.sign_) {
      integer result;
      result.sign_ = a.sign_;
      result.magnitude_ = magnitude_add(a.magnitude_, b.magnitude_);
      if (result.magnitude_.empty()) { result.sign_ = sign_type::positive; }
      return result;
    }

    // Different signs: subtract smaller magnitude from larger.
    auto cmp = magnitude_compare(a.magnitude_, b.magnitude_);
    if (cmp == std::strong_ordering::equal) { return integer(); }

    integer result;
    if (cmp == std::strong_ordering::greater) {
      result.magnitude_ = magnitude_sub(a.magnitude_, b.magnitude_);
      result.sign_ = a.sign_;
    } else {
      result.magnitude_ = magnitude_sub(b.magnitude_, a.magnitude_);
      result.sign_ = b.sign_;
    }
    if (result.magnitude_.empty()) { result.sign_ = sign_type::positive; }
    return result;
  }

  integer
  operator-(const integer& a, const integer& b) {
    return a + (-b);
  }

  integer
  operator*(const integer& a, const integer& b) {
    using sign_type = integer::sign_type;
    if (a.magnitude_.empty() || b.magnitude_.empty()) { return integer(); }

    integer result;
    result.magnitude_ = magnitude_mul(a.magnitude_, b.magnitude_);
    result.sign_ =
        (a.sign_ == b.sign_) ? sign_type::positive : sign_type::negative;
    if (result.magnitude_.empty()) { result.sign_ = sign_type::positive; }
    return result;
  }

  integer
  operator/(const integer& a, const integer& b) {
    using sign_type = integer::sign_type;
    auto [q, r] = magnitude_divmod(a.magnitude_, b.magnitude_);

    integer result;
    result.magnitude_ = std::move(q);
    if (!result.magnitude_.empty()) {
      result.sign_ =
          (a.sign_ == b.sign_) ? sign_type::positive : sign_type::negative;
    }
    return result;
  }

  integer
  operator%(const integer& a, const integer& b) {
    auto [q, r] = magnitude_divmod(a.magnitude_, b.magnitude_);

    integer result;
    result.magnitude_ = std::move(r);
    if (!result.magnitude_.empty()) {
      result.sign_ = a.sign_; // Remainder has same sign as dividend.
    }
    return result;
  }

  integer&
  integer::operator+=(const integer& other) {
    *this = *this + other;
    return *this;
  }

  integer&
  integer::operator-=(const integer& other) {
    *this = *this - other;
    return *this;
  }

  integer&
  integer::operator*=(const integer& other) {
    *this = *this * other;
    return *this;
  }

  integer&
  integer::operator/=(const integer& other) {
    *this = *this / other;
    return *this;
  }

  integer&
  integer::operator%=(const integer& other) {
    *this = *this % other;
    return *this;
  }

  integer::
  operator int64_t() const {
    if (magnitude_.empty()) { return 0; }

    // Check if the value fits in int64_t.
    uint64_t abs_val = 0;
    if (magnitude_.size() > 2) {
      throw std::overflow_error("integer: value too large for int64_t");
    }
    abs_val = magnitude_[0];
    if (magnitude_.size() == 2) {
      abs_val |= static_cast<uint64_t>(magnitude_[1]) << 32;
    }

    if (sign_ == sign_type::negative) {
      // INT64_MIN = -9223372036854775808, abs = 9223372036854775808
      if (abs_val > static_cast<uint64_t>(INT64_MAX) + 1) {
        throw std::overflow_error("integer: value too large for int64_t");
      }
      return -static_cast<int64_t>(abs_val - 1) - 1;
    }

    if (abs_val > static_cast<uint64_t>(INT64_MAX)) {
      throw std::overflow_error("integer: value too large for int64_t");
    }
    return static_cast<int64_t>(abs_val);
  }

  integer::
  operator uint64_t() const {
    if (magnitude_.empty()) { return 0; }

    if (sign_ == sign_type::negative) {
      throw std::overflow_error("integer: negative value cannot convert to "
                                "uint64_t");
    }
    if (magnitude_.size() > 2) {
      throw std::overflow_error("integer: value too large for uint64_t");
    }

    uint64_t val = magnitude_[0];
    if (magnitude_.size() == 2) {
      val |= static_cast<uint64_t>(magnitude_[1]) << 32;
    }
    return val;
  }

  integer::
  operator double() const {
    if (magnitude_.empty()) { return 0.0; }

    double result = 0.0;
    double base = 1.0;
    for (auto limb : magnitude_) {
      result += static_cast<double>(limb) * base;
      base *= 4294967296.0; // 2^32
    }
    if (sign_ == sign_type::negative) { result = -result; }
    return result;
  }

} // namespace xb
