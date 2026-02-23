#pragma once

#include <xb/date.hpp>
#include <xb/date_time.hpp>
#include <xb/day_time_duration.hpp>
#include <xb/decimal.hpp>
#include <xb/duration.hpp>
#include <xb/integer.hpp>
#include <xb/time.hpp>
#include <xb/year_month_duration.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xb {

  // Parse XML text to C++ value
  template <typename T>
  T
  parse(std::string_view text);

  // Format C++ value to XML text
  std::string
  format(const std::string& value);
  std::string
  format(bool value);
  std::string
  format(int8_t value);
  std::string
  format(int16_t value);
  std::string
  format(int32_t value);
  std::string
  format(int64_t value);
  std::string
  format(uint8_t value);
  std::string
  format(uint16_t value);
  std::string
  format(uint32_t value);
  std::string
  format(uint64_t value);
  std::string
  format(float value);
  std::string
  format(double value);
  std::string
  format(const xb::integer& value);
  std::string
  format(const xb::decimal& value);
  std::string
  format(const xb::date& value);
  std::string
  format(const xb::time& value);
  std::string
  format(const xb::date_time& value);
  std::string
  format(const xb::duration& value);
  std::string
  format(const xb::year_month_duration& value);
  std::string
  format(const xb::day_time_duration& value);

  // Whitespace modes (per XSD type)
  enum class whitespace_mode { preserve, replace, collapse };

  std::string
  apply_whitespace(std::string_view text, whitespace_mode mode);

  // Hex binary parse/format
  std::vector<std::byte>
  parse_hex_binary(std::string_view text);
  std::string
  format_hex_binary(const std::vector<std::byte>& bytes);

  // Base64 binary parse/format
  std::vector<std::byte>
  parse_base64_binary(std::string_view text);
  std::string
  format_base64_binary(const std::vector<std::byte>& bytes);

} // namespace xb
