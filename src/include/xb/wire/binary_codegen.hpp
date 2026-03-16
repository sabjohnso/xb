#ifndef XB_WIRE_BINARY_CODEGEN_HPP
#define XB_WIRE_BINARY_CODEGEN_HPP

#include <xb/wire/layout_engine.hpp>

#include <sstream>
#include <string>
#include <variant>

namespace xb::wire {

  namespace detail {

    // Map field width to unsigned C++ integer type name.
    inline std::string
    uint_type_for_width(unsigned bits) {
      if (bits <= 8) return "std::uint8_t";
      if (bits <= 16) return "std::uint16_t";
      if (bits <= 32) return "std::uint32_t";
      return "std::uint64_t";
    }

    // Map field width to signed C++ integer type name.
    inline std::string
    int_type_for_width(unsigned bits) {
      if (bits <= 8) return "std::int8_t";
      if (bits <= 16) return "std::int16_t";
      if (bits <= 32) return "std::int32_t";
      return "std::int64_t";
    }

    // Concept: has encoding member (optional<primitive_encoding_type>)
    template <typename T>
    concept has_encoding = requires(T t) { t.encoding; };

    // Concept: has byte_order member
    template <typename T>
    concept has_byte_order = requires(T t) { t.byte_order; };

    // Concept: has null_value member (field_type with sentinel)
    template <typename T>
    concept has_null_value = requires(T t) { t.null_value; };

    // Concept: has value member (constant_type)
    template <typename T>
    concept has_value_member = requires(T t) { t.value; };

    // Resolve byte order: per-field override, then defaults
    template <typename Field, typename Defaults>
    std::string
    resolve_endian(const Field& f, const Defaults& defaults) {
      // Check per-field byte order
      if constexpr (has_byte_order<Field>) {
        if (f.byte_order.has_value()) {
          auto bo = *f.byte_order;
          // Compare using to_string
          auto s = to_string(bo);
          if (s == "little-endian") return "std::endian::little";
          if (s == "native") return "std::endian::native";
          return "std::endian::big";
        }
      }
      // Fall back to defaults
      if (defaults.byte_order.has_value()) {
        auto s = to_string(*defaults.byte_order);
        if (s == "little-endian") return "std::endian::little";
        if (s == "native") return "std::endian::native";
      }
      return "std::endian::big";
    }

    // Determine if a field's encoding is a string type
    template <typename Field>
    bool
    field_is_string(const Field& f) {
      if constexpr (has_encoding<Field>) {
        if (f.encoding.has_value()) {
          auto s = to_string(*f.encoding);
          return s == "ascii" || s == "utf-8" || s == "utf-16";
        }
      }
      return false;
    }

    // Determine if a field's encoding is raw binary
    template <typename Field>
    bool
    field_is_raw(const Field& f) {
      if constexpr (has_encoding<Field>) {
        if (f.encoding.has_value()) { return to_string(*f.encoding) == "raw"; }
      }
      return false;
    }

    // Determine if a field's encoding is signed (twos_complement)
    template <typename Field>
    bool
    field_is_signed(const Field& f) {
      if constexpr (has_encoding<Field>) {
        if (f.encoding.has_value()) {
          return to_string(*f.encoding) == "twos-complement";
        }
      }
      return false;
    }

    // Determine if a field has a null_value sentinel
    template <typename Field>
    bool
    field_has_null_value(const Field& f) {
      if constexpr (has_null_value<Field>) { return f.null_value.has_value(); }
      return false;
    }

    // Get the null_value string from a field
    template <typename Field>
    std::string
    field_null_value(const Field& f) {
      if constexpr (has_null_value<Field>) {
        if (f.null_value.has_value()) return *f.null_value;
      }
      return "";
    }

    // Check if offset is byte-aligned and width is a standard size
    inline bool
    is_byte_aligned(unsigned offset_bits, unsigned width_bits) {
      return (offset_bits % 8 == 0) && (width_bits % 8 == 0) &&
             (width_bits == 8 || width_bits == 16 || width_bits == 32 ||
              width_bits == 64);
    }

    // Emit accessor for a byte-aligned integer field using from_wire
    inline void
    emit_aligned_int_accessor(std::ostringstream& out, const std::string& name,
                              const std::string& cpp_type,
                              const std::string& endian, unsigned offset_bytes,
                              unsigned width_bytes) {
      out << "  auto " << name << "() const -> " << cpp_type << " {\n";
      if (width_bytes == 1) {
        out << "    return static_cast<" << cpp_type << ">(buf_["
            << offset_bytes << "]);\n";
      } else {
        // Use memcpy to avoid aliasing issues, then from_wire
        out << "    " << cpp_type << " v;\n";
        out << "    std::memcpy(&v, buf_.data() + " << offset_bytes
            << ", sizeof(v));\n";
        out << "    return xb::wire::from_wire<" << endian << ">(v);\n";
      }
      out << "  }\n";
    }

