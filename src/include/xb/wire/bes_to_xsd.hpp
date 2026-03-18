#ifndef XB_WIRE_BES_TO_XSD_HPP
#define XB_WIRE_BES_TO_XSD_HPP

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/qname.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/wire/encoding.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

namespace xb::wire {

  // -------------------------------------------------------------------------
  // Type inference result
  // -------------------------------------------------------------------------

  /**
   * @brief Result of inferring an XSD type from BES field properties.
   *
   * Contains the base XSD type name (e.g. "xs:unsignedShort"), a local
   * name for use in synthetic type construction, and any facets needed
   * to constrain the type for non-standard widths.
   */
  struct inferred_xsd_type {
    std::string local_name;
    std::string base_type;
    facet_set facets;
  };

  // -------------------------------------------------------------------------
  // Type inference
  // -------------------------------------------------------------------------

  namespace detail {

    /**
     * @brief Return the narrowest standard unsigned XSD type that can
     *        hold a value of the given bit width.
     */
    inline std::pair<std::string, unsigned>
    narrowest_unsigned(unsigned bits) {
      if (bits <= 8) return {"unsignedByte", 8};
      if (bits <= 16) return {"unsignedShort", 16};
      if (bits <= 32) return {"unsignedInt", 32};
      return {"unsignedLong", 64};
    }

    /**
     * @brief Return the narrowest standard signed XSD type that can
     *        hold a value of the given bit width.
     */
    inline std::pair<std::string, unsigned>
    narrowest_signed(unsigned bits) {
      if (bits <= 8) return {"byte", 8};
      if (bits <= 16) return {"short", 16};
      if (bits <= 32) return {"int", 32};
      return {"long", 64};
    }

    /**
     * @brief Compute 2^n as a string (for n <= 63).
     */
    inline std::string
    power_of_two_str(unsigned n) {
      if (n == 0) return "1";
      std::uint64_t val = std::uint64_t{1} << n;
      return std::to_string(val);
    }

    /**
     * @brief Compute 2^n - 1 as a string (for n <= 63).
     */
    inline std::string
    power_of_two_minus_one_str(unsigned n) {
      if (n == 0) return "0";
      if (n >= 64) return "18446744073709551615"; // 2^64 - 1
      std::uint64_t val = (std::uint64_t{1} << n) - 1;
      return std::to_string(val);
    }

    /**
     * @brief Compute -(2^n) as a string.
     */
    inline std::string
    neg_power_of_two_str(unsigned n) {
      if (n == 0) return "-1";
      std::int64_t val = -(static_cast<std::int64_t>(std::uint64_t{1} << n));
      return std::to_string(val);
    }

  } // namespace detail

  /**
   * @brief Infer the narrowest XSD type for a BES field based on its
   *        bit width and encoding.
   *
   * For standard widths (8, 16, 32, 64), the exact XSD type is returned
   * with no additional facets.  For non-standard widths, the next-larger
   * standard type is returned with minInclusive/maxInclusive facets
   * constraining the representable range.
   */
  inline inferred_xsd_type
  infer_xsd_type(unsigned bits,
                 std::optional<bes::primitive_encoding_type> encoding) {
    using enc = bes::primitive_encoding_type;

    // Default to unsigned if no encoding specified
    auto eff = encoding.value_or(enc::unsigned_);

    switch (eff) {
      case enc::unsigned_:
      case enc::enum_integer: {
        auto [local, std_bits] = detail::narrowest_unsigned(bits);
        inferred_xsd_type result;
        result.local_name = local;
        result.base_type = "xs:" + local;
        if (bits < std_bits) {
          result.facets.max_inclusive =
              detail::power_of_two_minus_one_str(bits);
        }
        return result;
      }

      case enc::twos_complement: {
        auto [local, std_bits] = detail::narrowest_signed(bits);
        inferred_xsd_type result;
        result.local_name = local;
        result.base_type = "xs:" + local;
        if (bits < std_bits) {
          result.facets.min_inclusive = detail::neg_power_of_two_str(bits - 1);
          result.facets.max_inclusive =
              detail::power_of_two_minus_one_str(bits - 1);
        }
        return result;
      }

      case enc::ascii:
      case enc::utf8: {
        inferred_xsd_type result;
        result.local_name = "string";
        result.base_type = "xs:string";
        result.facets.max_length = bits / 8;
        return result;
      }

      case enc::raw: {
        inferred_xsd_type result;
        result.local_name = "hexBinary";
        result.base_type = "xs:hexBinary";
        result.facets.length = bits / 8;
        return result;
      }

      case enc::ieee754: {
        inferred_xsd_type result;
        if (bits <= 32) {
          result.local_name = "float";
          result.base_type = "xs:float";
        } else {
          result.local_name = "double";
          result.base_type = "xs:double";
        }
        return result;
      }

      case enc::boolean: {
        return {"boolean", "xs:boolean", {}};
      }

      case enc::epoch: {
        auto [local, std_bits] = detail::narrowest_unsigned(bits);
        inferred_xsd_type result;
        result.local_name = local;
        result.base_type = "xs:" + local;
        if (bits < std_bits) {
          result.facets.max_inclusive =
              detail::power_of_two_minus_one_str(bits);
        }
        return result;
      }

      default: {
        // enum_char, utf16_be, utf16_le, etc. — treat as unsigned
        auto [local, std_bits] = detail::narrowest_unsigned(bits);
        inferred_xsd_type result;
        result.local_name = local;
        result.base_type = "xs:" + local;
        if (bits < std_bits) {
          result.facets.max_inclusive =
              detail::power_of_two_minus_one_str(bits);
        }
        return result;
      }
    }
  }

