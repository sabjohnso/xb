#pragma once

#include <xb/qname.hpp>
#include <xb/xml_escape.hpp>

#include <compare>
#include <functional>
#include <ostream>
#include <string>

namespace xb {

  class any_attribute {
    qname name_;
    std::string value_;

  public:
    any_attribute() = default;

    any_attribute(qname name, std::string value)
        : name_(std::move(name)), value_(std::move(value)) {}

    const qname&
    name() const {
      return name_;
    }

    const std::string&
    value() const {
      return value_;
    }

    auto
    operator<=>(const any_attribute&) const = default;
    bool
    operator==(const any_attribute&) const = default;

    friend std::ostream&
    operator<<(std::ostream& os, const any_attribute& a) {
      os << a.name_ << "=\"";
      escape_attribute(os, a.value_);
      return os << '"';
    }
  };

} // namespace xb

template <>
struct std::hash<xb::any_attribute> {
  std::size_t
  operator()(const xb::any_attribute& a) const noexcept {
    std::size_t h1 = std::hash<xb::qname>{}(a.name());
    std::size_t h2 = std::hash<std::string>{}(a.value());
    return h1 ^ (h2 << 1);
  }
};