    // Emit accessor for a sub-byte field using extract_bits
    inline void
    emit_bitfield_accessor(std::ostringstream& out, const std::string& name,
                           const std::string& cpp_type, unsigned offset_bits,
                           unsigned width_bits) {
      out << "  auto " << name << "() const -> " << cpp_type << " {\n";
      out << "    return static_cast<" << cpp_type
          << ">(xb::wire::extract_bits<" << offset_bits << ", " << width_bits
          << ">(buf_));\n";
      out << "  }\n";
    }

    // Emit accessor for a raw binary field returning span
    inline void
    emit_raw_accessor(std::ostringstream& out, const std::string& name,
                      unsigned offset_bytes, unsigned width_bytes) {
      out << "  auto " << name << "() const -> std::span<const std::byte> {\n";
      out << "    return buf_.subspan(" << offset_bytes << ", " << width_bytes
          << ");\n";
      out << "  }\n";
    }

    // Emit accessor for a fixed-width string field
    inline void
    emit_string_accessor(std::ostringstream& out, const std::string& name,
                         unsigned offset_bytes, unsigned width_bytes) {
      out << "  auto " << name << "() const -> std::string_view {\n";
      out << "    return std::string_view("
          << "reinterpret_cast<const char*>(buf_.data() + " << offset_bytes
          << "), " << width_bytes << ");\n";
      out << "  }\n";
    }

    // Emit optional accessor for a byte-aligned integer with null sentinel
    inline void
    emit_optional_aligned_int_accessor(std::ostringstream& out,
                                       const std::string& name,
                                       const std::string& cpp_type,
                                       const std::string& endian,
                                       unsigned offset_bytes,
                                       unsigned width_bytes,
                                       const std::string& null_val) {
      out << "  auto " << name << "() const -> std::optional<" << cpp_type
          << "> {\n";
      if (width_bytes == 1) {
        out << "    auto v = static_cast<" << cpp_type << ">(buf_["
            << offset_bytes << "]);\n";
      } else {
        out << "    " << cpp_type << " v;\n";
        out << "    std::memcpy(&v, buf_.data() + " << offset_bytes
            << ", sizeof(v));\n";
        out << "    v = xb::wire::from_wire<" << endian << ">(v);\n";
      }
      out << "    if (v == static_cast<" << cpp_type << ">(" << null_val
          << ")) return std::nullopt;\n";
      out << "    return v;\n";
      out << "  }\n";
    }

    // Emit optional accessor for a sub-byte field with null sentinel
    inline void
    emit_optional_bitfield_accessor(std::ostringstream& out,
                                    const std::string& name,
                                    const std::string& cpp_type,
                                    unsigned offset_bits, unsigned width_bits,
                                    const std::string& null_val) {
      out << "  auto " << name << "() const -> std::optional<" << cpp_type
          << "> {\n";
      out << "    auto v = static_cast<" << cpp_type
          << ">(xb::wire::extract_bits<" << offset_bits << ", " << width_bits
          << ">(buf_));\n";
      out << "    if (v == static_cast<" << cpp_type << ">(" << null_val
          << ")) return std::nullopt;\n";
      out << "    return v;\n";
      out << "  }\n";
    }

    // Emit accessor for a constant
    inline void
    emit_constant_accessor(std::ostringstream& out, const std::string& name,
                           const std::string& value) {
      out << "  static constexpr auto " << name << "() { return " << value
          << "; }\n";
    }

    // Build field index: maps field name → index into message.choice
    // for looking up encoding params from the original BES field.
    template <typename Message>
    struct field_source {
      const Message& message;

