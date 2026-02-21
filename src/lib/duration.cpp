#include <xb/duration.hpp>

#include <stdexcept>

namespace xb {

  namespace {

    constexpr int64_t seconds_per_minute = 60;
    constexpr int64_t seconds_per_hour = 3600;
    constexpr int64_t seconds_per_day = 86400;

    int64_t
    parse_digits(std::string_view str, std::size_t& pos) {
      int64_t value = 0;
      std::size_t start = pos;
      while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        value = value * 10 + (str[pos] - '0');
        ++pos;
      }
      if (pos == start) {
        throw std::invalid_argument("duration: expected digit");
      }
      return value;
    }

    int32_t
    parse_fractional(std::string_view str, std::size_t& pos) {
      if (pos >= str.size() || str[pos] != '.') { return 0; }
      ++pos;
      int32_t nanos = 0;
      int digits = 0;
      while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        if (digits < 9) {
          nanos = nanos * 10 + (str[pos] - '0');
          ++digits;
        }
        ++pos;
      }
      while (digits < 9) {
        nanos *= 10;
        ++digits;
      }
      return nanos;
    }

    struct parsed_duration {
      bool negative = false;
      int32_t total_months = 0;
      int64_t total_seconds = 0;
      int32_t nanoseconds = 0;
    };

    parsed_duration
    parse_duration_str(std::string_view str) {
      if (str.empty()) {
        throw std::invalid_argument("duration: empty string");
      }

      parsed_duration result;
      std::size_t pos = 0;

      if (str[pos] == '-') {
        result.negative = true;
        ++pos;
      }

      if (pos >= str.size() || str[pos] != 'P') {
        throw std::invalid_argument("duration: expected 'P'");
      }
      ++pos;

      if (pos >= str.size()) {
        throw std::invalid_argument("duration: expected component after 'P'");
      }

      bool found_any = false;
      int64_t total_months = 0;
      int64_t total_seconds = 0;

      // Parse date portion (Y, M, D before T)
      while (pos < str.size() && str[pos] != 'T') {
        if (str[pos] >= '0' && str[pos] <= '9') {
          int64_t value = parse_digits(str, pos);
          if (pos >= str.size()) {
            throw std::invalid_argument("duration: expected designator");
          }
          if (str[pos] == 'Y') {
            total_months += value * 12;
            found_any = true;
            ++pos;
          } else if (str[pos] == 'M') {
            total_months += value;
            found_any = true;
            ++pos;
          } else if (str[pos] == 'D') {
            total_seconds += value * seconds_per_day;
            found_any = true;
            ++pos;
          } else {
            throw std::invalid_argument("duration: unexpected designator");
          }
        } else {
          throw std::invalid_argument("duration: unexpected character");
        }
      }

      // Parse time portion
      if (pos < str.size() && str[pos] == 'T') {
        ++pos;
        if (pos >= str.size()) {
          throw std::invalid_argument("duration: expected component after 'T'");
        }

        bool found_time = false;
        while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
          int64_t value = parse_digits(str, pos);
          if (pos >= str.size()) {
            throw std::invalid_argument("duration: expected designator");
          }
          if (str[pos] == 'H') {
            total_seconds += value * seconds_per_hour;
            found_time = true;
            ++pos;
          } else if (str[pos] == 'M') {
            total_seconds += value * seconds_per_minute;
            found_time = true;
            ++pos;
          } else if (str[pos] == 'S' || str[pos] == '.') {
            total_seconds += value;
            result.nanoseconds = parse_fractional(str, pos);
            if (pos >= str.size() || str[pos] != 'S') {
              throw std::invalid_argument("duration: expected 'S'");
            }
            ++pos;
            found_time = true;
          } else {
            throw std::invalid_argument("duration: unexpected designator");
          }
        }

        if (!found_time) {
          throw std::invalid_argument("duration: no time components after 'T'");
        }
        found_any = true;
      }

      if (!found_any) {
        throw std::invalid_argument("duration: no components found");
      }

      if (pos != str.size()) {
        throw std::invalid_argument("duration: trailing characters");
      }

      result.total_months = static_cast<int32_t>(total_months);
      result.total_seconds = total_seconds;

      // Normalize negative zero
      if (result.total_months == 0 && result.total_seconds == 0 &&
          result.nanoseconds == 0) {
        result.negative = false;
      }

      return result;
    }

  } // namespace

  duration::duration(std::string_view str) {
    auto parsed = parse_duration_str(str);
    negative_ = parsed.negative;
    total_months_ = parsed.total_months;
    total_seconds_ = parsed.total_seconds;
    nanoseconds_ = parsed.nanoseconds;
  }

  std::string
  duration::to_string() const {
    std::string result;
    if (negative_) { result += '-'; }
    result += 'P';

    int32_t y = total_months_ / 12;
    int32_t mo = total_months_ % 12;

    int64_t remaining = total_seconds_;
    int64_t d = remaining / seconds_per_day;
    remaining %= seconds_per_day;
    int64_t h = remaining / seconds_per_hour;
    remaining %= seconds_per_hour;
    int64_t mi = remaining / seconds_per_minute;
    int64_t s = remaining % seconds_per_minute;

    bool has_date_part = (y > 0 || mo > 0 || d > 0);
    bool has_time_part = (h > 0 || mi > 0 || s > 0 || nanoseconds_ > 0);

    if (y > 0) {
      result += std::to_string(y);
      result += 'Y';
    }
    if (mo > 0) {
      result += std::to_string(mo);
      result += 'M';
    }
    if (d > 0) {
      result += std::to_string(d);
      result += 'D';
    }

    if (has_time_part) {
      result += 'T';
      bool wrote_any = false;
      if (h > 0) {
        result += std::to_string(h);
        result += 'H';
        wrote_any = true;
      }
      if (mi > 0) {
        result += std::to_string(mi);
        result += 'M';
        wrote_any = true;
      }
      if (s > 0 || nanoseconds_ > 0 || !wrote_any) {
        result += std::to_string(s);
        if (nanoseconds_ > 0) {
          result += '.';
          std::string frac = std::to_string(nanoseconds_);
          frac.insert(0, 9 - frac.size(), '0');
          while (frac.back() == '0') {
            frac.pop_back();
          }
          result += frac;
        }
        result += 'S';
      }
    } else if (!has_date_part) {
      // Zero duration
      result += "T0S";
    }

    return result;
  }

  bool
  duration::is_zero() const {
    return total_months_ == 0 && total_seconds_ == 0 && nanoseconds_ == 0;
  }

  bool
  duration::is_negative() const {
    return negative_;
  }

  year_month_duration
  duration::year_month_part() const {
    if (total_months_ == 0) { return year_month_duration(); }
    auto ym = year_month_duration(total_months_ / 12, total_months_ % 12);
    return negative_ ? -ym : ym;
  }

  day_time_duration
  duration::day_time_part() const {
    if (total_seconds_ == 0 && nanoseconds_ == 0) {
      return day_time_duration();
    }
    // Build string representation for the day-time portion
    std::string s;
    if (negative_) { s += '-'; }
    s += 'P';

    int64_t remaining = total_seconds_;
    int64_t d = remaining / seconds_per_day;
    remaining %= seconds_per_day;

    if (d > 0) {
      s += std::to_string(d);
      s += 'D';
    }

    int64_t h = remaining / seconds_per_hour;
    remaining %= seconds_per_hour;
    int64_t mi = remaining / seconds_per_minute;
    int64_t sec = remaining % seconds_per_minute;

    bool has_time = (h > 0 || mi > 0 || sec > 0 || nanoseconds_ > 0 || d == 0);
    if (has_time) {
      s += 'T';
      bool wrote = false;
      if (h > 0) {
        s += std::to_string(h);
        s += 'H';
        wrote = true;
      }
      if (mi > 0) {
        s += std::to_string(mi);
        s += 'M';
        wrote = true;
      }
      if (sec > 0 || nanoseconds_ > 0 || !wrote) {
        s += std::to_string(sec);
        if (nanoseconds_ > 0) {
          s += '.';
          std::string frac = std::to_string(nanoseconds_);
          frac.insert(0, 9 - frac.size(), '0');
          while (frac.back() == '0') {
            frac.pop_back();
          }
          s += frac;
        }
        s += 'S';
      }
    }

    return day_time_duration(s);
  }

  int32_t
  duration::years() const {
    return total_months_ / 12;
  }

  int32_t
  duration::months() const {
    return total_months_ % 12;
  }

  int64_t
  duration::days() const {
    return total_seconds_ / seconds_per_day;
  }

  int32_t
  duration::hours() const {
    return static_cast<int32_t>((total_seconds_ % seconds_per_day) /
                                seconds_per_hour);
  }

  int32_t
  duration::minutes() const {
    return static_cast<int32_t>((total_seconds_ % seconds_per_hour) /
                                seconds_per_minute);
  }

  int32_t
  duration::seconds() const {
    return static_cast<int32_t>(total_seconds_ % seconds_per_minute);
  }

  int32_t
  duration::nanoseconds() const {
    return nanoseconds_;
  }

  duration
  duration::operator-() const {
    duration result;
    result.total_months_ = total_months_;
    result.total_seconds_ = total_seconds_;
    result.nanoseconds_ = nanoseconds_;
    if (total_months_ != 0 || total_seconds_ != 0 || nanoseconds_ != 0) {
      result.negative_ = !negative_;
    }
    return result;
  }

  bool
  duration::operator==(const duration& other) const {
    return negative_ == other.negative_ &&
           total_months_ == other.total_months_ &&
           total_seconds_ == other.total_seconds_ &&
           nanoseconds_ == other.nanoseconds_;
  }

} // namespace xb
