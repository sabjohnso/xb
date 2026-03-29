#include <xb/wire/bes_value_generator.hpp>

#include <cstdint>
#include <string>

namespace xb::wire {

  namespace {

    using bes::primitive_encoding_type;

    void
    add_if_unique(std::vector<bes_test_value>& values, std::string xml_text,
                  std::string reason) {
      for (const auto& v : values) {
        if (v.xml_text == xml_text) return;
      }
      values.push_back({std::move(xml_text), std::move(reason)});
    }

    // Compute 2^n - 1 as a string for n <= 64.
    std::string
    unsigned_max(unsigned bits) {
      if (bits >= 64) return "18446744073709551615";
      uint64_t max = (uint64_t{1} << bits) - 1;
      return std::to_string(max);
    }

    // Compute 2^(n-1) as a string (the magnitude of the signed min).
    std::string
    signed_min(unsigned bits) {
      if (bits >= 64) return "-9223372036854775808";
      int64_t min = -(int64_t{1} << (bits - 1));
      return std::to_string(min);
    }

    // Compute 2^(n-1) - 1 as a string.
    std::string
    signed_max(unsigned bits) {
      if (bits >= 64) return "9223372036854775807";
      int64_t max = (int64_t{1} << (bits - 1)) - 1;
      return std::to_string(max);
    }

    // Decrement a non-negative decimal integer string by 1.
    std::string
    decrement_str(const std::string& s) {
      std::string result = s;
      int borrow = 1;
      for (auto it = result.rbegin(); it != result.rend() && borrow; ++it) {
        if (*it < '0' || *it > '9') break;
        int digit = (*it - '0') - borrow;
        if (digit < 0) {
          digit += 10;
          borrow = 1;
        } else {
          borrow = 0;
        }
        *it = static_cast<char>('0' + digit);
      }
      auto first = result.find_first_not_of('0');
      if (first == std::string::npos) return "0";
      if (first > 0) result.erase(0, first);
      return result;
    }

    void
    generate_unsigned(std::vector<bes_test_value>& values, unsigned bits) {
      std::string max_val = unsigned_max(bits);

      add_if_unique(values, "0", "wire-min");
      add_if_unique(values, "1", "wire-min+1");
      add_if_unique(values, decrement_str(max_val), "wire-max-1");
      add_if_unique(values, max_val, "wire-max");
    }

    void
    generate_signed(std::vector<bes_test_value>& values, unsigned bits) {
      std::string min_val = signed_min(bits);
      std::string max_val = signed_max(bits);

      add_if_unique(values, min_val, "wire-min");
      // min+1: strip the '-' sign, decrement magnitude, add sign back
      std::string min_mag = min_val.substr(1);
      std::string min_plus_1_mag = decrement_str(min_mag);
      add_if_unique(values, "-" + min_plus_1_mag, "wire-min+1");
      add_if_unique(values, "0", "zero");
      add_if_unique(values, decrement_str(max_val), "wire-max-1");
      add_if_unique(values, max_val, "wire-max");
    }

  } // namespace

  bes_test_value_set
  bes_value_generator::generate(const bes::field_type& field) {
    bes_test_value_set result;
    result.field_name = field.name;

    auto enc = field.encoding.value_or(primitive_encoding_type::unsigned_);

    switch (enc) {
      case primitive_encoding_type::unsigned_:
      case primitive_encoding_type::epoch:
      case primitive_encoding_type::epoch_millis:
      case primitive_encoding_type::epoch_micros:
      case primitive_encoding_type::epoch_nanos:
        generate_unsigned(result.values, field.bits);
        break;

      case primitive_encoding_type::twos_complement:
        generate_signed(result.values, field.bits);
        break;

      case primitive_encoding_type::boolean:
        add_if_unique(result.values, "true", "boolean-true");
        add_if_unique(result.values, "false", "boolean-false");
        break;

      case primitive_encoding_type::ieee754:
        add_if_unique(result.values, "0", "zero");
        add_if_unique(result.values, "1", "one");
        add_if_unique(result.values, "-1", "minus-one");
        add_if_unique(result.values, "0.5", "fractional");
        break;

      case primitive_encoding_type::fixed_point:
        add_if_unique(result.values, "0", "zero");
        add_if_unique(result.values, "1", "one");
        add_if_unique(result.values, "-1", "minus-one");
        break;

      case primitive_encoding_type::ascii:
      case primitive_encoding_type::utf8:
      case primitive_encoding_type::utf16: {
        // Character count from bit width
        unsigned char_bits = (enc == primitive_encoding_type::utf16) ? 16 : 8;
        unsigned max_chars = field.bits / char_bits;
        add_if_unique(result.values, "", "empty");
        if (max_chars > 0) {
          add_if_unique(result.values, "a", "single-char");
          add_if_unique(result.values, std::string(max_chars, 'a'),
                        "full-length");
        }
        break;
      }

      case primitive_encoding_type::raw:
        add_if_unique(result.values, "", "empty");
        break;

      case primitive_encoding_type::bcd:
      case primitive_encoding_type::ascii_decimal:
        // BCD: each nibble is a digit, so N bits = N/4 digits
        generate_unsigned(result.values, field.bits);
        break;

      case primitive_encoding_type::enum_integer:
      case primitive_encoding_type::enum_char:
        if (field.enum_map) {
          std::size_t i = 0;
          for (const auto& entry : field.enum_map->entry) {
            ++i;
            add_if_unique(result.values, entry.xsd_value,
                          "enum-" + std::to_string(i) + "-of-" +
                              std::to_string(field.enum_map->entry.size()));
          }
        } else {
          // No map: fall back to integer boundaries
          generate_unsigned(result.values, field.bits);
        }
        break;

      case primitive_encoding_type::bitset:
        add_if_unique(result.values, "0", "all-clear");
        add_if_unique(result.values, unsigned_max(field.bits), "all-set");
        break;

      case primitive_encoding_type::bcd_datetime:
        add_if_unique(result.values, "2024-01-01T00:00:00Z", "epoch-ish");
        break;
    }

    // Add null sentinel value if present
    if (field.null_value) {
      add_if_unique(result.values, *field.null_value, "null-sentinel");
    }

    return result;
  }

} // namespace xb::wire
