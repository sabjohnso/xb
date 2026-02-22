#pragma once

#include <xb/attribute_decl.hpp>
#include <xb/content_type.hpp>
#include <xb/qname.hpp>
#include <xb/wildcard.hpp>

#include <optional>
#include <vector>

namespace xb {

  class complex_type {
    qname name_;
    bool abstract_ = false;
    bool mixed_ = false;
    content_type content_;
    std::vector<attribute_use> attributes_;
    std::vector<attribute_group_ref> attribute_group_refs_;
    std::optional<wildcard> attribute_wildcard_;

  public:
    complex_type() = default;

    complex_type(qname name, bool abstract, bool mixed, content_type content,
                 std::vector<attribute_use> attributes = {},
                 std::vector<attribute_group_ref> attribute_group_refs = {},
                 std::optional<wildcard> attribute_wildcard = std::nullopt)
        : name_(std::move(name)), abstract_(abstract), mixed_(mixed),
          content_(std::move(content)), attributes_(std::move(attributes)),
          attribute_group_refs_(std::move(attribute_group_refs)),
          attribute_wildcard_(std::move(attribute_wildcard)) {}

    complex_type(const complex_type&) = delete;
    complex_type&
    operator=(const complex_type&) = delete;

    complex_type(complex_type&&) = default;
    complex_type&
    operator=(complex_type&&) = default;

    const qname&
    name() const {
      return name_;
    }

    bool
    abstract() const {
      return abstract_;
    }

    bool
    mixed() const {
      return mixed_;
    }

    const content_type&
    content() const {
      return content_;
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
    operator==(const complex_type& other) const {
      return name_ == other.name_ && abstract_ == other.abstract_ &&
             mixed_ == other.mixed_ && content_ == other.content_ &&
             attributes_ == other.attributes_ &&
             attribute_group_refs_ == other.attribute_group_refs_ &&
             attribute_wildcard_ == other.attribute_wildcard_;
    }
  };

} // namespace xb
