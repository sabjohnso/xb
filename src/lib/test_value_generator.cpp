#include <xb/test_value_generator.hpp>

#include <xb/facet_set.hpp>

#include <string>

namespace xb {

  namespace {

    const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema";

    struct numeric_range {
      bool is_signed = true;
      bool bounded = false;
      std::string min_value;
      std::string max_value;
    };

    // XSD spec-defined ranges for bounded integer types.
    numeric_range
    builtin_integer_range(const std::string& local_name) {
      if (local_name == "byte") return {true, true, "-128", "127"};
      if (local_name == "short") return {true, true, "-32768", "32767"};
      if (local_name == "int") return {true, true, "-2147483648", "2147483647"};
      if (local_name == "long")
        return {true, true, "-9223372036854775808", "9223372036854775807"};
      if (local_name == "unsignedByte") return {false, true, "0", "255"};
      if (local_name == "unsignedShort") return {false, true, "0", "65535"};
      if (local_name == "unsignedInt") return {false, true, "0", "4294967295"};
      if (local_name == "unsignedLong")
        return {false, true, "0", "18446744073709551615"};
      return {};
    }

    bool
    is_integer_type(const std::string& local_name) {
      return local_name == "byte" || local_name == "short" ||
             local_name == "int" || local_name == "long" ||
             local_name == "unsignedByte" || local_name == "unsignedShort" ||
             local_name == "unsignedInt" || local_name == "unsignedLong" ||
             local_name == "integer" || local_name == "positiveInteger" ||
             local_name == "nonNegativeInteger" ||
             local_name == "negativeInteger" ||
             local_name == "nonPositiveInteger";
    }

    bool
    is_float_type(const std::string& local_name) {
      return local_name == "float" || local_name == "double" ||
             local_name == "decimal";
    }

    bool
    is_string_type(const std::string& local_name) {
      return local_name == "string" || local_name == "normalizedString" ||
             local_name == "token" || local_name == "Name" ||
             local_name == "NCName" || local_name == "NMTOKEN" ||
             local_name == "language" || local_name == "anyURI" ||
             local_name == "ID" || local_name == "IDREF";
    }

    // Increment a non-negative decimal integer string by 1.
    std::string
    increment(const std::string& s) {
      std::string result = s;
      int carry = 1;
      for (auto it = result.rbegin(); it != result.rend() && carry; ++it) {
        if (*it < '0' || *it > '9') break;
        int digit = (*it - '0') + carry;
        *it = static_cast<char>('0' + digit % 10);
        carry = digit / 10;
      }
      if (carry) result.insert(result.begin(), '1');
      return result;
    }

    // Decrement a positive decimal integer string by 1.
    std::string
    decrement(const std::string& s) {
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
      // Strip leading zeros (but keep at least one digit)
      auto first_nonzero = result.find_first_not_of('0');
      if (first_nonzero == std::string::npos) return "0";
      if (first_nonzero > 0) result.erase(0, first_nonzero);
      return result;
    }

    // Compare two integer strings numerically: return true if a < b.
    bool
    numeric_less_than(const std::string& a, const std::string& b) {
      bool a_neg = !a.empty() && a[0] == '-';
      bool b_neg = !b.empty() && b[0] == '-';
      if (a_neg != b_neg) return a_neg; // negative < positive

      std::string_view a_mag = a_neg ? std::string_view(a).substr(1) : a;
      std::string_view b_mag = b_neg ? std::string_view(b).substr(1) : b;

      // Compare magnitudes
      auto cmp = [](std::string_view x, std::string_view y) {
        if (x.size() != y.size()) return x.size() < y.size();
        return x < y;
      };

      return a_neg ? cmp(b_mag, a_mag) : cmp(a_mag, b_mag);
    }

    // Add +1 or -1 to a possibly-negative integer string.
    std::string
    add_one(const std::string& s) {
      if (s.empty()) return "1";
      if (s[0] == '-') {
        // Negative: adding 1 means decrement the magnitude
        std::string mag = s.substr(1);
        std::string result = decrement(mag);
        if (result == "0") return "0";
        result.insert(result.begin(), '-');
        return result;
      }
      return increment(s);
    }

