#pragma once

#include <xb/qname.hpp>
#include <xb/type_alternative.hpp>

#include <optional>
#include <string>
#include <vector>

namespace xb {

  class element_decl {
    qname name_;
    qname type_name_;
    bool nillable_ = false;
    bool abstract_ = false;
    std::optional<std::string> default_value_;
    std::optional<std::string> fixed_value_;
    std::optional<qname> substitution_group_;
    std::vector<type_alternative> type_alternatives_;

  public:
    element_decl() = default;

    element_decl(qname name, qname type_name, bool nillable = false,
                 bool abstract = false,
                 std::optional<std::string> default_value = std::nullopt,
                 std::optional<std::string> fixed_value = std::nullopt,
                 std::optional<qname> substitution_group = std::nullopt,
                 std::vector<type_alternative> type_alternatives = {})
        : name_(std::move(name)), type_name_(std::move(type_name)),
          nillable_(nillable), abstract_(abstract),
          default_value_(std::move(default_value)),
          fixed_value_(std::move(fixed_value)),
          substitution_group_(std::move(substitution_group)),
          type_alternatives_(std::move(type_alternatives)) {}

    const qname&
    name() const {
      return name_;
    }

    const qname&
    type_name() const {
      return type_name_;
    }

    bool
    nillable() const {
      return nillable_;
    }

    bool
    abstract() const {
      return abstract_;
    }

    const std::optional<std::string>&
    default_value() const {
      return default_value_;
    }

    const std::optional<std::string>&
    fixed_value() const {
      return fixed_value_;
    }

    const std::optional<qname>&
    substitution_group() const {
      return substitution_group_;
    }

    const std::vector<type_alternative>&
    type_alternatives() const {
      return type_alternatives_;
    }

    bool
    operator==(const element_decl&) const = default;
  };

  struct element_ref {
    qname ref;

    bool
    operator==(const element_ref&) const = default;
  };

} // namespace xb
