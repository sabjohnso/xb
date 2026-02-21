#include <xb/decimal.hpp>

#include <cstdio>
#include <stdexcept>
#include <string>

namespace xb {

  namespace {

    // Compute 10^n as an integer.
    integer
    pow10(int n) {
      integer result(int64_t{1});
      integer ten(int64_t{10});
      for (int i = 0; i < n; ++i) {
        result = result * ten;
      }
      return result;
    }

    // Remove trailing decimal zeros from significand, adjusting exponent.
    // For zero, normalize to significand=0, exponent=0.
    void
    normalize(integer& significand, int& exponent) {
      if (significand.is_zero()) {
        exponent = 0;
        return;
      }

      integer ten(int64_t{10});
      while (!significand.is_zero()) {
        auto rem = significand % ten;
        if (!rem.is_zero()) { break; }
        significand = significand / ten;
        ++exponent;
      }
    }

    // Align two decimal values to the same exponent (the smaller one).
    // Returns the aligned significands and the common exponent.
    struct aligned {
      integer a;
      integer b;
      int exponent;
    };

    aligned
    align_exponents(const integer& sig_a, int exp_a, const integer& sig_b,
                    int exp_b) {
      if (exp_a == exp_b) { return {sig_a, sig_b, exp_a}; }
      if (exp_a < exp_b) {
        return {sig_a, sig_b * pow10(exp_b - exp_a), exp_a};
      }
      return {sig_a * pow10(exp_a - exp_b), sig_b, exp_b};
    }

  } // namespace

  decimal::decimal(std::string_view str) {
    if (str.empty()) { throw std::invalid_argument("decimal: empty string"); }

    auto dot_pos = str.find('.');
    if (dot_pos == std::string_view::npos) {
      significand_ = integer(str);
      exponent_ = 0;
    } else {
      if (str.find('.', dot_pos + 1) != std::string_view::npos) {
        throw std::invalid_argument("decimal: multiple decimal points in '" +
                                    std::string(str) + "'");
      }

      std::string_view before = str.substr(0, dot_pos);
      std::string_view after = str.substr(dot_pos + 1);

      if (before.empty() && after.empty()) {
        throw std::invalid_argument("decimal: no digits in '" +
                                    std::string(str) + "'");
      }

      bool negative = false;
      std::string_view before_trimmed = before;
      if (!before_trimmed.empty() &&
          (before_trimmed[0] == '-' || before_trimmed[0] == '+')) {
        if (before_trimmed[0] == '-') { negative = true; }
        before_trimmed = before_trimmed.substr(1);
      }

      if (before_trimmed.empty() && after.empty()) {
        throw std::invalid_argument("decimal: no digits in '" +
                                    std::string(str) + "'");
      }

      for (char c : before_trimmed) {
        if (c < '0' || c > '9') {
          throw std::invalid_argument("decimal: invalid character in '" +
                                      std::string(str) + "'");
        }
      }
      for (char c : after) {
        if (c < '0' || c > '9') {
          throw std::invalid_argument("decimal: invalid character in '" +
                                      std::string(str) + "'");
        }
      }

      std::string digits = std::string(before_trimmed) + std::string(after);
      if (digits.empty()) {
        throw std::invalid_argument("decimal: no digits in '" +
                                    std::string(str) + "'");
      }

      std::string sig_str = (negative ? "-" : "") + digits;
      significand_ = integer(sig_str);
      exponent_ = -static_cast<int>(after.size());
    }

    normalize(significand_, exponent_);
  }

  decimal::decimal(double value) {
    if (value == 0.0) { return; }
    // Use fixed-point notation to avoid scientific notation that the
    // string parser cannot handle.  %.17f always produces a plain decimal
    // string (no 'e' exponent), at the cost of potentially many digits
    // for very large values.
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%.17f", value);
    *this = decimal(std::string_view(buf));
  }