    std::string
    sub_one(const std::string& s) {
      if (s.empty()) return "-1";
      if (s == "0") return "-1";
      if (s[0] == '-') {
        // Negative: subtracting 1 means increment the magnitude
        std::string mag = s.substr(1);
        std::string result = increment(mag);
        result.insert(result.begin(), '-');
        return result;
      }
      return decrement(s);
    }

    void
    add_if_unique(std::vector<test_value>& values, std::string xml_text,
                  std::string reason) {
      for (const auto& v : values) {
        if (v.xml_text == xml_text) return;
      }
      values.push_back({std::move(xml_text), std::move(reason)});
    }

    void
    generate_bounded_integer(std::vector<test_value>& values,
                             const std::string& min_val,
                             const std::string& max_val, bool is_signed) {
      add_if_unique(values, min_val, "boundary-min");
      add_if_unique(values, add_one(min_val), "boundary-min+1");
      if (is_signed) {
        add_if_unique(values, "0", "zero");
        add_if_unique(values, "1", "one");
      } else {
        add_if_unique(values, "0", "zero");
        add_if_unique(values, "1", "one");
      }
      add_if_unique(values, sub_one(max_val), "boundary-max-1");
      add_if_unique(values, max_val, "boundary-max");
    }

    void
    generate_unbounded_integer(std::vector<test_value>& values,
                               const std::string& local_name) {
      if (local_name == "integer") {
        add_if_unique(values, "0", "zero");
        add_if_unique(values, "1", "one");
        add_if_unique(values, "-1", "minus-one");
        add_if_unique(values, "999999999", "large-positive");
        add_if_unique(values, "-999999999", "large-negative");
      } else if (local_name == "positiveInteger") {
        add_if_unique(values, "1", "boundary-min");
        add_if_unique(values, "2", "boundary-min+1");
        add_if_unique(values, "999999999", "large-positive");
      } else if (local_name == "nonNegativeInteger") {
        add_if_unique(values, "0", "boundary-min");
        add_if_unique(values, "1", "boundary-min+1");
        add_if_unique(values, "999999999", "large-positive");
      } else if (local_name == "negativeInteger") {
        add_if_unique(values, "-1", "boundary-max");
        add_if_unique(values, "-2", "boundary-max-1");
        add_if_unique(values, "-999999999", "large-negative");
      } else if (local_name == "nonPositiveInteger") {
        add_if_unique(values, "0", "boundary-max");
        add_if_unique(values, "-1", "boundary-max-1");
        add_if_unique(values, "-999999999", "large-negative");
      }
    }

    test_value_set
    generate_for_builtin(const qname& type_name) {
      test_value_set result;
      result.type_name = type_name;

      const auto& local = type_name.local_name();

      auto range = builtin_integer_range(local);
      if (range.bounded) {
        generate_bounded_integer(result.values, range.min_value,
                                 range.max_value, range.is_signed);
        return result;
      }

      if (is_integer_type(local)) {
        generate_unbounded_integer(result.values, local);
        return result;
      }

      if (local == "boolean") {
        result.values.push_back({"true", "boolean-true"});
        result.values.push_back({"false", "boolean-false"});
        return result;
      }

      if (is_float_type(local)) {
        add_if_unique(result.values, "0", "zero");
        add_if_unique(result.values, "1", "one");
        add_if_unique(result.values, "-1", "minus-one");
        add_if_unique(result.values, "0.5", "fractional");
        return result;
      }

      if (is_string_type(local)) {
        add_if_unique(result.values, "", "empty");
        add_if_unique(result.values, "test", "typical");
        return result;
      }

      // Date/time and other types: single representative value
      if (local == "dateTime") {
        add_if_unique(result.values, "2024-01-01T00:00:00Z", "epoch-ish");
        add_if_unique(result.values, "2024-02-29T12:30:00+05:30",
                      "leap-day-tz");
        return result;
      }
      if (local == "date") {
        add_if_unique(result.values, "2024-01-01", "epoch-ish");
        add_if_unique(result.values, "2024-02-29", "leap-day");
        return result;
      }
      if (local == "time") {
        add_if_unique(result.values, "00:00:00Z", "midnight");
        add_if_unique(result.values, "23:59:59Z", "end-of-day");
        return result;
      }
      if (local == "duration") {
        add_if_unique(result.values, "P0D", "zero");
        add_if_unique(result.values, "P1Y2M3DT4H5M6S", "typical");
        return result;
      }
      if (local == "hexBinary") {
        add_if_unique(result.values, "", "empty");
        add_if_unique(result.values, "DEADBEEF", "typical");
        return result;
      }
      if (local == "base64Binary") {
        add_if_unique(result.values, "", "empty");
        add_if_unique(result.values, "dGVzdA==", "typical");
        return result;
      }

      // Fallback
      add_if_unique(result.values, "", "empty");
      return result;
    }