  // -------------------------------------------------------------------------
  // XSD generation from BES
  // -------------------------------------------------------------------------

  namespace detail {

    /**
     * @brief Concept for BES field_type: has name, bits, encoding, null_value.
     */
    template <typename T>
    concept bes_data_field = requires(const T& t) {
      { t.name } -> std::convertible_to<const std::string&>;
      { t.bits } -> std::convertible_to<unsigned>;
      { t.encoding };
      { t.offset };
    };

    /**
     * @brief Concept for BES wire_field_type: has name and bits but not offset.
     */
    template <typename T>
    concept bes_wire_field = requires(const T& t) {
      { t.name } -> std::convertible_to<const std::string&>;
      { t.bits } -> std::convertible_to<unsigned>;
    } && !bes_data_field<T>;

    /**
     * @brief Write an xs:element for a data field, with optional restriction.
     */
    inline void
    write_element(std::ostringstream& out, const bes::field_type& field,
                  const std::string& indent) {
      auto inf = infer_xsd_type(field.bits, field.encoding);
      bool has_facets = inf.facets.max_inclusive.has_value() ||
                        inf.facets.min_inclusive.has_value() ||
                        inf.facets.max_length.has_value() ||
                        inf.facets.length.has_value();
      bool optional = field.null_value.has_value();

      if (!has_facets) {
        // Simple element with type attribute
        out << indent << "<xs:element name=\"" << field.name << "\" type=\""
            << inf.base_type << "\"";
        if (optional) out << " minOccurs=\"0\"";
        out << "/>\n";
      } else {
        // Element with inline restriction
        out << indent << "<xs:element name=\"" << field.name << "\"";
        if (optional) out << " minOccurs=\"0\"";
        out << ">\n";
        out << indent << "  <xs:simpleType>\n";
        out << indent << "    <xs:restriction base=\"" << inf.base_type
            << "\">\n";
        if (inf.facets.min_inclusive.has_value()) {
          out << indent << "      <xs:minInclusive value=\""
              << *inf.facets.min_inclusive << "\"/>\n";
        }
        if (inf.facets.max_inclusive.has_value()) {
          out << indent << "      <xs:maxInclusive value=\""
              << *inf.facets.max_inclusive << "\"/>\n";
        }
        if (inf.facets.max_length.has_value()) {
          out << indent << "      <xs:maxLength value=\""
              << *inf.facets.max_length << "\"/>\n";
        }
        if (inf.facets.length.has_value()) {
          out << indent << "      <xs:length value=\"" << *inf.facets.length
              << "\"/>\n";
        }
        out << indent << "    </xs:restriction>\n";
        out << indent << "  </xs:simpleType>\n";
        out << indent << "</xs:element>\n";
      }
    }

  } // namespace detail

