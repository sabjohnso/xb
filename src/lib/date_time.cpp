#include <xb/date_time.hpp>

#include "xsd_time_parse.hpp"

#include <stdexcept>

namespace xb {

  namespace {

    void
    expect_digit_pair(std::string_view str, std::size_t pos) {
      if (pos + 1 >= str.size() || str[pos] < '0' || str[pos] > '9' ||
          str[pos + 1] < '0' || str[pos + 1] > '9') {
        throw std::invalid_argument("date_time: expected 2 digits");
      }
    }

    int64_t
    parse_digits(std::string_view str, std::size_t& pos,
                 std::size_t min_digits) {
      int64_t value = 0;
      std::size_t start = pos;
      while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
        value = value * 10 + (str[pos] - '0');
        ++pos;
      }
      if (pos - start < min_digits) {
        throw std::invalid_argument("date_time: insufficient digits");
      }
      return value;
    }

    struct parsed_date_time {
      int32_t year = 1;
      uint8_t month = 1;
      uint8_t day = 1;
      uint8_t hour = 0;
      uint8_t minute = 0;
      uint8_t second = 0;
      int32_t nanosecond = 0;
      std::optional<int16_t> tz_offset_minutes;
    };

    parsed_date_time
    parse_date_time_str(std::string_view str) {
      if (str.size() < 19) {
        throw std::invalid_argument("date_time: string too short");
      }

      parsed_date_time result;
      std::size_t pos = 0;

      // Parse year
      bool neg_year = false;
      if (str[pos] == '-') {
        neg_year = true;
        ++pos;
      }
      int64_t year = parse_digits(str, pos, 4);
      result.year = static_cast<int32_t>(neg_year ? -year : year);

      if (pos >= str.size() || str[pos] != '-') {
        throw std::invalid_argument("date_time: expected '-' after year");
      }
      ++pos;

      // Parse month
      std::size_t month_start = pos;
      int64_t month = parse_digits(str, pos, 2);
      if (pos - month_start != 2 || month < 1 || month > 12) {
        throw std::invalid_argument("date_time: invalid month");
      }
      result.month = static_cast<uint8_t>(month);

      if (pos >= str.size() || str[pos] != '-') {
        throw std::invalid_argument("date_time: expected '-' after month");
      }
      ++pos;

      // Parse day
      std::size_t day_start = pos;
      int64_t day = parse_digits(str, pos, 2);
      if (pos - day_start != 2 || day < 1 ||
          day > detail::days_in_month(result.year, result.month)) {
        throw std::invalid_argument("date_time: invalid day");
      }
      result.day = static_cast<uint8_t>(day);

      // Expect 'T'
      if (pos >= str.size() || str[pos] != 'T') {
        throw std::invalid_argument("date_time: expected 'T'");
      }
      ++pos;

      // Parse hour
      expect_digit_pair(str, pos);
      int hour = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      if (pos >= str.size() || str[pos] != ':') {
        throw std::invalid_argument("date_time: expected ':'");
      }
      ++pos;

      // Parse minute
      expect_digit_pair(str, pos);
      int minute = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      if (pos >= str.size() || str[pos] != ':') {
        throw std::invalid_argument("date_time: expected ':'");
      }
      ++pos;

      // Parse second
      expect_digit_pair(str, pos);
      int second = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      // Handle 24:00:00 — canonicalize to 00:00:00
      // and roll day forward
      bool roll_day = false;
      if (hour == 24) {
        if (minute != 0 || second != 0) {
          throw std::invalid_argument("date_time: 24:XX:XX requires 24:00:00");
        }
        if (pos < str.size() && str[pos] == '.') {
          throw std::invalid_argument("date_time: 24:00:00 cannot have "
                                      "fractional seconds");
        }
        hour = 0;
        roll_day = true;
      } else if (hour > 23) {
        throw std::invalid_argument("date_time: hour out of range");
      }

      if (minute > 59) {
        throw std::invalid_argument("date_time: minute out of range");
      }
      if (second > 59) {
        throw std::invalid_argument("date_time: second out of range");
      }

      result.hour = static_cast<uint8_t>(hour);
      result.minute = static_cast<uint8_t>(minute);
      result.second = static_cast<uint8_t>(second);

      // Parse fractional seconds
      auto frac = detail::parse_fractional_seconds(str.substr(pos));
      result.nanosecond = frac.nanos;
      pos += frac.consumed;

      // Parse timezone
      auto tz = detail::parse_timezone(str.substr(pos));
      result.tz_offset_minutes = tz.offset_minutes;
      pos += tz.consumed;

      if (pos != str.size()) {
        throw std::invalid_argument("date_time: trailing characters");
      }

      // Roll day forward for 24:00:00
      if (roll_day) {
        int32_t d = result.day + 1;
        if (d > detail::days_in_month(result.year, result.month)) {
          d = 1;
          uint8_t mo = result.month + 1;
          if (mo > 12) {
            mo = 1;
            result.year += 1;
          }
          result.month = mo;
        }
        result.day = static_cast<uint8_t>(d);
      }

      return result;
    }

  } // namespace

  date_time::date_time(std::string_view str) {
    auto p = parse_date_time_str(str);
    year_ = p.year;
    month_ = p.month;
    day_ = p.day;
    hour_ = p.hour;
    minute_ = p.minute;
    second_ = p.second;
    nanosecond_ = p.nanosecond;
    tz_offset_minutes_ = p.tz_offset_minutes;
  }

  std::string
  date_time::to_string() const {
    std::string result;
    int32_t y = year_;
    if (y < 0) {
      result += '-';
      y = -y;
    }
    std::string year_str = std::to_string(y);
    if (year_str.size() < 4) { year_str.insert(0, 4 - year_str.size(), '0'); }
    result += year_str;
    result += '-';
    result += static_cast<char>('0' + month_ / 10);
    result += static_cast<char>('0' + month_ % 10);
    result += '-';
    result += static_cast<char>('0' + day_ / 10);
    result += static_cast<char>('0' + day_ % 10);
    result += 'T';
    result += static_cast<char>('0' + hour_ / 10);
    result += static_cast<char>('0' + hour_ % 10);
    result += ':';
    result += static_cast<char>('0' + minute_ / 10);
    result += static_cast<char>('0' + minute_ % 10);
    result += ':';
    result += static_cast<char>('0' + second_ / 10);
    result += static_cast<char>('0' + second_ % 10);

    detail::format_fractional_seconds(result, nanosecond_);
    detail::format_timezone(result, tz_offset_minutes_);
    return result;
  }

  int32_t
  date_time::year() const {
    return year_;
  }

  uint8_t
  date_time::month() const {
    return month_;
  }

  uint8_t
  date_time::day() const {
    return day_;
  }

  uint8_t
  date_time::hour() const {
    return hour_;
  }

  uint8_t
  date_time::minute() const {
    return minute_;
  }

  uint8_t
  date_time::second() const {
    return second_;
  }

  int32_t
  date_time::nanosecond() const {
    return nanosecond_;
  }

  bool
  date_time::has_timezone() const {
    return tz_offset_minutes_.has_value();
  }

  std::optional<int16_t>
  date_time::tz_offset_minutes() const {
    return tz_offset_minutes_;
  }

  xb::date
  date_time::date_part() const {
    return xb::date(year_, month_, day_, tz_offset_minutes_);
  }

  xb::time
  date_time::time_part() const {
    // Build string representation for time
    std::string s;
    s += static_cast<char>('0' + hour_ / 10);
    s += static_cast<char>('0' + hour_ % 10);
    s += ':';
    s += static_cast<char>('0' + minute_ / 10);
    s += static_cast<char>('0' + minute_ % 10);
    s += ':';
    s += static_cast<char>('0' + second_ / 10);
    s += static_cast<char>('0' + second_ % 10);

    detail::format_fractional_seconds(s, nanosecond_);
    detail::format_timezone(s, tz_offset_minutes_);

    return xb::time(s);
  }

  bool
  date_time::operator==(const date_time& other) const {
    bool this_has_tz = has_timezone();
    bool other_has_tz = other.has_timezone();

    if (this_has_tz != other_has_tz) { return false; }

    if (this_has_tz && other_has_tz) {
      auto a =
          detail::normalize_to_utc(year_, month_, day_, hour_, minute_, second_,
                                   nanosecond_, *tz_offset_minutes_);
      auto b = detail::normalize_to_utc(
          other.year_, other.month_, other.day_, other.hour_, other.minute_,
          other.second_, other.nanosecond_, *other.tz_offset_minutes_);
      return a.year == b.year && a.month == b.month && a.day == b.day &&
             a.hour == b.hour && a.minute == b.minute && a.second == b.second &&
             a.nanosecond == b.nanosecond;
    }

    return year_ == other.year_ && month_ == other.month_ &&
           day_ == other.day_ && hour_ == other.hour_ &&
           minute_ == other.minute_ && second_ == other.second_ &&
           nanosecond_ == other.nanosecond_;
  }

} // namespace xb
