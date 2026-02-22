#pragma once

#include <xb/attribute_decl.hpp>
#include <xb/qname.hpp>
#include <xb/wildcard.hpp>

#include <optional>
#include <vector>

namespace xb {

  class attribute_group_def {
    qname name_;
    std::vector<attribute_use> attributes_;
    std::vector<attribute_group_ref> attribute_group_refs_;
    std::optional<wildcard> attribute_wildcard_;

  public:
    attribute_group_def() = default;

    attribute_group_def(
        qname name, std::vector<attribute_use> attributes = {},
        std::vector<attribute_group_ref> attribute_group_refs = {},
        std::optional<wildcard> attribute_wildcard = std::nullopt)
        : name_(std::move(name)), attributes_(std::move(attributes)),
          attribute_group_refs_(std::move(attribute_group_refs)),
          attribute_wildcard_(std::move(attribute_wildcard)) {}

    const qname&
    name() const {
      return name_;
    }

    const std::vector<attribute_use>&
    attributes() const {
      return attributes_;
    }

    const std::vector<attribute_group_ref>&
    attribute_group_refs() const {
      return attribute_group_refs_;
    }

    const std::optional<wildcard>&
    attribute_wildcard() const {
      return attribute_wildcard_;
    }

    bool
    operator==(const attribute_group_def&) const = default;
  };

} // namespace xb
