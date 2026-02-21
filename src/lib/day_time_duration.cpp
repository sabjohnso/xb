#include <xb/day_time_duration.hpp>

#include <cstdlib>
#include <stdexcept>

namespace xb {

  namespace {

    constexpr int64_t seconds_per_minute = 60;
    constexpr int64_t seconds_per_hour = 3600;
    constexpr int64_t seconds_per_day = 86400;
    constexpr int64_t nanos_per_second = 1000000000LL;

    struct parsed_dt {
      bool negative = false;
      int64_t total_seconds = 0;
      int32_t nanoseconds = 0;
    };

    // Parse digits starting at pos, return value and advance
    // pos.
    int64_t
    parse_digits(std::string_view str, std::size_t& pos) {
      int64_t value = 0;
      std::size_t start = pos;
      while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        value = value * 10 + (str[pos] - '0');
        ++pos;
      }
      if (pos == start) {
        throw std::invalid_argument("day_time_duration: expected digit");
      }
      return value;
    }

    // Parse fractional seconds (.NNN) and return nanoseconds.
    int32_t
    parse_fractional(std::string_view str, std::size_t& pos) {
      if (pos >= str.size() || str[pos] != '.') { return 0; }
      ++pos; // skip '.'

      int32_t nanos = 0;
      int digits = 0;
      while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        if (digits < 9) {
          nanos = nanos * 10 + (str[pos] - '0');
          ++digits;
        }
        // digits > 9 are truncated
        ++pos;
      }
      // Pad to 9 digits
      while (digits < 9) {
        nanos *= 10;
        ++digits;
      }
      return nanos;
    }

    parsed_dt
    parse_day_time_duration(std::string_view str) {
      if (str.empty()) {
        throw std::invalid_argument("day_time_duration: empty string");
      }

      parsed_dt result;
      std::size_t pos = 0;

      if (str[pos] == '-') {
        result.negative = true;
        ++pos;
      }

      if (pos >= str.size() || str[pos] != 'P') {
        throw std::invalid_argument("day_time_duration: expected 'P'");
      }
      ++pos;

      if (pos >= str.size()) {
        throw std::invalid_argument(
            "day_time_duration: expected component after 'P'");
      }

      // Must not contain 'Y' or month 'M' before T
      // (that's year_month_duration territory)
      if (str.find('Y', pos) != std::string_view::npos) {
        throw std::invalid_argument(
            "day_time_duration: unexpected 'Y' component");
      }

      // Check for month M (M before T or M without T)
      auto t_pos = str.find('T', pos);
      auto m_pos = str.find('M', pos);
      if (m_pos != std::string_view::npos &&
          (t_pos == std::string_view::npos || m_pos < t_pos)) {
        throw std::invalid_argument("day_time_duration: unexpected month 'M' "
                                    "component");
      }

      int64_t total_seconds = 0;
      bool found_any = false;

      // Parse optional days (before T)
      if (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        int64_t days = parse_digits(str, pos);
        if (pos >= str.size() || str[pos] != 'D') {
          throw std::invalid_argument(
              "day_time_duration: expected 'D' after number");
        }
        ++pos;
        total_seconds += days * seconds_per_day;
        found_any = true;
      }

      // Parse time portion (after T)
      if (pos < str.size() && str[pos] == 'T') {
        ++pos;
        if (pos >= str.size()) {
          throw std::invalid_argument("day_time_duration: expected component "
                                      "after 'T'");
        }

        bool found_time = false;

        // Parse hours
        if (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
          int64_t value = parse_digits(str, pos);
          if (pos < str.size() && str[pos] == 'H') {
            total_seconds += value * seconds_per_hour;
            found_time = true;
            ++pos;
          } else if (pos < str.size() && str[pos] == 'M') {
            total_seconds += value * seconds_per_minute;
            found_time = true;
            ++pos;
          } else if (pos < str.size() && (str[pos] == 'S' || str[pos] == '.')) {
            total_seconds += value;
            result.nanoseconds = parse_fractional(str, pos);
            if (pos >= str.size() || str[pos] != 'S') {
              throw std::invalid_argument("day_time_duration: expected 'S'");
            }
            ++pos;
            found_time = true;
          } else {
            throw std::invalid_argument(
                "day_time_duration: unexpected character "
                "after number");
          }

          // Parse minutes (if we parsed hours above)
          if (found_time && pos < str.size() && str[pos] >= '0' &&
              str[pos] <= '9') {
            int64_t value2 = parse_digits(str, pos);
            if (pos < str.size() && str[pos] == 'M') {
              total_seconds += value2 * seconds_per_minute;
              ++pos;
            } else if (pos < str.size() &&
                       (str[pos] == 'S' || str[pos] == '.')) {
              total_seconds += value2;
              result.nanoseconds = parse_fractional(str, pos);
              if (pos >= str.size() || str[pos] != 'S') {
                throw std::invalid_argument("day_time_duration: expected 'S'");
              }
              ++pos;
            } else {
              throw std::invalid_argument(
                  "day_time_duration: unexpected character");
            }
          }

          // Parse seconds (if we still have digits)
          if (found_time && pos < str.size() && str[pos] >= '0' &&
              str[pos] <= '9') {
            int64_t value3 = parse_digits(str, pos);
            total_seconds += value3;
            result.nanoseconds = parse_fractional(str, pos);
            if (pos >= str.size() || str[pos] != 'S') {
              throw std::invalid_argument("day_time_duration: expected 'S'");
            }
            ++pos;
          }
        }

        if (!found_time) {
          throw std::invalid_argument("day_time_duration: no time components "
                                      "after 'T'");
        }
        found_any = true;
      }

      if (!found_any) {
        throw std::invalid_argument("day_time_duration: no components found");
      }

      if (pos != str.size()) {
        throw std::invalid_argument("day_time_duration: trailing characters");
      }

      result.total_seconds = total_seconds;

      // Normalize negative zero
      if (result.total_seconds == 0 && result.nanoseconds == 0) {
        result.negative = false;
      }

      return result;
    }

  } // namespace

  day_time_duration::day_time_duration(std::string_view str) {
    auto parsed = parse_day_time_duration(str);
    negative_ = parsed.negative;
    total_seconds_ = parsed.total_seconds;
    nanoseconds_ = parsed.nanoseconds;
  }

  std::string
  day_time_duration::to_string() const {
    std::string result;
    if (negative_) { result += '-'; }
    result += 'P';

    int64_t remaining = total_seconds_;
    int64_t d = remaining / seconds_per_day;
    remaining %= seconds_per_day;
    int64_t h = remaining / seconds_per_hour;
    remaining %= seconds_per_hour;
    int64_t m = remaining / seconds_per_minute;
    int64_t s = remaining % seconds_per_minute;

    if (d > 0) {
      result += std::to_string(d);
      result += 'D';
    }

    bool need_time = (h > 0 || m > 0 || s > 0 || nanoseconds_ > 0 || d == 0);
    if (need_time) {
      result += 'T';
      bool wrote_any = false;

      if (h > 0) {
        result += std::to_string(h);
        result += 'H';
        wrote_any = true;
      }
      if (m > 0) {
        result += std::to_string(m);
        result += 'M';
        wrote_any = true;
      }
      if (s > 0 || nanoseconds_ > 0 || !wrote_any) {
        result += std::to_string(s);
        if (nanoseconds_ > 0) {
          result += '.';
          std::string frac = std::to_string(nanoseconds_);
          // Pad to 9 digits
          frac.insert(0, 9 - frac.size(), '0');
          // Strip trailing zeros
          while (frac.back() == '0') {
            frac.pop_back();
          }
          result += frac;
        }
        result += 'S';
      }
    }

    return result;
  }

  bool
  day_time_duration::is_zero() const {
    return total_seconds_ == 0 && nanoseconds_ == 0;
  }

  bool
  day_time_duration::is_negative() const {
    return negative_;
  }

  int64_t
  day_time_duration::days() const {
    return total_seconds_ / seconds_per_day;
  }

  int32_t
  day_time_duration::hours() const {
    return static_cast<int32_t>((total_seconds_ % seconds_per_day) /
                                seconds_per_hour);
  }

  int32_t
  day_time_duration::minutes() const {
    return static_cast<int32_t>((total_seconds_ % seconds_per_hour) /
                                seconds_per_minute);
  }

  int32_t
  day_time_duration::seconds() const {
    return static_cast<int32_t>(total_seconds_ % seconds_per_minute);
  }

  int32_t
  day_time_duration::nanoseconds() const {
    return nanoseconds_;
  }

  day_time_duration
  day_time_duration::operator-() const {
    day_time_duration result;
    result.total_seconds_ = total_seconds_;
    result.nanoseconds_ = nanoseconds_;
    if (total_seconds_ != 0 || nanoseconds_ != 0) {
      result.negative_ = !negative_;
    }
    return result;
  }

  namespace {} // namespace

  day_time_duration
  operator+(const day_time_duration& a, const day_time_duration& b) {
    int64_t a_sec = a.negative_ ? -a.total_seconds_ : a.total_seconds_;
    int64_t a_ns =
        a.negative_ ? -int64_t{a.nanoseconds_} : int64_t{a.nanoseconds_};
    int64_t b_sec = b.negative_ ? -b.total_seconds_ : b.total_seconds_;
    int64_t b_ns =
        b.negative_ ? -int64_t{b.nanoseconds_} : int64_t{b.nanoseconds_};

    int64_t total_ns = (a_sec + b_sec) * nanos_per_second + a_ns + b_ns;
    bool neg = total_ns < 0;
    if (neg) { total_ns = -total_ns; }

    day_time_duration result;
    result.total_seconds_ = total_ns / nanos_per_second;
    result.nanoseconds_ = static_cast<int32_t>(total_ns % nanos_per_second);
    result.negative_ =
        (result.total_seconds_ != 0 || result.nanoseconds_ != 0) ? neg : false;
    return result;
  }

  day_time_duration
  operator-(const day_time_duration& a, const day_time_duration& b) {
    return a + (-b);
  }

  day_time_duration
  operator*(const day_time_duration& a, int64_t n) {
    // Multiply seconds and nanoseconds separately to avoid
    // overflow.
    int64_t a_sec = a.negative_ ? -a.total_seconds_ : a.total_seconds_;
    int64_t a_ns =
        a.negative_ ? -int64_t{a.nanoseconds_} : int64_t{a.nanoseconds_};

    int64_t sec_product = a_sec * n;
    int64_t ns_product = a_ns * n;

    // Normalize nanoseconds into seconds
    sec_product += ns_product / nanos_per_second;
    ns_product %= nanos_per_second;

    // Ensure same sign
    if (sec_product > 0 && ns_product < 0) {
      sec_product -= 1;
      ns_product += nanos_per_second;
    } else if (sec_product < 0 && ns_product > 0) {
      sec_product += 1;
      ns_product -= nanos_per_second;
    }

    bool neg = sec_product < 0 || (sec_product == 0 && ns_product < 0);

    day_time_duration result;
    result.total_seconds_ = neg ? -sec_product : sec_product;
    result.nanoseconds_ = static_cast<int32_t>(neg ? -ns_product : ns_product);
    result.negative_ =
        (result.total_seconds_ != 0 || result.nanoseconds_ != 0) ? neg : false;
    return result;
  }

  day_time_duration
  operator*(int64_t n, const day_time_duration& a) {
    return a * n;
  }

  day_time_duration&
  day_time_duration::operator+=(const day_time_duration& o) {
    *this = *this + o;
    return *this;
  }

  day_time_duration&
  day_time_duration::operator-=(const day_time_duration& o) {
    *this = *this - o;
    return *this;
  }

  day_time_duration&
  day_time_duration::operator*=(int64_t n) {
    *this = *this * n;
    return *this;
  }

  std::strong_ordering
  day_time_duration::operator<=>(const day_time_duration& other) const {
    int64_t this_ns = total_seconds_ * nanos_per_second + nanoseconds_;
    if (negative_) { this_ns = -this_ns; }

    int64_t other_ns =
        other.total_seconds_ * nanos_per_second + other.nanoseconds_;
    if (other.negative_) { other_ns = -other_ns; }

    return this_ns <=> other_ns;
  }

  bool
  day_time_duration::operator==(const day_time_duration& other) const {
    return negative_ == other.negative_ &&
           total_seconds_ == other.total_seconds_ &&
           nanoseconds_ == other.nanoseconds_;
  }

  day_time_duration::
  operator std::chrono::nanoseconds() const {
    int64_t total_ns = total_seconds_ * nanos_per_second + nanoseconds_;
    if (negative_) { total_ns = -total_ns; }
    return std::chrono::nanoseconds{total_ns};
  }

  day_time_duration::day_time_duration(std::chrono::nanoseconds ns) {
    auto count = ns.count();
    if (count < 0) {
      negative_ = true;
      count = -count;
    }
    total_seconds_ = count / nanos_per_second;
    nanoseconds_ = static_cast<int32_t>(count % nanos_per_second);
    if (total_seconds_ == 0 && nanoseconds_ == 0) { negative_ = false; }
  }

} // namespace xb
