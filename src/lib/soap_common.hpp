#pragma once

// Internal header shared between soap_envelope.cpp and soap_fault.cpp.
// Not installed; not part of the public API.

#include <xb/soap_model.hpp>
#include <xb/xml_reader.hpp>

#include <string>

namespace xb::soap::detail {

  inline const std::string soap_1_1_ns =
      "http://schemas.xmlsoap.org/soap/envelope/";
  inline const std::string soap_1_2_ns =
      "http://www.w3.org/2003/05/soap-envelope";

  inline const std::string soap_prefix = "soap";

  inline const std::string&
  namespace_for(soap_version v) {
    return v == soap_version::v1_1 ? soap_1_1_ns : soap_1_2_ns;
  }

  inline bool
  is_soap_element(const qname& name, const std::string& soap_ns,
                  const std::string& local) {
    return name.namespace_uri() == soap_ns && name.local_name() == local;
  }

  inline bool
  read_skip_ws(xml_reader& reader) {
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::characters) {
        auto text = reader.text();
        bool all_ws = true;
        for (char c : text) {
          if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            all_ws = false;
            break;
          }
        }
        if (all_ws) continue;
      }
      return true;
    }
    return false;
  }

  inline std::string
  read_text_content(xml_reader& reader) {
    std::string result;
    auto start_depth = reader.depth();
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == start_depth) {
        return result;
      }
      if (reader.node_type() == xml_node_type::characters) {
        result += reader.text();
      }
    }
    return result;
  }

  inline std::string
  opt_attr(xml_reader& reader, const qname& name) {
    for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
      if (reader.attribute_name(i) == name) {
        return std::string(reader.attribute_value(i));
      }
    }
    return "";
  }

} // namespace xb::soap::detail
