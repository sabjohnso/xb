#include <xb/type_map.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace xb {

  type_map
  type_map::defaults() {
    type_map map;

    // String types
    map.set("string", {"std::string", "<string>"});
    map.set("normalizedString", {"std::string", "<string>"});
    map.set("token", {"std::string", "<string>"});
    map.set("anyURI", {"std::string", "<string>"});
    map.set("ID", {"std::string", "<string>"});
    map.set("IDREF", {"std::string", "<string>"});
    map.set("NMTOKEN", {"std::string", "<string>"});
    map.set("language", {"std::string", "<string>"});

    // Built-in types (no header needed)
    map.set("boolean", {"bool", ""});
    map.set("float", {"float", ""});
    map.set("double", {"double", ""});

    // Arbitrary-precision types
    map.set("decimal", {"xb::decimal", "<xb/decimal.hpp>"});
    map.set("integer", {"xb::integer", "<xb/integer.hpp>"});
    map.set("nonPositiveInteger", {"xb::integer", "<xb/integer.hpp>"});
    map.set("negativeInteger", {"xb::integer", "<xb/integer.hpp>"});
    map.set("nonNegativeInteger", {"xb::integer", "<xb/integer.hpp>"});
    map.set("positiveInteger", {"xb::integer", "<xb/integer.hpp>"});

    // Bounded integer types
    map.set("long", {"int64_t", "<cstdint>"});
    map.set("int", {"int32_t", "<cstdint>"});
    map.set("short", {"int16_t", "<cstdint>"});
    map.set("byte", {"int8_t", "<cstdint>"});
    map.set("unsignedLong", {"uint64_t", "<cstdint>"});
    map.set("unsignedInt", {"uint32_t", "<cstdint>"});
    map.set("unsignedShort", {"uint16_t", "<cstdint>"});
    map.set("unsignedByte", {"uint8_t", "<cstdint>"});

    // Date/time types
    map.set("dateTime", {"xb::date_time", "<xb/date_time.hpp>"});
    map.set("date", {"xb::date", "<xb/date.hpp>"});
    map.set("time", {"xb::time", "<xb/time.hpp>"});
    map.set("duration", {"xb::duration", "<xb/duration.hpp>"});

    // Binary types
    map.set("hexBinary", {"std::vector<std::byte>", "<vector> <cstddef>"});
    map.set("base64Binary", {"std::vector<std::byte>", "<vector> <cstddef>"});

    // QName
    map.set("QName", {"xb::qname", "<xb/qname.hpp>"});

    return map;
  }

  namespace {

    const std::string typemap_ns = "http://xb.dev/typemap";

    const std::set<std::string> known_xsd_types = {
        "string",
        "normalizedString",
        "token",
        "boolean",
        "float",
        "double",
        "decimal",
        "integer",
        "nonPositiveInteger",
        "negativeInteger",
        "nonNegativeInteger",
        "positiveInteger",
        "long",
        "int",
        "short",
        "byte",
        "unsignedLong",
        "unsignedInt",
        "unsignedShort",
        "unsignedByte",
        "dateTime",
        "date",
        "time",
        "duration",
        "hexBinary",
        "base64Binary",
        "anyURI",
        "QName",
        "ID",
        "IDREF",
        "NMTOKEN",
        "language",
    };

    bool
    is_whitespace_only(std::string_view sv) {
      return !sv.empty() && std::all_of(sv.begin(), sv.end(), [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
      });
    }

    bool
    read_skip_ws(xml_reader& reader) {
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::characters &&
            is_whitespace_only(reader.text()))
          continue;
        return true;
      }
      return false;
    }

  } // namespace

  type_map
  type_map::load(xml_reader& reader) {
    if (!read_skip_ws(reader) ||
        reader.node_type() != xml_node_type::start_element ||
        reader.name() != qname(typemap_ns, "typemap")) {
      throw std::runtime_error(
          "type_map::load: expected <typemap> root element "
          "in namespace http://xb.dev/typemap");
    }

    type_map result;

    while (read_skip_ws(reader)) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.name() == qname(typemap_ns, "typemap")) {
        break;
      }

      if (reader.node_type() != xml_node_type::start_element ||
          reader.name() != qname(typemap_ns, "mapping")) {
        throw std::runtime_error(
            "type_map::load: unexpected element inside <typemap>");
      }

      auto xsd_type =
          std::string(reader.attribute_value(qname("", "xsd-type")));
      auto cpp_type =
          std::string(reader.attribute_value(qname("", "cpp-type")));
      auto cpp_header =
          std::string(reader.attribute_value(qname("", "cpp-header")));

      if (known_xsd_types.find(xsd_type) == known_xsd_types.end()) {
        throw std::runtime_error("type_map::load: unknown xsd-type '" +
                                 xsd_type + "'");
      }

      result.set(std::move(xsd_type),
                 {std::move(cpp_type), std::move(cpp_header)});

      // Advance past end_element for this mapping
      read_skip_ws(reader);
    }

    return result;
  }

  void
  type_map::merge(const type_map& overrides) {
    for (const auto& [xsd_type, mapping] : overrides.entries_) {
      if (entries_.find(xsd_type) == entries_.end()) {
        throw std::runtime_error(
            "type_map::merge: cannot override unknown xsd-type '" + xsd_type +
            "'");
      }
      entries_[xsd_type] = mapping;
    }
  }

  const type_mapping*
  type_map::find(const std::string& xsd_type) const {
    auto it = entries_.find(xsd_type);
    if (it == entries_.end()) return nullptr;
    return &it->second;
  }

  void
  type_map::set(std::string xsd_type, type_mapping mapping) {
    entries_.insert_or_assign(std::move(xsd_type), std::move(mapping));
  }

  std::size_t
  type_map::size() const {
    return entries_.size();
  }

  bool
  type_map::contains(const std::string& xsd_type) const {
    return entries_.count(xsd_type) != 0;
  }

} // namespace xb
