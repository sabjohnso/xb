#ifndef XB_WIRE_LAYOUT_VALIDATION_HPP
#define XB_WIRE_LAYOUT_VALIDATION_HPP

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace xb::wire {

  struct validation_result {
    bool valid = true;
    std::string field_name;
    std::string message;
  };

  namespace detail {

    // Minimum bit width required for a given XSD built-in type.
    // Returns 0 if the type has no minimum (string, binary, unknown).
    inline unsigned
    min_bits_for_xsd_type(std::string_view xsd_local) {
      // Strip "xs:" prefix if present
      if (xsd_local.starts_with("xs:")) xsd_local = xsd_local.substr(3);

      // Unsigned integers
      if (xsd_local == "unsignedByte") return 8;
      if (xsd_local == "unsignedShort") return 16;
      if (xsd_local == "unsignedInt") return 32;
      if (xsd_local == "unsignedLong") return 64;

      // Signed integers
      if (xsd_local == "byte") return 8;
      if (xsd_local == "short") return 16;
      if (xsd_local == "int") return 32;
      if (xsd_local == "long") return 64;

      // Floating point
      if (xsd_local == "float") return 32;
      if (xsd_local == "double") return 64;

      // Boolean — at least 1 bit
      if (xsd_local == "boolean") return 1;

      // String, binary, date/time, arbitrary-precision — no minimum
      // (string width depends on encoding, not type)
      return 0;
    }

  } // namespace detail

  /// Validate that a BES field width is sufficient for an XSD type.
  inline validation_result
  validate_field_width(const std::string& field_name, unsigned bits,
                       const std::string& xsd_type) {
    if (xsd_type.empty()) return {true, field_name, ""};

    unsigned min = detail::min_bits_for_xsd_type(xsd_type);
    if (min == 0 || bits >= min) return {true, field_name, ""};

    return {false, field_name,
            "field '" + field_name + "': " + std::to_string(bits) +
                " bits too narrow for " + xsd_type + " (needs at least " +
                std::to_string(min) + ")"};
  }

  /// Validate all fields in a BES message against their XSD type
  /// annotations.  Returns a list of errors (empty if valid).
  template <typename Message>
  std::vector<validation_result>
  validate_message_fields(const Message& msg) {
    std::vector<validation_result> errors;

    for (const auto& item : msg.choice) {
      std::visit(
          [&](const auto& v) {
            if constexpr (requires {
                            v.name;
                            v.bits;
                            v.type;
                          }) {
              if (v.type.has_value() && !v.type->empty()) {
                auto result = validate_field_width(v.name, v.bits, *v.type);
                if (!result.valid) errors.push_back(std::move(result));
              }
            }
          },
          item);
    }

    return errors;
  }

} // namespace xb::wire

#endif // XB_WIRE_LAYOUT_VALIDATION_HPP
