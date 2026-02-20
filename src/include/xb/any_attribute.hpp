#pragma once

#include <xb/qname.hpp>

#include <compare>
#include <functional>
#include <ostream>
#include <string>

namespace xb {

struct any_attribute {
  qname name;
  std::string value;

  auto operator<=>(const any_attribute&) const = default;
  bool operator==(const any_attribute&) const = default;

  friend std::ostream& operator<<(std::ostream& os, const any_attribute& a) {
    return os << a.name << '=' << '"' << a.value << '"';
  }
};

} // namespace xb

template <>
struct std::hash<xb::any_attribute> {
  std::size_t operator()(const xb::any_attribute& a) const noexcept {
    std::size_t h1 = std::hash<xb::qname>{}(a.name);
    std::size_t h2 = std::hash<std::string>{}(a.value);
    return h1 ^ (h2 << 1);
  }
};