  std::string
  decimal::to_string() const {
    if (significand_.is_zero()) { return "0.0"; }

    std::string digits = significand_.to_string();
    bool negative = false;
    if (digits[0] == '-') {
      negative = true;
      digits.erase(digits.begin());
    }

    if (exponent_ >= 0) {
      digits.append(static_cast<std::size_t>(exponent_), '0');
      if (negative) { return "-" + digits + ".0"; }
      return digits + ".0";
    }

    int decimal_places = -exponent_;
    if (static_cast<int>(digits.size()) <= decimal_places) {
      std::string result = "0.";
      result.append(static_cast<std::size_t>(decimal_places) - digits.size(),
                    '0');
      result += digits;
      if (negative) { return "-" + result; }
      return result;
    }

    std::string result =
        digits.substr(0, digits.size() -
                             static_cast<std::size_t>(decimal_places)) +
        "." +
        digits.substr(digits.size() - static_cast<std::size_t>(decimal_places));
    if (negative) { return "-" + result; }
    return result;
  }

  bool
  decimal::is_zero() const {
    return significand_.is_zero();
  }

  decimal
  decimal::operator-() const {
    decimal result;
    result.significand_ = -significand_;
    result.exponent_ = exponent_;
    if (result.significand_.is_zero()) { result.exponent_ = 0; }
    return result;
  }

  decimal
  operator+(const decimal& a, const decimal& b) {
    auto [sa, sb, exp] = align_exponents(a.significand_, a.exponent_,
                                         b.significand_, b.exponent_);
    decimal result;
    result.significand_ = sa + sb;
    result.exponent_ = exp;
    normalize(result.significand_, result.exponent_);
    return result;
  }

  decimal
  operator-(const decimal& a, const decimal& b) {
    return a + (-b);
  }

  decimal
  operator*(const decimal& a, const decimal& b) {
    decimal result;
    result.significand_ = a.significand_ * b.significand_;
    result.exponent_ = a.exponent_ + b.exponent_;
    normalize(result.significand_, result.exponent_);
    return result;
  }

  decimal
  operator/(const decimal& a, const decimal& b) {
    if (b.significand_.is_zero()) {
      throw std::domain_error("decimal: division by zero");
    }
    if (a.significand_.is_zero()) { return decimal(); }

    // Scale dividend significand by 10^precision, then integer divide.
    int precision = decimal::default_division_precision;
    integer scaled = a.significand_ * pow10(precision);
    integer quotient = scaled / b.significand_;
    int new_exponent = a.exponent_ - b.exponent_ - precision;

    decimal result;
    result.significand_ = quotient;
    result.exponent_ = new_exponent;
    normalize(result.significand_, result.exponent_);
    return result;
  }

  decimal&
  decimal::operator+=(const decimal& other) {
    *this = *this + other;
    return *this;
  }

  decimal&
  decimal::operator-=(const decimal& other) {
    *this = *this - other;
    return *this;
  }

  decimal&
  decimal::operator*=(const decimal& other) {
    *this = *this * other;
    return *this;
  }

  decimal&
  decimal::operator/=(const decimal& other) {
    *this = *this / other;
    return *this;
  }

  std::strong_ordering
  decimal::operator<=>(const decimal& other) const {
    auto [sa, sb, exp] = align_exponents(significand_, exponent_,
                                         other.significand_, other.exponent_);
    return sa <=> sb;
  }

  bool
  decimal::operator==(const decimal& other) const {
    // After normalization, equal values have identical representation.
    return significand_ == other.significand_ && exponent_ == other.exponent_;
  }

  decimal::
  operator double() const {
    if (significand_.is_zero()) { return 0.0; }
    double sig = static_cast<double>(significand_);
    double exp = 1.0;
    if (exponent_ > 0) {
      for (int i = 0; i < exponent_; ++i) {
        exp *= 10.0;
      }
    } else {
      for (int i = 0; i < -exponent_; ++i) {
        exp /= 10.0;
      }
    }
    return sig * exp;
  }

} // namespace xb