      // Visit the choice variant at position idx to extract the field
      // data for codegen. Returns true if an accessor was emitted.
      template <typename Defaults>
      bool
      emit_accessor(std::ostringstream& out, const resolved_field& rf,
                    const Defaults& defaults) const {
        switch (rf.category) {
          case field_category::padding:
            return false;
          case field_category::constant: {
            // Find the constant in the message
            for (const auto& item : message.choice) {
              auto emitted = std::visit(
                  [&](const auto& v) -> bool {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (has_value_member<V> && has_name_concept<V> &&
                                  !has_bits_concept<V>) {
                      if (v.name == rf.name) {
                        emit_constant_accessor(out, rf.name, v.value);
                        return true;
                      }
                    }
                    return false;
                  },
                  item);
              if (emitted) return true;
            }
            return false;
          }
          default:
            break;
        }

        // For data/wire_only/computed: find the source field and emit
        for (const auto& item : message.choice) {
          auto emitted = std::visit(
              [&](const auto& v) -> bool {
                using V = std::decay_t<decltype(v)>;
                if constexpr (has_name_concept<V> && has_bits_concept<V>) {
                  if (v.name != rf.name) return false;

                  // Raw binary field?
                  if (field_is_raw(v) && rf.offset_bits.has_value() &&
                      *rf.offset_bits % 8 == 0 && rf.width_bits % 8 == 0) {
                    emit_raw_accessor(out, rf.name, *rf.offset_bits / 8,
                                      rf.width_bits / 8);
                    return true;
                  }

                  // String field?
                  if (field_is_string(v) && rf.offset_bits.has_value() &&
                      *rf.offset_bits % 8 == 0 && rf.width_bits % 8 == 0) {
                    emit_string_accessor(out, rf.name, *rf.offset_bits / 8,
                                         rf.width_bits / 8);
                    return true;
                  }

                  // Integer field
                  bool is_signed = field_is_signed(v);
                  auto cpp_type = is_signed
                                      ? int_type_for_width(rf.width_bits)
                                      : uint_type_for_width(rf.width_bits);
                  auto endian = resolve_endian(v, defaults);
                  bool has_null = field_has_null_value(v);
                  auto null_val = field_null_value(v);

                  if (has_null && rf.offset_bits.has_value() &&
                      is_byte_aligned(*rf.offset_bits, rf.width_bits)) {
                    emit_optional_aligned_int_accessor(
                        out, rf.name, cpp_type, endian, *rf.offset_bits / 8,
                        rf.width_bits / 8, null_val);
                  } else if (has_null && rf.offset_bits.has_value()) {
                    emit_optional_bitfield_accessor(out, rf.name, cpp_type,
                                                    *rf.offset_bits,
                                                    rf.width_bits, null_val);
                  } else if (rf.offset_bits.has_value() &&
                             is_byte_aligned(*rf.offset_bits, rf.width_bits)) {
                    emit_aligned_int_accessor(out, rf.name, cpp_type, endian,
                                              *rf.offset_bits / 8,
                                              rf.width_bits / 8);
                  } else if (rf.offset_bits.has_value()) {
                    emit_bitfield_accessor(out, rf.name, cpp_type,
                                           *rf.offset_bits, rf.width_bits);
                  }
                  return true;
                }
                return false;
              },
              item);
          if (emitted) return true;
        }
        return false;
      }

      // Concept helpers matching layout_engine's pattern
      template <typename T>
      static constexpr bool has_name_concept = requires(T t) { t.name; };

      template <typename T>
      static constexpr bool has_bits_concept = requires(T t) { t.bits; };
    };

  } // namespace detail

  template <typename Message, typename Defaults>
  std::string
  generate_view_class(const std::string& class_name, const Message& message,
                      const resolved_layout& layout, const Defaults& defaults) {
    std::ostringstream out;

    // wire_size computed early — used in both the constant and the check
    unsigned wire_bytes = layout.is_fixed ? (layout.total_bits + 7) / 8 : 0;

    out << "template <xb::wire::validation_level V = "
           "xb::wire::validation_level::full>\n";
    out << "class " << class_name << " {\n";
    out << "  std::span<const std::byte> buf_;\n";
    out << "public:\n";
    out << "  explicit " << class_name
        << "(std::span<const std::byte> buf) : buf_(buf) {\n";
    if (layout.is_fixed && wire_bytes > 0) {
      out << "    if constexpr (V >= xb::wire::validation_level::structural) "
             "{\n";
      out << "      if (buf.size() < wire_size)\n";
      out << "        throw std::runtime_error(\"" << class_name
          << ": buffer too small\");\n";
      out << "    }\n";
    }
    out << "  }\n\n";

    detail::field_source<Message> source{message};

    for (const auto& rf : layout.fields) {
      source.emit_accessor(out, rf, defaults);
      out << "\n";
    }

    out << "  static constexpr std::size_t wire_size = " << wire_bytes << ";\n";

    out << "};\n";

    return out.str();
  }

} // namespace xb::wire

#endif // XB_WIRE_BINARY_CODEGEN_HPP
