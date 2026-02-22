#pragma once

#include <xb/qname.hpp>

#include <optional>
#include <string>

namespace xb {

  class attribute_decl {
    qname name_;
    qname type_name_;
    std::optional<std::string> default_value_;
    std::optional<std::string> fixed_value_;

  public:
    attribute_decl() = default;

    attribute_decl(qname name, qname type_name,
                   std::optional<std::string> default_value = std::nullopt,
                   std::optional<std::string> fixed_value = std::nullopt)
        : name_(std::move(name)), type_name_(std::move(type_name)),
          default_value_(std::move(default_value)),
          fixed_value_(std::move(fixed_value)) {}

    const qname&
    name() const {
      return name_;
    }

    const qname&
    type_name() const {
      return type_name_;
    }

    const std::optional<std::string>&
    default_value() const {
      return default_value_;
    }

    const std::optional<std::string>&
    fixed_value() const {
      return fixed_value_;
    }

    bool
    operator==(const attribute_decl&) const = default;
  };

  struct attribute_use {
    qname name;
    qname type_name;
    bool required = false;
    std::optional<std::string> default_value;
    std::optional<std::string> fixed_value;

    bool
    operator==(const attribute_use&) const = default;
  };

  struct attribute_group_ref {
    qname ref;

    bool
    operator==(const attribute_group_ref&) const = default;
  };

} // namespace xb