    // Resolve a user-defined type to its base built-in type, collecting
    // facets along the chain.
    struct resolved_facets {
      qname builtin_type;
      facet_set merged;
    };

    resolved_facets
    resolve_type_chain(const schema_set& schemas, const simple_type& st) {
      resolved_facets result;
      result.merged = st.facets();

      qname current = st.base_type_name();
      while (current.namespace_uri() != xsd_ns) {
        const auto* base = schemas.find_simple_type(current);
        if (!base) break;

        // Merge: child facets override parent, but parent provides defaults
        const auto& parent = base->facets();
        if (!result.merged.min_inclusive && parent.min_inclusive)
          result.merged.min_inclusive = parent.min_inclusive;
        if (!result.merged.max_inclusive && parent.max_inclusive)
          result.merged.max_inclusive = parent.max_inclusive;
        if (!result.merged.min_exclusive && parent.min_exclusive)
          result.merged.min_exclusive = parent.min_exclusive;
        if (!result.merged.max_exclusive && parent.max_exclusive)
          result.merged.max_exclusive = parent.max_exclusive;
        if (!result.merged.length && parent.length)
          result.merged.length = parent.length;
        if (!result.merged.min_length && parent.min_length)
          result.merged.min_length = parent.min_length;
        if (!result.merged.max_length && parent.max_length)
          result.merged.max_length = parent.max_length;
        if (!result.merged.total_digits && parent.total_digits)
          result.merged.total_digits = parent.total_digits;
        if (!result.merged.fraction_digits && parent.fraction_digits)
          result.merged.fraction_digits = parent.fraction_digits;
        if (result.merged.enumeration.empty() && !parent.enumeration.empty())
          result.merged.enumeration = parent.enumeration;

        current = base->base_type_name();
      }

      result.builtin_type = current;
      return result;
    }

    void
    generate_faceted_numeric(std::vector<test_value>& values,
                             const facet_set& facets,
                             const qname& builtin_type) {
      const auto& local = builtin_type.local_name();
      auto range = builtin_integer_range(local);

      // Determine effective min
      std::string effective_min;
      bool has_min = false;

      if (facets.min_inclusive) {
        effective_min = *facets.min_inclusive;
        has_min = true;
        add_if_unique(values, effective_min, "boundary-min");
        add_if_unique(values, add_one(effective_min), "boundary-min+1");
      } else if (facets.min_exclusive) {
        effective_min = add_one(*facets.min_exclusive);
        has_min = true;
        add_if_unique(values, effective_min, "boundary-min");
        add_if_unique(values, add_one(effective_min), "boundary-min+1");
      } else if (range.bounded) {
        effective_min = range.min_value;
        has_min = true;
        add_if_unique(values, effective_min, "boundary-min");
        add_if_unique(values, add_one(effective_min), "boundary-min+1");
      }

      // Determine effective max
      std::string effective_max;
      bool has_max = false;

      if (facets.max_inclusive) {
        effective_max = *facets.max_inclusive;
        has_max = true;
        add_if_unique(values, sub_one(effective_max), "boundary-max-1");
        add_if_unique(values, effective_max, "boundary-max");
      } else if (facets.max_exclusive) {
        effective_max = sub_one(*facets.max_exclusive);
        has_max = true;
        add_if_unique(values, sub_one(effective_max), "boundary-max-1");
        add_if_unique(values, effective_max, "boundary-max");
      } else if (range.bounded) {
        effective_max = range.max_value;
        has_max = true;
        add_if_unique(values, sub_one(effective_max), "boundary-max-1");
        add_if_unique(values, effective_max, "boundary-max");
      }

      // Skip remaining generation if the range is empty (malformed schema).
      if (has_min && has_max && numeric_less_than(effective_max, effective_min))
        return;

      // Zero if in range
      if (has_min && has_max) {
        // Simple check: if min starts with '-' or is "0", zero is valid
        bool zero_valid = (effective_min[0] == '-' || effective_min == "0") &&
                          (effective_max[0] != '-');
        if (zero_valid) add_if_unique(values, "0", "zero");
      } else {
        add_if_unique(values, "0", "zero");
      }

      // total_digits
      if (facets.total_digits) {
        std::string all_nines(*facets.total_digits, '9');
        add_if_unique(values, all_nines, "total_digits-max");
        add_if_unique(values, "-" + all_nines, "total_digits-max-neg");
      }
    }

