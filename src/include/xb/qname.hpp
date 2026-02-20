#pragma once

#include <compare>
#include <functional>
#include <ostream>
#include <string>

namespace xb {

  class qname {
    std::string namespace_uri_;
    std::string local_name_;

  public:
    qname() = default;

    qname(std::string namespace_uri, std::string local_name)
        : namespace_uri_(std::move(namespace_uri)),
          local_name_(std::move(local_name)) {}

    const std::string&
    namespace_uri() const {
      return namespace_uri_;
    }

    const std::string&
    local_name() const {
      return local_name_;
    }

    auto
    operator<=>(const qname&) const = default;

    bool
    operator==(const qname&) const = default;

    friend std::ostream&
    operator<<(std::ostream& os, const qname& q) {
      if (q.namespace_uri_.empty()) { return os << q.local_name_; }
      return os << '{' << q.namespace_uri_ << '}' << q.local_name_;
    }
  };

} // namespace xb

template <>
struct std::hash<xb::qname> {
  std::size_t
  operator()(const xb::qname& q) const noexcept {
    std::size_t h1 = std::hash<std::string>{}(q.namespace_uri());
    std::size_t h2 = std::hash<std::string>{}(q.local_name());
    return h1 ^ (h2 << 1);
  }
};
