#pragma once

#include <compare>
#include <functional>
#include <ostream>
#include <string>

namespace xb {

struct qname {
  std::string namespace_uri;
  std::string local_name;

  auto operator<=>(const qname&) const = default;
  bool operator==(const qname&) const = default;

  friend std::ostream& operator<<(std::ostream& os, const qname& q) {
    if (q.namespace_uri.empty()) {
      return os << q.local_name;
    }
    return os << '{' << q.namespace_uri << '}' << q.local_name;
  }
};

} // namespace xb

template <>
struct std::hash<xb::qname> {
  std::size_t operator()(const xb::qname& q) const noexcept {
    std::size_t h1 = std::hash<std::string>{}(q.namespace_uri);
    std::size_t h2 = std::hash<std::string>{}(q.local_name);
    return h1 ^ (h2 << 1);
  }
};
