// Stub specializations for xb::parse<> that the generated read functions
// call but that don't have real implementations yet.  These are needed to
// link the XSD translation test.  The stubs return default-constructed values.
//
// TODO: Implement proper parse specializations for qname and list types
// in the xb library.

#include <xb/qname.hpp>
#include <xb/xml_value.hpp>

#include <string_view>
#include <vector>

#include "xml_schema.hpp"

namespace xb {

  template <>
  qname
  parse<qname>(std::string_view text) {
    // Split prefix:local — we don't have namespace context here,
    // so store the prefix as-is in the namespace URI slot.
    auto colon = text.find(':');
    if (colon != std::string_view::npos)
      return qname(std::string(text.substr(0, colon)),
                   std::string(text.substr(colon + 1)));
    return qname("", std::string(text));
  }

  template <>
  std::vector<xb::qname>
  parse<std::vector<xb::qname>>(std::string_view) {
    return {};
  }

  template <>
  std::vector<xs::reduced_derivation_control>
  parse<std::vector<xs::reduced_derivation_control>>(std::string_view) {
    return {};
  }

  template <>
  std::vector<xs::type_derivation_control>
  parse<std::vector<xs::type_derivation_control>>(std::string_view) {
    return {};
  }

  template <>
  std::vector<xs::block_set_member_1_item>
  parse<std::vector<xs::block_set_member_1_item>>(std::string_view) {
    return {};
  }

  template <>
  std::vector<xs::simple_derivation_set_member_1_item>
  parse<std::vector<xs::simple_derivation_set_member_1_item>>(
      std::string_view) {
    return {};
  }

  template <>
  std::vector<std::variant<std::string, xs::basic_namespace_list_item_member_0>>
  parse<std::vector<
      std::variant<std::string, xs::basic_namespace_list_item_member_0>>>(
      std::string_view) {
    return {};
  }

  template <>
  std::vector<std::variant<xb::qname, xs::qname_list_item_member_0>>
  parse<std::vector<std::variant<xb::qname, xs::qname_list_item_member_0>>>(
      std::string_view) {
    return {};
  }

  template <>
  std::vector<std::variant<xb::qname, xs::qname_list_a_item_member_0>>
  parse<std::vector<std::variant<xb::qname, xs::qname_list_a_item_member_0>>>(
      std::string_view) {
    return {};
  }

} // namespace xb
