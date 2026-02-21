#include <xb/year_month_duration.hpp>

#include <cstdlib>
#include <stdexcept>

namespace xb {

  namespace {

    struct parsed_ym {
      bool negative = false;
      int32_t total_months = 0;
    };

    parsed_ym
    parse_year_month_duration(std::string_view str) {
      if (str.empty()) {
        throw std::invalid_argument("year_month_duration: empty string");
      }

      parsed_ym result;
      std::size_t pos = 0;

      if (str[pos] == '-') {
        result.negative = true;
        ++pos;
      }

      if (pos >= str.size() || str[pos] != 'P') {
        throw std::invalid_argument("year_month_duration: expected 'P'");
      }
      ++pos;

      if (pos >= str.size()) {
        throw std::invalid_argument(
            "year_month_duration: expected component after 'P'");
      }

      // Must not contain 'T' (that's day_time_duration territory)
      if (str.find('T', pos) != std::string_view::npos) {
        throw std::invalid_argument(
            "year_month_duration: unexpected 'T' component");
      }

      // Must not contain 'D' (that's day_time_duration territory)
      if (str.find('D', pos) != std::string_view::npos) {
        throw std::invalid_argument(
            "year_month_duration: unexpected 'D' component");
      }

      bool found_any = false;
      int64_t years = 0;
      int64_t months = 0;

      // Parse optional years
      if (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        std::size_t num_start = pos;
        while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
          ++pos;
        }
        if (pos >= str.size()) {
          throw std::invalid_argument(
              "year_month_duration: expected 'Y' or 'M' after "
              "number");
        }
        auto num_str = str.substr(num_start, pos - num_start);
        int64_t value = 0;
        for (char c : num_str) {
          value = value * 10 + (c - '0');
        }

        if (str[pos] == 'Y') {
          years = value;
          found_any = true;
          ++pos;
        } else if (str[pos] == 'M') {
          months = value;
          found_any = true;
          ++pos;
        } else {
          throw std::invalid_argument(
              "year_month_duration: unexpected character");
        }
      }

      // Parse optional months (if years was parsed above)
      if (pos < str.size() && str[pos] >= '0' && str[pos] <= '9' && found_any &&
          months == 0) {
        std::size_t num_start = pos;
        while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
          ++pos;
        }
        if (pos >= str.size() || str[pos] != 'M') {
          throw std::invalid_argument(
              "year_month_duration: expected 'M' after number");
        }
        auto num_str = str.substr(num_start, pos - num_start);
        int64_t value = 0;
        for (char c : num_str) {
          value = value * 10 + (c - '0');
        }
        months = value;
        ++pos;
      }

      if (!found_any) {
        throw std::invalid_argument("year_month_duration: no components found");
      }

      if (pos != str.size()) {
        throw std::invalid_argument("year_month_duration: trailing characters");
      }

      result.total_months = static_cast<int32_t>(years * 12 + months);

      // Normalize negative zero
      if (result.total_months == 0) { result.negative = false; }

      return result;
    }

  } // namespace

  year_month_duration::year_month_duration(std::string_view str) {
    auto parsed = parse_year_month_duration(str);
    negative_ = parsed.negative;
    total_months_ = parsed.total_months;
  }

  year_month_duration::year_month_duration(int32_t years, int32_t months) {
    total_months_ = years * 12 + months;
    if (total_months_ == 0) { negative_ = false; }
  }

  std::string
  year_month_duration::to_string() const {
    std::string result;
    if (negative_) { result += '-'; }
    result += 'P';

    int32_t y = total_months_ / 12;
    int32_t m = total_months_ % 12;

    if (y > 0) {
      result += std::to_string(y);
      result += 'Y';
    }
    if (m > 0 || y == 0) {
      result += std::to_string(m);
      result += 'M';
    }
    return result;
  }

  bool
  year_month_duration::is_zero() const {
    return total_months_ == 0;
  }

  bool
  year_month_duration::is_negative() const {
    return negative_;
  }

  int32_t
  year_month_duration::years() const {
    return total_months_ / 12;
  }

  int32_t
  year_month_duration::months() const {
    return total_months_ % 12;
  }

  int32_t
  year_month_duration::total_months() const {
    return total_months_;
  }

  year_month_duration
  year_month_duration::operator-() const {
    year_month_duration result;
    result.total_months_ = total_months_;
    if (total_months_ != 0) { result.negative_ = !negative_; }
    return result;
  }

  year_month_duration
  operator+(const year_month_duration& a, const year_month_duration& b) {
    int64_t a_signed =
        a.negative_ ? -int64_t{a.total_months_} : int64_t{a.total_months_};
    int64_t b_signed =
        b.negative_ ? -int64_t{b.total_months_} : int64_t{b.total_months_};
    int64_t sum = a_signed + b_signed;

    year_month_duration result;
    if (sum < 0) {
      result.negative_ = true;
      result.total_months_ = static_cast<int32_t>(-sum);
    } else {
      result.negative_ = false;
      result.total_months_ = static_cast<int32_t>(sum);
    }
    return result;
  }

  year_month_duration
  operator-(const year_month_duration& a, const year_month_duration& b) {
    return a + (-b);
  }

  year_month_duration
  operator*(const year_month_duration& a, int32_t n) {
    int64_t a_signed =
        a.negative_ ? -int64_t{a.total_months_} : int64_t{a.total_months_};
    int64_t product = a_signed * n;

    year_month_duration result;
    if (product < 0) {
      result.negative_ = true;
      result.total_months_ = static_cast<int32_t>(-product);
    } else if (product > 0) {
      result.negative_ = false;
      result.total_months_ = static_cast<int32_t>(product);
    }
    return result;
  }

  year_month_duration
  operator*(int32_t n, const year_month_duration& a) {
    return a * n;
  }

  year_month_duration&
  year_month_duration::operator+=(const year_month_duration& o) {
    *this = *this + o;
    return *this;
  }

  year_month_duration&
  year_month_duration::operator-=(const year_month_duration& o) {
    *this = *this - o;
    return *this;
  }

  year_month_duration&
  year_month_duration::operator*=(int32_t n) {
    *this = *this * n;
    return *this;
  }

  std::strong_ordering
  year_month_duration::operator<=>(const year_month_duration& other) const {
    int64_t this_signed =
        negative_ ? -int64_t{total_months_} : int64_t{total_months_};
    int64_t other_signed = other.negative_ ? -int64_t{other.total_months_}
                                           : int64_t{other.total_months_};
    return this_signed <=> other_signed;
  }

  bool
  year_month_duration::operator==(const year_month_duration& other) const {
    return negative_ == other.negative_ && total_months_ == other.total_months_;
  }

  year_month_duration::
  operator std::chrono::months() const {
    int64_t signed_months =
        negative_ ? -int64_t{total_months_} : int64_t{total_months_};
    return std::chrono::months{static_cast<int>(signed_months)};
  }

  year_month_duration::year_month_duration(std::chrono::months m) {
    auto count = m.count();
    if (count < 0) {
      negative_ = true;
      total_months_ = static_cast<int32_t>(-count);
    } else {
      negative_ = false;
      total_months_ = static_cast<int32_t>(count);
    }
  }

} // namespace xb
