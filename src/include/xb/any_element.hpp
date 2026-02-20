#pragma once

#include <xb/any_attribute.hpp>

#include <compare>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace xb {

struct any_element {
  qname name;
  std::vector<any_attribute> attributes;
  std::vector<std::variant<std::string, any_element>> children;

  auto operator<=>(const any_element&) const = default;
  bool operator==(const any_element&) const = default;

  friend std::ostream& operator<<(std::ostream& os, const any_element& e) {
    os << '<' << e.name;
    for (const auto& attr : e.attributes) {
      os << ' ' << attr;
    }
    if (e.children.empty()) {
      return os << "/>";
    }
    os << '>';
    for (const auto& child : e.children) {
      std::visit(
          [&os](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
              os << v;
            } else {
              os << v;
            }
          },
          child);
    }
    return os << "</" << e.name << '>';
  }
};

} // namespace xb
