#include <xb/date.hpp>

#include "xsd_time_parse.hpp"

#include <stdexcept>

namespace xb {

  namespace {

    struct parsed_date {
      int32_t year = 1;
      uint8_t month = 1;
      uint8_t day = 1;
      std::optional<int16_t> tz_offset_minutes;
    };

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
        throw std::invalid_argument("date: insufficient digits");
      }
      return value;
    }

    parsed_date
    parse_date_str(std::string_view str) {
      if (str.empty()) { throw std::invalid_argument("date: empty string"); }

      parsed_date result;
      std::size_t pos = 0;

      // Parse optional negative sign for year
      bool neg_year = false;
      if (str[pos] == '-') {
        neg_year = true;
        ++pos;
      }

      // Parse year (at least 4 digits)
      int64_t year = parse_digits(str, pos, 4);
      result.year = static_cast<int32_t>(neg_year ? -year : year);

      // Expect '-'
      if (pos >= str.size() || str[pos] != '-') {
        throw std::invalid_argument("date: expected '-' after year");
      }
      ++pos;

      // Parse month (exactly 2 digits)
      std::size_t month_start = pos;
      int64_t month = parse_digits(str, pos, 2);
      if (pos - month_start != 2) {
        throw std::invalid_argument("date: month must be 2 digits");
      }
      if (month < 1 || month > 12) {
        throw std::invalid_argument("date: month out of range");
      }
      result.month = static_cast<uint8_t>(month);

      // Expect '-'
      if (pos >= str.size() || str[pos] != '-') {
        throw std::invalid_argument("date: expected '-' after month");
      }
      ++pos;

      // Parse day (exactly 2 digits)
      std::size_t day_start = pos;
      int64_t day = parse_digits(str, pos, 2);
      if (pos - day_start != 2) {
        throw std::invalid_argument("date: day must be 2 digits");
      }
      if (day < 1 || day > detail::days_in_month(result.year, result.month)) {
        throw std::invalid_argument("date: day out of range");
      }
      result.day = static_cast<uint8_t>(day);

      // Parse optional timezone
      auto tz = detail::parse_timezone(str.substr(pos));
      result.tz_offset_minutes = tz.offset_minutes;
      pos += tz.consumed;

      if (pos != str.size()) {
        throw std::invalid_argument("date: trailing characters");
      }

      return result;
    }

  } // namespace

  date::date(std::string_view str) {
    auto parsed = parse_date_str(str);
    year_ = parsed.year;
    month_ = parsed.month;
    day_ = parsed.day;
    tz_offset_minutes_ = parsed.tz_offset_minutes;
  }

  date::date(int32_t year, uint8_t month, uint8_t day,
             std::optional<int16_t> tz_offset_minutes)
      : year_(year), month_(month), day_(day),
        tz_offset_minutes_(tz_offset_minutes) {
    if (month < 1 || month > 12) {
      throw std::invalid_argument("date: month out of range");
    }
    if (day < 1 || day > detail::days_in_month(year, month)) {
      throw std::invalid_argument("date: day out of range");
    }
  }

  std::string
  date::to_string() const {
    std::string result;
    int32_t y = year_;
    if (y < 0) {
      result += '-';
      y = -y;
    }
    // Pad to at least 4 digits
    std::string year_str = std::to_string(y);
    if (year_str.size() < 4) { year_str.insert(0, 4 - year_str.size(), '0'); }
    result += year_str;
    result += '-';
    result += static_cast<char>('0' + month_ / 10);
    result += static_cast<char>('0' + month_ % 10);
    result += '-';
    result += static_cast<char>('0' + day_ / 10);
    result += static_cast<char>('0' + day_ % 10);

    detail::format_timezone(result, tz_offset_minutes_);
    return result;
  }

  int32_t
  date::year() const {
    return year_;
  }

  uint8_t
  date::month() const {
    return month_;
  }

  uint8_t
  date::day() const {
    return day_;
  }

  bool
  date::has_timezone() const {
    return tz_offset_minutes_.has_value();
  }

  std::optional<int16_t>
  date::tz_offset_minutes() const {
    return tz_offset_minutes_;
  }

  bool
  date::operator==(const date& other) const {
    bool this_has_tz = has_timezone();
    bool other_has_tz = other.has_timezone();

    // Mixed timezone: not equal
    if (this_has_tz != other_has_tz) { return false; }

    if (this_has_tz && other_has_tz) {
      // Both have timezone: normalize to UTC
      auto a = detail::normalize_to_utc(year_, month_, day_, 0, 0, 0, 0,
                                        *tz_offset_minutes_);
      auto b = detail::normalize_to_utc(other.year_, other.month_, other.day_,
                                        0, 0, 0, 0, *other.tz_offset_minutes_);
      return a.year == b.year && a.month == b.month && a.day == b.day;
    }

    // Neither has timezone: field compare
    return year_ == other.year_ && month_ == other.month_ && day_ == other.day_;
  }

  date::
  operator std::chrono::year_month_day() const {
    return std::chrono::year{year_} /
           std::chrono::month{static_cast<unsigned>(month_)} /
           std::chrono::day{static_cast<unsigned>(day_)};
  }

  date::date(std::chrono::year_month_day ymd)
      : year_(static_cast<int>(ymd.year())),
        month_(static_cast<uint8_t>(static_cast<unsigned>(ymd.month()))),
        day_(static_cast<uint8_t>(static_cast<unsigned>(ymd.day()))) {}

} // namespace xb
