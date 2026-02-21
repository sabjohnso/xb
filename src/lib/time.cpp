#include <xb/time.hpp>

#include "xsd_time_parse.hpp"

#include <stdexcept>

namespace xb {

  namespace {

    struct parsed_time {
      uint8_t hour = 0;
      uint8_t minute = 0;
      uint8_t second = 0;
      int32_t nanosecond = 0;
      std::optional<int16_t> tz_offset_minutes;
    };

    void
    expect_digit_pair(std::string_view str, std::size_t pos) {
      if (pos + 1 >= str.size() || str[pos] < '0' || str[pos] > '9' ||
          str[pos + 1] < '0' || str[pos + 1] > '9') {
        throw std::invalid_argument("time: expected 2 digits");
      }
    }

    parsed_time
    parse_time_str(std::string_view str) {
      if (str.size() < 8) {
        throw std::invalid_argument("time: string too short");
      }

      parsed_time result;
      std::size_t pos = 0;

      // Parse hour (2 digits)
      expect_digit_pair(str, pos);
      int hour = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      // Expect ':'
      if (pos >= str.size() || str[pos] != ':') {
        throw std::invalid_argument("time: expected ':' after hour");
      }
      ++pos;

      // Parse minute (2 digits)
      expect_digit_pair(str, pos);
      int minute = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      // Expect ':'
      if (pos >= str.size() || str[pos] != ':') {
        throw std::invalid_argument("time: expected ':' after minute");
      }
      ++pos;

      // Parse second (2 digits)
      expect_digit_pair(str, pos);
      int second = (str[pos] - '0') * 10 + (str[pos + 1] - '0');
      pos += 2;

      // Validate 24:00:00
      if (hour == 24) {
        if (minute != 0 || second != 0) {
          throw std::invalid_argument("time: 24:XX:XX requires 24:00:00");
        }
        // Check no fractional seconds for 24:00:00
        if (pos < str.size() && str[pos] == '.') {
          throw std::invalid_argument("time: 24:00:00 cannot have fractional "
                                      "seconds");
        }
        // Canonicalize to 00:00:00
        hour = 0;
      } else if (hour > 23) {
        throw std::invalid_argument("time: hour out of range");
      }

      if (minute > 59) {
        throw std::invalid_argument("time: minute out of range");
      }
      if (second > 59) {
        throw std::invalid_argument("time: second out of range");
      }

      result.hour = static_cast<uint8_t>(hour);
      result.minute = static_cast<uint8_t>(minute);
      result.second = static_cast<uint8_t>(second);

      // Parse optional fractional seconds
      auto frac = detail::parse_fractional_seconds(str.substr(pos));
      result.nanosecond = frac.nanos;
      pos += frac.consumed;

      // Parse optional timezone
      auto tz = detail::parse_timezone(str.substr(pos));
      result.tz_offset_minutes = tz.offset_minutes;
      pos += tz.consumed;

      if (pos != str.size()) {
        throw std::invalid_argument("time: trailing characters");
      }

      return result;
    }

  } // namespace

  time::time(std::string_view str) {
    auto parsed = parse_time_str(str);
    hour_ = parsed.hour;
    minute_ = parsed.minute;
    second_ = parsed.second;
    nanosecond_ = parsed.nanosecond;
    tz_offset_minutes_ = parsed.tz_offset_minutes;
  }

  std::string
  time::to_string() const {
    std::string result;
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

  uint8_t
  time::hour() const {
    return hour_;
  }

  uint8_t
  time::minute() const {
    return minute_;
  }

  uint8_t
  time::second() const {
    return second_;
  }

  int32_t
  time::nanosecond() const {
    return nanosecond_;
  }

  bool
  time::has_timezone() const {
    return tz_offset_minutes_.has_value();
  }

  std::optional<int16_t>
  time::tz_offset_minutes() const {
    return tz_offset_minutes_;
  }

  bool
  time::operator==(const time& other) const {
    bool this_has_tz = has_timezone();
    bool other_has_tz = other.has_timezone();

    if (this_has_tz != other_has_tz) { return false; }

    if (this_has_tz && other_has_tz) {
      // Normalize to UTC using a reference date (2000-01-01)
      auto a = detail::normalize_to_utc(2000, 1, 1, hour_, minute_, second_,
                                        nanosecond_, *tz_offset_minutes_);
      auto b = detail::normalize_to_utc(2000, 1, 1, other.hour_, other.minute_,
                                        other.second_, other.nanosecond_,
                                        *other.tz_offset_minutes_);
      return a.hour == b.hour && a.minute == b.minute && a.second == b.second &&
             a.nanosecond == b.nanosecond;
    }

    return hour_ == other.hour_ && minute_ == other.minute_ &&
           second_ == other.second_ && nanosecond_ == other.nanosecond_;
  }

} // namespace xb