    void
    generate_faceted_string(std::vector<test_value>& values,
                            const facet_set& facets) {
      if (facets.length) {
        std::string val(*facets.length, 'a');
        add_if_unique(values, val, "exact-length");
        return;
      }

      if (facets.min_length || facets.max_length) {
        if (facets.min_length) {
          add_if_unique(values, std::string(*facets.min_length, 'a'),
                        "min-length");
          add_if_unique(values, std::string(*facets.min_length + 1, 'a'),
                        "min-length+1");
        }
        if (facets.max_length) {
          if (*facets.max_length > 0)
            add_if_unique(values, std::string(*facets.max_length - 1, 'a'),
                          "max-length-1");
          add_if_unique(values, std::string(*facets.max_length, 'a'),
                        "max-length");
        }
        return;
      }

      // No length facets: defaults
      add_if_unique(values, "", "empty");
      add_if_unique(values, "test", "typical");
    }

  } // namespace

  test_value_generator::test_value_generator(const schema_set& schemas)
      : schemas_(schemas) {}

  test_value_set
  test_value_generator::generate(const qname& type_name) const {
    // Check if it's an XSD built-in type
    if (type_name.namespace_uri() == xsd_ns)
      return generate_for_builtin(type_name);

    // Look up user-defined simple type
    const auto* st = schemas_.find_simple_type(type_name);
    if (!st) {
      test_value_set result;
      result.type_name = type_name;
      return result;
    }

    return generate(*st);
  }

  test_value_set
  test_value_generator::generate(const simple_type& st) const {
    test_value_set result;
    result.type_name = st.name();

    // Enumeration takes priority: produce all values
    if (!st.facets().enumeration.empty()) {
      std::size_t i = 0;
      for (const auto& val : st.facets().enumeration) {
        ++i;
        result.values.push_back(
            {val, "enum-" + std::to_string(i) + "-of-" +
                      std::to_string(st.facets().enumeration.size())});
      }
      return result;
    }

    // Resolve type chain to built-in
    auto resolved = resolve_type_chain(schemas_, st);
    const auto& builtin_local = resolved.builtin_type.local_name();
    const auto& facets = resolved.merged;

    // Check resolved facets for enumerations (from base types)
    if (!facets.enumeration.empty()) {
      std::size_t i = 0;
      for (const auto& val : facets.enumeration) {
        ++i;
        result.values.push_back(
            {val, "enum-" + std::to_string(i) + "-of-" +
                      std::to_string(facets.enumeration.size())});
      }
      return result;
    }

    // Numeric types with facets
    if (is_integer_type(builtin_local) || is_float_type(builtin_local)) {
      generate_faceted_numeric(result.values, facets, resolved.builtin_type);
      if (result.values.empty())
        return generate_for_builtin(resolved.builtin_type);
      return result;
    }

    // String types with facets
    if (is_string_type(builtin_local)) {
      generate_faceted_string(result.values, facets);
      return result;
    }

    if (builtin_local == "boolean") {
      result.values.push_back({"true", "boolean-true"});
      result.values.push_back({"false", "boolean-false"});
      return result;
    }

    // Fallback: delegate to built-in generator
    return generate_for_builtin(resolved.builtin_type);
  }

} // namespace xb
