#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xb::detail {

  inline bool
  is_leap_year(int32_t year) {
    if (year < 0) { year = -(year + 1); }
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  }

  inline uint8_t
  days_in_month(int32_t year, uint8_t month) {
    static constexpr uint8_t table[] = {0,  31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
      throw std::invalid_argument("days_in_month: invalid month");
    }
    if (month == 2 && is_leap_year(year)) { return 29; }
    return table[month];
  }

  struct tz_result {
    std::optional<int16_t> offset_minutes;
    std::size_t consumed;
  };

  inline tz_result
  parse_timezone(std::string_view str) {
    if (str.empty()) { return {std::nullopt, 0}; }

    if (str[0] == 'Z') { return {int16_t{0}, 1}; }

    if (str[0] == '+' || str[0] == '-') {
      if (str.size() < 6 || str[3] != ':') {
        throw std::invalid_argument("invalid timezone format");
      }
      bool neg = str[0] == '-';
      int hours = (str[1] - '0') * 10 + (str[2] - '0');
      int mins = (str[4] - '0') * 10 + (str[5] - '0');

      if (hours > 14 || (hours == 14 && mins > 0) || mins > 59) {
        throw std::invalid_argument("timezone offset out of range");
      }

      int16_t offset = static_cast<int16_t>(hours * 60 + mins);
      if (neg) { offset = static_cast<int16_t>(-offset); }
      return {offset, 6};
    }

    return {std::nullopt, 0};
  }

  inline void
  format_timezone(std::string& out, std::optional<int16_t> tz) {
    if (!tz.has_value()) { return; }
    int16_t offset = *tz;
    if (offset == 0) {
      out += 'Z';
      return;
    }
    out += (offset < 0) ? '-' : '+';
    if (offset < 0) { offset = static_cast<int16_t>(-offset); }
    int h = offset / 60;
    int m = offset % 60;
    out += static_cast<char>('0' + h / 10);
    out += static_cast<char>('0' + h % 10);
    out += ':';
    out += static_cast<char>('0' + m / 10);
    out += static_cast<char>('0' + m % 10);
  }

  struct frac_result {
    int32_t nanos;
    std::size_t consumed;
  };

  inline frac_result
  parse_fractional_seconds(std::string_view str) {
    if (str.empty() || str[0] != '.') { return {0, 0}; }
    std::size_t pos = 1;
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
    return {nanos, pos};
  }

  inline void
  format_fractional_seconds(std::string& out, int32_t nanos) {
    if (nanos == 0) { return; }
    out += '.';
    std::string frac = std::to_string(nanos);
    frac.insert(0, 9 - frac.size(), '0');
    while (frac.back() == '0') {
      frac.pop_back();
    }
    out += frac;
  }

  struct utc_normalized {
    int32_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int32_t nanosecond;
  };

  inline utc_normalized
  normalize_to_utc(int32_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t minute, uint8_t second, int32_t nanosecond,
                   int16_t tz_offset) {
    // Subtract timezone offset to get UTC
    int total_minutes = hour * 60 + minute - tz_offset;

    int h = total_minutes / 60;
    int m = total_minutes % 60;

    // Normalize minute
    if (m < 0) {
      m += 60;
      h -= 1;
    }

    // Normalize hour
    int day_adj = 0;
    if (h < 0) {
      day_adj = -(((-h) + 23) / 24);
      h -= day_adj * 24;
    } else if (h >= 24) {
      day_adj = h / 24;
      h -= day_adj * 24;
    }

    // Adjust day
    int32_t d = day + day_adj;
    int32_t y = year;
    uint8_t mo = month;

    // Normalize day underflow
    while (d < 1) {
      if (mo == 1) {
        mo = 12;
        y -= 1;
      } else {
        mo -= 1;
      }
      d += days_in_month(y, mo);
    }

    // Normalize day overflow
    while (d > days_in_month(y, mo)) {
      d -= days_in_month(y, mo);
      if (mo == 12) {
        mo = 1;
        y += 1;
      } else {
        mo += 1;
      }
    }

    return {y,
            mo,
            static_cast<uint8_t>(d),
            static_cast<uint8_t>(h),
            static_cast<uint8_t>(m),
            second,
            nanosecond};
  }

} // namespace xb::detail
