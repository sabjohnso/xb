#ifndef XB_WIRE_BINARY_CODEGEN_HPP
#define XB_WIRE_BINARY_CODEGEN_HPP

#include <xb/wire/layout_engine.hpp>

#include <algorithm>
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

    // Concept: has bit_order member
    template <typename T>
    concept has_bit_order = requires(T t) { t.bit_order; };

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

    // Resolve bit order: per-field override, then defaults.
    // Returns the C++ expression for the bit_order template argument,
    // or empty string for the default (msb_first).
    template <typename Field, typename Defaults>
    std::string
    resolve_bit_order(const Field& f, const Defaults& defaults) {
      if constexpr (has_bit_order<Field>) {
        if (f.bit_order.has_value()) {
          auto s = to_string(*f.bit_order);
          if (s == "lsb-first") return "xb::wire::bit_order::lsb_first";
          return "";
        }
      }
      if (defaults.bit_order.has_value()) {
        auto s = to_string(*defaults.bit_order);
        if (s == "lsb-first") return "xb::wire::bit_order::lsb_first";
      }
      return "";
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
      if (width_bytes == 1) {
        // Single-byte access is constexpr-safe
        out << "  constexpr auto " << name << "() const -> " << cpp_type
            << " {\n";
        out << "    return static_cast<" << cpp_type << ">(buf_["
            << offset_bytes << "]);\n";
      } else {
        // Multi-byte memcpy is not constexpr in C++20
        out << "  auto " << name << "() const -> " << cpp_type << " {\n";
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
                           unsigned width_bits,
                           const std::string& bit_order_str = "") {
      out << "  constexpr auto " << name << "() const -> " << cpp_type
          << " {\n";
      out << "    return static_cast<" << cpp_type
          << ">(xb::wire::extract_bits<" << offset_bits << ", " << width_bits;
      if (!bit_order_str.empty()) out << ", " << bit_order_str;
      out << ">(buf_));\n";
      out << "  }\n";
    }

    // Emit accessor for a raw binary field returning span
    inline void
    emit_raw_accessor(std::ostringstream& out, const std::string& name,
                      unsigned offset_bytes, unsigned width_bytes) {
      out << "  auto " << name << "() const -> std::span<const std::byte> {\n";
      out << "    return std::span<const std::byte>(buf_).subspan("
          << offset_bytes << ", " << width_bytes << ");\n";
      out << "  }\n";
    }

    // Emit accessor for a fixed-width string field (trimmed).
    // Strips trailing padding characters (space by default).
    inline void
    emit_string_accessor(std::ostringstream& out, const std::string& name,
                         unsigned offset_bytes, unsigned width_bytes,
                         char pad_char = ' ') {
      out << "  auto " << name << "() const -> std::string_view {\n";
      out << "    std::string_view raw("
          << "reinterpret_cast<const char*>(buf_.data() + " << offset_bytes
          << "), " << width_bytes << ");\n";
      // Emit the trim: find_last_not_of the padding char
      if (pad_char == '\0') {
        out << "    auto pos = raw.find_last_not_of('\\0');\n";
      } else {
        out << "    auto pos = raw.find_last_not_of('" << pad_char << "');\n";
      }
      out << "    return pos == std::string_view::npos "
          << "? std::string_view{} : raw.substr(0, pos + 1);\n";
      out << "  }\n";
    }

    // Emit raw accessor for a fixed-width string field (unstripped).
    // Returns the full-width string_view including padding.
    inline void
    emit_raw_string_accessor(std::ostringstream& out, const std::string& name,
                             unsigned offset_bytes, unsigned width_bytes) {
      out << "  auto raw_" << name << "() const -> std::string_view {\n";
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
                                    const std::string& null_val,
                                    const std::string& bit_order_str = "") {
      out << "  constexpr auto " << name << "() const -> std::optional<"
          << cpp_type << "> {\n";
      out << "    auto v = static_cast<" << cpp_type
          << ">(xb::wire::extract_bits<" << offset_bits << ", " << width_bits;
      if (!bit_order_str.empty()) out << ", " << bit_order_str;
      out << ">(buf_));\n";
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

    // --- Write mutator emitters ---

    // Emit mutator for a byte-aligned integer field using to_wire
    inline void
    emit_aligned_int_mutator(std::ostringstream& out, const std::string& name,
                             const std::string& cpp_type,
                             const std::string& endian, unsigned offset_bytes,
                             unsigned width_bytes) {
      out << "  void set_" << name << "(" << cpp_type << " v) {\n";
      if (width_bytes == 1) {
        out << "    buf_[" << offset_bytes
            << "] = static_cast<std::byte>(v);\n";
      } else {
        out << "    v = xb::wire::to_wire<" << endian << ">(v);\n";
        out << "    std::memcpy(buf_.data() + " << offset_bytes
            << ", &v, sizeof(v));\n";
      }
      out << "  }\n";
    }

    // Emit mutator for a sub-byte field using insert_bits
    inline void
    emit_bitfield_mutator(std::ostringstream& out, const std::string& name,
                          const std::string& cpp_type, unsigned offset_bits,
                          unsigned width_bits,
                          const std::string& bit_order_str = "") {
      out << "  void set_" << name << "(" << cpp_type << " v) {\n";
      out << "    xb::wire::insert_bits<" << offset_bits << ", " << width_bits;
      if (!bit_order_str.empty()) out << ", " << bit_order_str;
      out << ">(buf_, static_cast<xb::wire::detail::uint_for_width_t<"
          << width_bits << ">>(v));\n";
      out << "  }\n";
    }

    // Emit mutator for a fixed-width string field
    inline void
    emit_string_mutator(std::ostringstream& out, const std::string& name,
                        unsigned offset_bytes, unsigned width_bytes) {
      out << "  void set_" << name << "(std::string_view v) {\n";
      out << "    auto n = std::min(v.size(), std::size_t{" << width_bytes
          << "});\n";
      out << "    std::memcpy(buf_.data() + " << offset_bytes
          << ", v.data(), n);\n";
      out << "    std::memset(buf_.data() + " << offset_bytes << " + n, ' ', "
          << width_bytes << " - n);\n";
      out << "  }\n";
    }

    // Emit mutator for a raw binary field
    inline void
    emit_raw_mutator(std::ostringstream& out, const std::string& name,
                     unsigned offset_bytes, unsigned width_bytes) {
      out << "  void set_" << name << "(std::span<const std::byte> v) {\n";
      out << "    auto n = std::min(v.size(), std::size_t{" << width_bytes
          << "});\n";
      out << "    std::memcpy(buf_.data() + " << offset_bytes
          << ", v.data(), n);\n";
      out << "  }\n";
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
                    // Determine padding character from defaults
                    char pad = ' '; // default: space
                    if constexpr (requires { defaults.string_padding; }) {
                      if (defaults.string_padding.has_value()) {
                        using pt =
                            std::decay_t<decltype(*defaults.string_padding)>;
                        if constexpr (requires(pt p) { to_string(p); }) {
                          auto s = to_string(*defaults.string_padding);
                          if (s == "null" || s == "zero") pad = '\0';
                        }
                      }
                    }
                    emit_string_accessor(out, rf.name, *rf.offset_bits / 8,
                                         rf.width_bits / 8, pad);
                    emit_raw_string_accessor(out, rf.name, *rf.offset_bits / 8,
                                             rf.width_bits / 8);
                    return true;
                  }

                  // Integer field
                  bool is_signed = field_is_signed(v);
                  auto cpp_type = is_signed
                                      ? int_type_for_width(rf.width_bits)
                                      : uint_type_for_width(rf.width_bits);
                  auto endian = resolve_endian(v, defaults);
                  auto bit_ord = resolve_bit_order(v, defaults);
                  bool has_null = field_has_null_value(v);
                  auto null_val = field_null_value(v);

                  if (has_null && rf.offset_bits.has_value() &&
                      is_byte_aligned(*rf.offset_bits, rf.width_bits)) {
                    emit_optional_aligned_int_accessor(
                        out, rf.name, cpp_type, endian, *rf.offset_bits / 8,
                        rf.width_bits / 8, null_val);
                  } else if (has_null && rf.offset_bits.has_value()) {
                    emit_optional_bitfield_accessor(
                        out, rf.name, cpp_type, *rf.offset_bits, rf.width_bits,
                        null_val, bit_ord);
                  } else if (rf.offset_bits.has_value() &&
                             is_byte_aligned(*rf.offset_bits, rf.width_bits)) {
                    emit_aligned_int_accessor(out, rf.name, cpp_type, endian,
                                              *rf.offset_bits / 8,
                                              rf.width_bits / 8);
                  } else if (rf.offset_bits.has_value()) {
                    emit_bitfield_accessor(out, rf.name, cpp_type,
                                           *rf.offset_bits, rf.width_bits,
                                           bit_ord);
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

      // Emit a write mutator for a resolved field.
      template <typename Defaults>
      bool
      emit_mutator(std::ostringstream& out, const resolved_field& rf,
                   const Defaults& defaults) const {
        // No mutators for padding or constants
        if (rf.category == field_category::padding ||
            rf.category == field_category::constant)
          return false;

        for (const auto& item : message.choice) {
          auto emitted = std::visit(
              [&](const auto& v) -> bool {
                using V = std::decay_t<decltype(v)>;
                if constexpr (has_name_concept<V> && has_bits_concept<V>) {
                  if (v.name != rf.name) return false;

                  // Raw binary field
                  if (field_is_raw(v) && rf.offset_bits.has_value() &&
                      *rf.offset_bits % 8 == 0 && rf.width_bits % 8 == 0) {
                    emit_raw_mutator(out, rf.name, *rf.offset_bits / 8,
                                     rf.width_bits / 8);
                    return true;
                  }

                  // String field
                  if (field_is_string(v) && rf.offset_bits.has_value() &&
                      *rf.offset_bits % 8 == 0 && rf.width_bits % 8 == 0) {
                    emit_string_mutator(out, rf.name, *rf.offset_bits / 8,
                                        rf.width_bits / 8);
                    return true;
                  }

                  // Integer field
                  bool is_signed = field_is_signed(v);
                  auto cpp_type = is_signed
                                      ? int_type_for_width(rf.width_bits)
                                      : uint_type_for_width(rf.width_bits);
                  auto endian = resolve_endian(v, defaults);
                  auto bit_ord = resolve_bit_order(v, defaults);

                  if (rf.offset_bits.has_value() &&
                      is_byte_aligned(*rf.offset_bits, rf.width_bits)) {
                    emit_aligned_int_mutator(out, rf.name, cpp_type, endian,
                                             *rf.offset_bits / 8,
                                             rf.width_bits / 8);
                  } else if (rf.offset_bits.has_value()) {
                    emit_bitfield_mutator(out, rf.name, cpp_type,
                                          *rf.offset_bits, rf.width_bits,
                                          bit_ord);
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

      // Resolve the C++ return type string for a field in the layout.
      // Used by concept generation, base class generation, and accessor
      // emission.
      template <typename Defaults>
      std::string
      field_return_type(const resolved_field& rf,
                        const Defaults& /*defaults*/) const {
        if (rf.category == field_category::constant) return "auto";
        if (rf.category == field_category::padding) return "";

        for (const auto& item : message.choice) {
          auto result = std::visit(
              [&](const auto& v) -> std::string {
                using V = std::decay_t<decltype(v)>;
                if constexpr (has_name_concept<V> && has_bits_concept<V>) {
                  if (v.name != rf.name) return "";

                  if (field_is_raw(v)) return "std::span<const std::byte>";
                  if (field_is_string(v)) return "std::string_view";

                  bool is_signed = field_is_signed(v);
                  auto cpp_type = is_signed
                                      ? int_type_for_width(rf.width_bits)
                                      : uint_type_for_width(rf.width_bits);

                  if (field_has_null_value(v))
                    return "std::optional<" + cpp_type + ">";

                  return cpp_type;
                }
                return "";
              },
              item);
          if (!result.empty()) return result;
        }
        return "";
      }

      // Concept helpers matching layout_engine's pattern
      template <typename T>
      static constexpr bool has_name_concept = requires(T t) { t.name; };

      template <typename T>
      static constexpr bool has_bits_concept = requires(T t) { t.bits; };
    };

  } // namespace detail

  /// Emit accessors (and optionally mutators) for choice alternative fields.
  template <typename Message, typename Defaults>
  void
  emit_choice_fields(std::ostringstream& out, const Message& message,
                     const resolved_layout& layout, const Defaults& defaults,
                     bool emit_mutators) {
    for (const auto& rc : layout.choices) {
      out << "\n  // --- choice: " << rc.name
          << " (discriminant: " << rc.discriminant_field << ") ---\n";
      for (const auto& ra : rc.alternatives) {
        for (const auto& item : message.choice) {
          std::visit(
              [&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (detail::is_unique_ptr<V>::value) {
                  using P = typename V::element_type;
                  if constexpr (detail::has_alternative<P>) {
                    for (const auto& alt_ptr : v->alternative) {
                      if (!alt_ptr || alt_ptr->name != ra.name) continue;
                      detail::field_source<std::decay_t<decltype(*alt_ptr)>>
                          alt_source{*alt_ptr};
                      for (const auto& rf : ra.fields) {
                        if (rf.category == field_category::padding) continue;
                        out << "  // [" << ra.name << "]\n";
                        alt_source.emit_accessor(out, rf, defaults);
                        if (emit_mutators)
                          alt_source.emit_mutator(out, rf, defaults);
                        out << "\n";
                      }
                    }
                  }
                }
              },
              item);
        }
      }
    }
  }

  /// Emit typed element accessor for each repeat with element-type.
  inline void
  emit_repeat_accessors(std::ostringstream& out, const resolved_layout& layout,
                        bool is_const) {
    for (const auto& rr : layout.repeats) {
      if (rr.element_type.empty()) continue;

      auto view_class = rr.element_type + "_view<>";
      auto owned_class = rr.element_type + "_owned";
      unsigned header_bytes = (rr.offset_bits + 7) / 8;

      out << "\n  // --- repeat: " << rr.count_field << " × " << rr.element_type
          << " ---\n";

      if (is_const) {
        out << "  auto element(std::size_t i) const -> " << view_class
            << " {\n";
        out << "    auto off = " << header_bytes << " + i * " << owned_class
            << "::wire_size;\n";
        out << "    return " << view_class
            << "::from_trusted(std::span<const std::byte>(buf_).subspan(off, "
            << owned_class << "::wire_size));\n";
        out << "  }\n";
      } else {
        // Mutable version returns mutable_view
        auto mut_view = rr.element_type + "_mutable_view<>";
        out << "  auto element(std::size_t i) -> " << mut_view << " {\n";
        out << "    auto off = " << header_bytes << " + i * " << owned_class
            << "::wire_size;\n";
        out << "    return " << mut_view
            << "(std::span<std::byte>(buf_).subspan(off, " << owned_class
            << "::wire_size));\n";
        out << "  }\n";
        // Also provide const version
        out << "  auto element(std::size_t i) const -> " << view_class
            << " {\n";
        out << "    auto off = " << header_bytes << " + i * " << owned_class
            << "::wire_size;\n";
        out << "    return " << view_class
            << "::from_trusted(std::span<const std::byte>(buf_).subspan(off, "
            << owned_class << "::wire_size));\n";
        out << "  }\n";
      }
    }
  }

  template <typename Message, typename Defaults>
  std::string
  generate_view_class(const std::string& class_name, const Message& message,
                      const resolved_layout& layout, const Defaults& defaults) {
    std::ostringstream out;

    // wire_size computed early — used in both the constant and the check
    unsigned wire_bytes = (layout.total_bits + 7) / 8;

    out << "template <xb::wire::validation_level V = "
           "xb::wire::validation_level::full>\n";
    out << "class " << class_name << " {\n";
    out << "  std::span<const std::byte> buf_;\n";
    out << "  struct trusted_tag {};\n";
    out << "  constexpr " << class_name
        << "(std::span<const std::byte> buf, trusted_tag) : buf_(buf) {}\n";
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

    // constexpr factory for trusted (pre-validated) buffers
    out << "  static constexpr auto from_trusted(std::span<const std::byte> "
           "buf) {\n";
    out << "    return " << class_name << "(buf, trusted_tag{});\n";
    out << "  }\n\n";

    detail::field_source<Message> source{message};

    // Check if there are any wire-only fields
    bool has_wire_only = std::any_of(
        layout.fields.begin(), layout.fields.end(), [](const auto& rf) {
          return rf.category == field_category::wire_only;
        });

    // Pass 1: data fields
    for (const auto& rf : layout.fields) {
      if (rf.category == field_category::wire_only) continue;
      if (rf.category == field_category::data) out << "  // [data]\n";
      source.emit_accessor(out, rf, defaults);
      out << "\n";
    }

    // Pass 2: wire-only fields with separator
    if (has_wire_only) {
      out << "  // --- wire-only fields ---\n";
      for (const auto& rf : layout.fields) {
        if (rf.category != field_category::wire_only) continue;
        out << "  // [wire-only]\n";
        source.emit_accessor(out, rf, defaults);
        out << "\n";
      }
    }

    // Pass 3: choice alternative fields (read-only)
    emit_choice_fields(out, message, layout, defaults, false);

    // Pass 4: typed repeat element accessors (read-only)
    emit_repeat_accessors(out, layout, true);

    out << "  static constexpr std::size_t wire_size = " << wire_bytes << ";\n";

    out << "};\n";

    return out.str();
  }

  /// Generate a C++ owned class for a fixed-layout BES message.
  /// The class owns a std::vector<std::byte> and provides both
  /// read accessors and write mutators.
  template <typename Message, typename Defaults>
  std::string
  generate_owned_class(const std::string& class_name, const Message& message,
                       const resolved_layout& layout,
                       const Defaults& defaults) {
    std::ostringstream out;

    unsigned wire_bytes = (layout.total_bits + 7) / 8;

    out << "class " << class_name << " {\n";
    out << "  std::vector<std::byte> buf_;\n";
    out << "public:\n";
    out << "  " << class_name << "() : buf_(wire_size, std::byte{0}) {}\n\n";

    detail::field_source<Message> source{message};

    // Collect data field names and types for constructors
    struct param_info {
      std::string name;
      std::string type;
    };

    std::vector<param_info> data_params;
    for (const auto& rf : layout.fields) {
      if (rf.category != field_category::data) continue;
      auto t = source.field_return_type(rf, defaults);
      if (t.empty()) continue;
      // Strip std::optional wrapper for constructor params
      if (t.starts_with("std::optional<") && t.back() == '>') {
        t = t.substr(14, t.size() - 15);
      }
      data_params.push_back({rf.name, t});
    }

    // Emit args struct for designated-initializer construction
    if (!data_params.empty()) {
      out << "  struct args {\n";
      for (const auto& p : data_params) {
        out << "    " << p.type << " " << p.name << "{};\n";
      }
      out << "  };\n\n";

      // Constructor from args
      out << "  explicit " << class_name
          << "(args a) : buf_(wire_size, std::byte{0}) {\n";
      for (const auto& p : data_params) {
        out << "    set_" << p.name << "(a." << p.name << ");\n";
      }
      // Auto-set wire-only discriminant value
      if constexpr (requires { message.discriminant_value; }) {
        if (message.discriminant_value.has_value() &&
            !message.discriminant_value->empty()) {
          // Find the first wire-only field to set
          for (const auto& rf : layout.fields) {
            if (rf.category == field_category::wire_only) {
              out << "    set_" << rf.name << "(" << *message.discriminant_value
                  << ");\n";
              break;
            }
          }
        }
      }
      out << "  }\n\n";

      // Aggregate constructor with positional parameters
      out << "  " << class_name << "(";
      for (std::size_t i = 0; i < data_params.size(); ++i) {
        if (i > 0) out << ", ";
        out << data_params[i].type << " " << data_params[i].name << "_arg";
      }
      out << ")\n      : buf_(wire_size, std::byte{0}) {\n";
      for (const auto& p : data_params) {
        out << "    set_" << p.name << "(" << p.name << "_arg);\n";
      }
      if constexpr (requires { message.discriminant_value; }) {
        if (message.discriminant_value.has_value() &&
            !message.discriminant_value->empty()) {
          for (const auto& rf : layout.fields) {
            if (rf.category == field_category::wire_only) {
              out << "    set_" << rf.name << "(" << *message.discriminant_value
                  << ");\n";
              break;
            }
          }
        }
      }
      out << "  }\n\n";
    }

    bool has_wire_only = std::any_of(
        layout.fields.begin(), layout.fields.end(), [](const auto& rf) {
          return rf.category == field_category::wire_only;
        });

    // Pass 1: data fields
    for (const auto& rf : layout.fields) {
      if (rf.category == field_category::wire_only) continue;
      if (rf.category == field_category::data) out << "  // [data]\n";
      source.emit_accessor(out, rf, defaults);
      source.emit_mutator(out, rf, defaults);
      out << "\n";
    }

    // Pass 2: wire-only fields
    if (has_wire_only) {
      out << "  // --- wire-only fields ---\n";
      for (const auto& rf : layout.fields) {
        if (rf.category != field_category::wire_only) continue;
        out << "  // [wire-only]\n";
        source.emit_accessor(out, rf, defaults);
        source.emit_mutator(out, rf, defaults);
        out << "\n";
      }
    }

    // Pass 3: choice alternative fields (read + write)
    emit_choice_fields(out, message, layout, defaults, true);

    // Pass 4: typed repeat element accessors (read + write)
    emit_repeat_accessors(out, layout, false);

    out << "  auto buffer() const -> std::span<const std::byte> { return "
           "buf_; }\n";
    out << "  auto mutable_buffer() -> std::span<std::byte> { return "
           "buf_; }\n";
    out << "  static constexpr std::size_t wire_size = " << wire_bytes << ";\n";

    out << "};\n";

    return out.str();
  }

  /// Generate a C++ mutable view class for a fixed-layout BES message.
  /// Non-owning: wraps a std::span<std::byte> with both read accessors
  /// and write mutators.
  template <typename Message, typename Defaults>
  std::string
  generate_mutable_view_class(const std::string& class_name,
                              const Message& message,
                              const resolved_layout& layout,
                              const Defaults& defaults) {
    std::ostringstream out;

    unsigned wire_bytes = (layout.total_bits + 7) / 8;

    out << "template <xb::wire::validation_level V = "
           "xb::wire::validation_level::full>\n";
    out << "class " << class_name << " {\n";
    out << "  std::span<std::byte> buf_;\n";
    out << "public:\n";
    out << "  explicit " << class_name
        << "(std::span<std::byte> buf) : buf_(buf) {\n";
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

    bool has_wire_only = std::any_of(
        layout.fields.begin(), layout.fields.end(), [](const auto& rf) {
          return rf.category == field_category::wire_only;
        });

    // Pass 1: data fields
    for (const auto& rf : layout.fields) {
      if (rf.category == field_category::wire_only) continue;
      if (rf.category == field_category::data) out << "  // [data]\n";
      source.emit_accessor(out, rf, defaults);
      source.emit_mutator(out, rf, defaults);
      out << "\n";
    }

    // Pass 2: wire-only fields
    if (has_wire_only) {
      out << "  // --- wire-only fields ---\n";
      for (const auto& rf : layout.fields) {
        if (rf.category != field_category::wire_only) continue;
        out << "  // [wire-only]\n";
        source.emit_accessor(out, rf, defaults);
        source.emit_mutator(out, rf, defaults);
        out << "\n";
      }
    }

    // Pass 3: choice alternative fields (read + write)
    emit_choice_fields(out, message, layout, defaults, true);

    // Pass 4: typed repeat element accessors (read + write)
    emit_repeat_accessors(out, layout, false);

    out << "  static constexpr std::size_t wire_size = " << wire_bytes << ";\n";

    out << "};\n";

    return out.str();
  }

  /// Generate a C++20 concept for a BES message type.
  template <typename Message, typename Defaults>
  std::string
  generate_concept(const std::string& concept_name, const Message& message,
                   const resolved_layout& layout, const Defaults& defaults) {
    std::ostringstream out;

    out << "template <typename T>\n";
    out << "concept " << concept_name << " = requires(const T& t) {\n";

    detail::field_source<Message> source{message};

    bool has_requirements = false;
    for (const auto& rf : layout.fields) {
      // Concepts expose only data fields — skip wire-only and padding
      if (rf.category == field_category::padding) continue;
      if (rf.category == field_category::wire_only) continue;

      auto ret_type = source.field_return_type(rf, defaults);
      if (ret_type.empty()) continue;

      if (rf.category == field_category::constant) {
        out << "  t." << rf.name << "();\n";
      } else {
        out << "  { t." << rf.name << "() } -> std::convertible_to<" << ret_type
            << ">;\n";
      }
      has_requirements = true;
    }

    if (!has_requirements) out << "  requires true;\n";

    out << "};\n";

    return out.str();
  }

  /// Generate an abstract base class for a BES message type.
  template <typename Message, typename Defaults>
  std::string
  generate_base_class(const std::string& class_name, const Message& message,
                      const resolved_layout& layout, const Defaults& defaults) {
    std::ostringstream out;

    out << "class " << class_name << " {\n";
    out << "public:\n";
    out << "  virtual ~" << class_name << "() = default;\n";

    detail::field_source<Message> source{message};

    for (const auto& rf : layout.fields) {
      // Base class exposes only data fields — skip wire-only and padding
      if (rf.category == field_category::padding) continue;
      if (rf.category == field_category::wire_only) continue;

      if (rf.category == field_category::constant) {
        // Constants are static, not virtual — find the value
        for (const auto& item : message.choice) {
          std::visit(
              [&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (detail::field_source<
                                  Message>::template has_name_concept<V>) {
                  if constexpr (detail::has_value_member<V>) {
                    if constexpr (!detail::field_source<
                                      Message>::template has_bits_concept<V>) {
                      if (v.name == rf.name) {
                        detail::emit_constant_accessor(out, rf.name, v.value);
                      }
                    }
                  }
                }
              },
              item);
        }
        continue;
      }

      auto ret_type = source.field_return_type(rf, defaults);
      if (ret_type.empty()) continue;

      out << "  virtual " << ret_type << " " << rf.name << "() const = 0;\n";
    }

    out << "};\n";

    return out.str();
  }

  /// Discriminant field metadata for discriminator generation.
  struct discriminant_info {
    std::string field_name;
    unsigned offset_bits;
    unsigned width_bits;
    std::string endian = "std::endian::big";
  };

  /// Entry mapping a discriminant value to a view class name.
  struct message_entry {
    std::string view_class_name;
    std::string discriminant_value;
  };

  /// Generate a discriminator function that reads a type field and
  /// returns a std::variant of view types.
  inline std::string
  generate_discriminator(const std::string& function_name,
                         const discriminant_info& disc,
                         const std::vector<message_entry>& entries) {
    std::ostringstream out;

    auto disc_type = detail::uint_type_for_width(disc.width_bits);

    // Build variant type list
    out << "template <xb::wire::validation_level V = "
           "xb::wire::validation_level::full>\n";
    out << "auto " << function_name << "(std::span<const std::byte> buf)\n";
    out << "    -> std::variant<";
    for (std::size_t i = 0; i < entries.size(); ++i) {
      if (i > 0) out << ", ";
      out << entries[i].view_class_name << "<V>";
    }
    out << "> {\n";

    // Read the discriminant field
    if (detail::is_byte_aligned(disc.offset_bits, disc.width_bits) &&
        disc.width_bits == 8) {
      out << "  auto disc = static_cast<" << disc_type << ">(buf["
          << disc.offset_bits / 8 << "]);\n";
    } else if (detail::is_byte_aligned(disc.offset_bits, disc.width_bits)) {
      out << "  " << disc_type << " disc;\n";
      out << "  std::memcpy(&disc, buf.data() + " << disc.offset_bits / 8
          << ", sizeof(disc));\n";
      out << "  disc = xb::wire::from_wire<" << disc.endian << ">(disc);\n";
    } else {
      out << "  auto disc = static_cast<" << disc_type
          << ">(xb::wire::extract_bits<" << disc.offset_bits << ", "
          << disc.width_bits << ">(buf));\n";
    }

    // Switch on discriminant value
    for (const auto& entry : entries) {
      out << "  if (disc == static_cast<" << disc_type << ">("
          << entry.discriminant_value << ")) return " << entry.view_class_name
          << "<V>(buf);\n";
    }

    out << "  throw std::runtime_error(\"unknown message type\");\n";
    out << "}\n";

    return out.str();
  }

  /// A layer in a frame parser: a fixed-size header with optional
  /// match condition and optional message iteration.
  struct frame_layer {
    std::string view_class_name; // e.g., "EthernetHeader_view"
    unsigned wire_size_bytes;    // fixed header size
    std::string match_field;     // field to check (empty = no match)
    std::string match_value;     // expected value
    // If this layer has a repeat (message iteration):
    std::string count_field;        // field on this header with count
    std::string block_length_field; // field on block header with msg length
    std::string block_view_class;   // view class for the block header
    unsigned block_wire_size = 0;   // block header size in bytes
  };

  /// Generate a callback-based frame parser that peels layers from a
  /// frame-stack and invokes the callback for each message body.
  /// Returns xb::wire::parse_result indicating success or failure mode.
  inline std::string
  generate_frame_parser(const std::string& function_name,
                        const std::vector<frame_layer>& layers) {
    std::ostringstream out;

    out << "template <typename Callback>\n";
    out << "xb::wire::parse_result " << function_name
        << "(std::span<const std::byte> frame, Callback&& cb) {\n";
    out << "  std::size_t offset = 0;\n\n";

    for (std::size_t i = 0; i < layers.size(); ++i) {
      const auto& layer = layers[i];
      std::string var = "layer_" + std::to_string(i);

      out << "  // Layer " << i << ": " << layer.view_class_name << "\n";
      out << "  if (frame.size() < offset + " << layer.wire_size_bytes
          << ") return xb::wire::parse_result::truncated;\n";
      out << "  " << layer.view_class_name << " " << var
          << "(frame.subspan(offset, " << layer.wire_size_bytes << "));\n";

      // Match condition
      if (!layer.match_field.empty()) {
        out << "  if (" << var << "." << layer.match_field
            << "() != static_cast<decltype(" << var << "." << layer.match_field
            << "())>(" << layer.match_value
            << ")) return xb::wire::parse_result::mismatch;\n";
      }

      out << "  offset += " << layer.wire_size_bytes << ";\n";

      // Message iteration (only on the last layer with repeat)
      if (!layer.count_field.empty()) {
        out << "\n  for (decltype(" << var << "." << layer.count_field
            << "()) i = 0; i < " << var << "." << layer.count_field
            << "(); ++i) {\n";

        if (layer.block_wire_size > 0) {
          out << "    if (frame.size() < offset + " << layer.block_wire_size
              << ") return xb::wire::parse_result::truncated;\n";
          out << "    " << layer.block_view_class << " blk"
              << "(frame.subspan(offset, " << layer.block_wire_size << "));\n";
          out << "    auto msg_len = blk." << layer.block_length_field
              << "();\n";
          out << "    offset += " << layer.block_wire_size << ";\n";
        } else {
          out << "    // No block header — messages are contiguous\n";
        }

        out << "    if (frame.size() < offset + msg_len) "
               "return xb::wire::parse_result::truncated;\n";
        out << "    cb(frame.subspan(offset, msg_len));\n";
        out << "    offset += msg_len;\n";
        out << "  }\n";
      }

      out << "\n";
    }

    out << "  return xb::wire::parse_result::ok;\n";
    out << "}\n";

    return out.str();
  }

} // namespace xb::wire

#endif // XB_WIRE_BINARY_CODEGEN_HPP