  /**
   * @brief Generate a standards-conformant XSD document from a BES encoding.
   *
   * Each message becomes a named xs:complexType with an xs:sequence of
   * elements.  Wire-only fields and constants are omitted.
   */
  template <typename Encoding>
  std::string
  generate_xsd(const Encoding& encoding) {
    std::ostringstream out;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"";
    if (encoding.target_namespace.has_value() &&
        !encoding.target_namespace->empty()) {
      out << "\n           targetNamespace=\"" << *encoding.target_namespace
          << "\"\n"
          << "           xmlns:tns=\"" << *encoding.target_namespace << "\"\n"
          << "           elementFormDefault=\"qualified\"";
    }
    out << ">\n\n";

    // Process each message
    for (const auto& choice_item : encoding.choice) {
      std::visit(
          [&](const auto& item) {
            if constexpr (requires {
                            item.name;
                            item.choice;
                            item.type;
                            item.discriminant_value;
                          }) {
              // This is a message_type
              out << "  <xs:complexType name=\"" << item.name << "\">\n";
              out << "    <xs:sequence>\n";

              for (const auto& field_item : item.choice) {
                std::visit(
                    [&](const auto& v) {
                      using V = std::decay_t<decltype(v)>;
                      if constexpr (detail::bes_data_field<V>) {
                        detail::write_element(out, v, "      ");
                      }
                      // wire_field_type, constant_type, etc. — skip
                    },
                    field_item);
              }

              out << "    </xs:sequence>\n";
              out << "  </xs:complexType>\n\n";
            }
          },
          choice_item);
    }

    out << "</xs:schema>\n";
    return out.str();
  }

  // -------------------------------------------------------------------------
  // Synthetic schema_set from BES
  // -------------------------------------------------------------------------

  /**
   * @brief Build a schema_set from a BES encoding, creating synthetic
   *        complex types for each message.
   *
   * This enables BES-only codegen: the synthetic schema_set provides
   * the type information that encoding_plan needs without requiring
   * a separate XSD file.
   */
  template <typename Encoding>
  schema_set
  build_synthetic_schemas(const Encoding& encoding) {
    std::string tns;
    if (encoding.target_namespace.has_value()) tns = *encoding.target_namespace;

    const std::string xs = "http://www.w3.org/2001/XMLSchema";

    schema s;
    s.set_target_namespace(tns);

    for (const auto& choice_item : encoding.choice) {
      std::visit(
          [&](const auto& item) {
            if constexpr (requires {
                            item.name;
                            item.choice;
                            item.type;
                            item.discriminant_value;
                          }) {
              // Build a model_group with elements for each data field
              model_group mg(compositor_kind::sequence);

              for (const auto& field_item : item.choice) {
                std::visit(
                    [&](const auto& v) {
                      using V = std::decay_t<decltype(v)>;
                      if constexpr (detail::bes_data_field<V>) {
                        auto inf = infer_xsd_type(v.bits, v.encoding);

                        // Strip "xs:" prefix to get the XSD namespace
                        // local name
                        std::string xsd_local = inf.base_type;
                        if (xsd_local.starts_with("xs:"))
                          xsd_local = xsd_local.substr(3);

                        bool optional = false;
                        if constexpr (requires { v.null_value; }) {
                          optional = v.null_value.has_value();
                        }

                        occurrence occ;
                        if (optional) occ = {0, 1};

                        element_decl elem(qname(tns, v.name),
                                          qname(xs, xsd_local));
                        mg.add_particle(
                            particle(std::move(elem), std::move(occ)));
                      }
                    },
                    field_item);
              }

              complex_content cc(qname(), derivation_method::restriction,
                                 std::move(mg));
              content_type ct(content_kind::element_only, std::move(cc));

              complex_type ctype(qname(tns, item.name), false, false,
                                 std::move(ct));
              s.add_complex_type(std::move(ctype));
            }
          },
          choice_item);
    }

    schema_set ss;
    ss.add(std::move(s));
    ss.resolve();
    return ss;
  }

} // namespace xb::wire

#endif // XB_WIRE_BES_TO_XSD_HPP
