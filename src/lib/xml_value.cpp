#include <xb/xml_value.hpp>

#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xb {

  // ===== parse specializations =====

  template <>
  bool
  parse<bool>(std::string_view text) {
    if (text == "true" || text == "1") return true;
    if (text == "false" || text == "0") return false;
    throw std::runtime_error("invalid boolean value: " + std::string(text));
  }

  template <>
  std::string
  parse<std::string>(std::string_view text) {
    return std::string(text);
  }

  namespace {

    template <typename T>
    T
    parse_signed_integer(std::string_view text) {
      // Use from_chars for portability and range checking
      T value{};
      auto [ptr, ec] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (ec == std::errc::result_out_of_range)
        throw std::runtime_error("integer value out of range: " +
                                 std::string(text));
      if (ec != std::errc{} || ptr != text.data() + text.size())
        throw std::runtime_error("invalid integer value: " + std::string(text));
      return value;
    }

    template <typename T>
    T
    parse_unsigned_integer(std::string_view text) {
      // Reject negative values explicitly (from_chars may wrap)
      if (!text.empty() && text[0] == '-')
        throw std::runtime_error("negative value for unsigned type: " +
                                 std::string(text));
      T value{};
      auto [ptr, ec] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (ec == std::errc::result_out_of_range)
        throw std::runtime_error("integer value out of range: " +
                                 std::string(text));
      if (ec != std::errc{} || ptr != text.data() + text.size())
        throw std::runtime_error("invalid integer value: " + std::string(text));
      return value;
    }

  } // namespace

  template <>
  int8_t
  parse<int8_t>(std::string_view text) {
    return parse_signed_integer<int8_t>(text);
  }

  template <>
  int16_t
  parse<int16_t>(std::string_view text) {
    return parse_signed_integer<int16_t>(text);
  }

  template <>
  int32_t
  parse<int32_t>(std::string_view text) {
    return parse_signed_integer<int32_t>(text);
  }

  template <>
  int64_t
  parse<int64_t>(std::string_view text) {
    return parse_signed_integer<int64_t>(text);
  }

  template <>
  uint8_t
  parse<uint8_t>(std::string_view text) {
    return parse_unsigned_integer<uint8_t>(text);
  }

  template <>
  uint16_t
  parse<uint16_t>(std::string_view text) {
    return parse_unsigned_integer<uint16_t>(text);
  }

  template <>
  uint32_t
  parse<uint32_t>(std::string_view text) {
    return parse_unsigned_integer<uint32_t>(text);
  }

  template <>
  uint64_t
  parse<uint64_t>(std::string_view text) {
    return parse_unsigned_integer<uint64_t>(text);
  }

  namespace {

    template <typename T>
    T
    parse_float(std::string_view text) {
      // XSD uses "INF", "-INF", "NaN" instead of C++ conventions
      if (text == "INF") return std::numeric_limits<T>::infinity();
      if (text == "-INF") return -std::numeric_limits<T>::infinity();
      if (text == "NaN") return std::numeric_limits<T>::quiet_NaN();

      T value{};
      auto [ptr, ec] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (ec != std::errc{} || ptr != text.data() + text.size())
        throw std::runtime_error("invalid floating-point value: " +
                                 std::string(text));
      return value;
    }

  } // namespace

  template <>
  float
  parse<float>(std::string_view text) {
    return parse_float<float>(text);
  }

  template <>
  double
  parse<double>(std::string_view text) {
    return parse_float<double>(text);
  }

  template <>
  xb::integer
  parse<xb::integer>(std::string_view text) {
    return xb::integer(text);
  }

  template <>
  xb::decimal
  parse<xb::decimal>(std::string_view text) {
    return xb::decimal(text);
  }

  template <>
  xb::date
  parse<xb::date>(std::string_view text) {
    return xb::date(text);
  }

  template <>
  xb::time
  parse<xb::time>(std::string_view text) {
    return xb::time(text);
  }

  template <>
  xb::date_time
  parse<xb::date_time>(std::string_view text) {
    return xb::date_time(text);
  }

  template <>
  xb::duration
  parse<xb::duration>(std::string_view text) {
    return xb::duration(text);
  }

  template <>
  xb::year_month_duration
  parse<xb::year_month_duration>(std::string_view text) {
    return xb::year_month_duration(text);
  }

  template <>
  xb::day_time_duration
  parse<xb::day_time_duration>(std::string_view text) {
    return xb::day_time_duration(text);
  }

  // ===== format overloads =====

  std::string
  format(const std::string& value) {
    return value;
  }

  std::string
  format(bool value) {
    return value ? "true" : "false";
  }

  std::string
  format(int8_t value) {
    return std::to_string(value);
  }

  std::string
  format(int16_t value) {
    return std::to_string(value);
  }

  std::string
  format(int32_t value) {
    return std::to_string(value);
  }

  std::string
  format(int64_t value) {
    return std::to_string(value);
  }

  std::string
  format(uint8_t value) {
    return std::to_string(value);
  }

  std::string
  format(uint16_t value) {
    return std::to_string(value);
  }

  std::string
  format(uint32_t value) {
    return std::to_string(value);
  }

  std::string
  format(uint64_t value) {
    return std::to_string(value);
  }

  namespace {

    template <typename T>
    std::string
    format_float(T value) {
      if (std::isinf(value)) return value > 0 ? "INF" : "-INF";
      if (std::isnan(value)) return "NaN";

      // Use to_chars for round-trip fidelity
      char buf[64];
      auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
      if (ec != std::errc{})
        throw std::runtime_error("failed to format floating-point value");
      return std::string(buf, ptr);
    }

  } // namespace

  std::string
  format(float value) {
    return format_float(value);
  }

  std::string
  format(double value) {
    return format_float(value);
  }

  std::string
  format(const xb::integer& value) {
    return value.to_string();
  }

  std::string
  format(const xb::decimal& value) {
    return value.to_string();
  }

  std::string
  format(const xb::date& value) {
    return value.to_string();
  }

  std::string
  format(const xb::time& value) {
    return value.to_string();
  }

  std::string
  format(const xb::date_time& value) {
    return value.to_string();
  }

  std::string
  format(const xb::duration& value) {
    return value.to_string();
  }

  std::string
  format(const xb::year_month_duration& value) {
    return value.to_string();
  }

  std::string
  format(const xb::day_time_duration& value) {
    return value.to_string();
  }

  // ===== whitespace =====

  std::string
  apply_whitespace(std::string_view text, whitespace_mode mode) {
    if (mode == whitespace_mode::preserve) return std::string(text);

    std::string result;
    result.reserve(text.size());

    if (mode == whitespace_mode::replace) {
      for (char c : text) {
        if (c == '\t' || c == '\n' || c == '\r')
          result += ' ';
        else
          result += c;
      }
      return result;
    }

    // collapse: replace whitespace chars, collapse runs, trim
    bool in_space = true; // treat leading space as already consumed
    for (char c : text) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        if (!in_space) {
          result += ' ';
          in_space = true;
        }
      } else {
        result += c;
        in_space = false;
      }
    }

    // trim trailing space
    if (!result.empty() && result.back() == ' ') result.pop_back();

    return result;
  }

  // ===== hex binary =====

  namespace {

    int
    hex_digit(char c) {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      throw std::runtime_error(std::string("invalid hex digit: ") + c);
    }

    char
    hex_char(int nibble) {
      return "0123456789ABCDEF"[nibble & 0xF];
    }

  } // namespace

  std::vector<std::byte>
  parse_hex_binary(std::string_view text) {
    if (text.size() % 2 != 0)
      throw std::runtime_error("hex binary string has odd length");

    std::vector<std::byte> result;
    result.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
      int high = hex_digit(text[i]);
      int low = hex_digit(text[i + 1]);
      result.push_back(static_cast<std::byte>((high << 4) | low));
    }
    return result;
  }

  std::string
  format_hex_binary(const std::vector<std::byte>& bytes) {
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto b : bytes) {
      auto val = static_cast<unsigned char>(b);
      result += hex_char(val >> 4);
      result += hex_char(val & 0xF);
    }
    return result;
  }

  // ===== base64 binary =====

  namespace {

    // clang-format off
    constexpr char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    // clang-format on

    int
    base64_index(char c) {
      if (c >= 'A' && c <= 'Z') return c - 'A';
      if (c >= 'a' && c <= 'z') return c - 'a' + 26;
      if (c >= '0' && c <= '9') return c - '0' + 52;
      if (c == '+') return 62;
      if (c == '/') return 63;
      throw std::runtime_error(std::string("invalid base64 character: ") + c);
    }

  } // namespace

  std::vector<std::byte>
  parse_base64_binary(std::string_view text) {
    // Strip whitespace
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') clean += c;
    }

    if (clean.empty()) return {};

    // Remove padding
    while (!clean.empty() && clean.back() == '=')
      clean.pop_back();

    std::vector<std::byte> result;
    result.reserve((clean.size() * 3) / 4);

    uint32_t accum = 0;
    int bits = 0;
    for (char c : clean) {
      accum = (accum << 6) | static_cast<uint32_t>(base64_index(c));
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        result.push_back(static_cast<std::byte>((accum >> bits) & 0xFF));
      }
    }

    return result;
  }

  std::string
  format_base64_binary(const std::vector<std::byte>& bytes) {
    if (bytes.empty()) return "";

    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 2 < bytes.size()) {
      uint32_t triplet = (static_cast<uint32_t>(bytes[i]) << 16) |
                         (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                         static_cast<uint32_t>(bytes[i + 2]);
      result += base64_chars[(triplet >> 18) & 0x3F];
      result += base64_chars[(triplet >> 12) & 0x3F];
      result += base64_chars[(triplet >> 6) & 0x3F];
      result += base64_chars[triplet & 0x3F];
      i += 3;
    }

    if (i + 1 == bytes.size()) {
      uint32_t val = static_cast<uint32_t>(bytes[i]) << 16;
      result += base64_chars[(val >> 18) & 0x3F];
      result += base64_chars[(val >> 12) & 0x3F];
      result += '=';
      result += '=';
    } else if (i + 2 == bytes.size()) {
      uint32_t val = (static_cast<uint32_t>(bytes[i]) << 16) |
                     (static_cast<uint32_t>(bytes[i + 1]) << 8);
      result += base64_chars[(val >> 18) & 0x3F];
      result += base64_chars[(val >> 12) & 0x3F];
      result += base64_chars[(val >> 6) & 0x3F];
      result += '=';
    }

    return result;
  }

} // namespace xb
