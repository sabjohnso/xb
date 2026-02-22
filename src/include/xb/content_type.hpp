#pragma once

#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/qname.hpp>
#include <xb/schema_fwd.hpp>

#include <optional>
#include <variant>

namespace xb {

  struct simple_content {
    qname base_type_name;
    derivation_method derivation = derivation_method::restriction;
    facet_set facets;

    bool
    operator==(const simple_content&) const = default;
  };

  struct complex_content {
    qname base_type_name;
    derivation_method derivation = derivation_method::restriction;
    std::optional<model_group> content_model;

    complex_content() = default;

    complex_content(qname base, derivation_method d,
                    std::optional<model_group> cm = std::nullopt)
        : base_type_name(std::move(base)), derivation(d),
          content_model(std::move(cm)) {}

    complex_content(const complex_content&) = delete;
    complex_content&
    operator=(const complex_content&) = delete;

    complex_content(complex_content&&) = default;
    complex_content&
    operator=(complex_content&&) = default;

    bool
    operator==(const complex_content& other) const {
      return base_type_name == other.base_type_name &&
             derivation == other.derivation &&
             content_model == other.content_model;
    }
  };

  struct content_type {
    content_kind kind = content_kind::empty;
    std::variant<std::monostate, simple_content, complex_content> detail;

    content_type() = default;

    content_type(
        content_kind k,
        std::variant<std::monostate, simple_content, complex_content> d)
        : kind(k), detail(std::move(d)) {}

    content_type(const content_type&) = delete;
    content_type&
    operator=(const content_type&) = delete;

    content_type(content_type&&) = default;
    content_type&
    operator=(content_type&&) = default;

    bool
    operator==(const content_type& other) const {
      return kind == other.kind && detail == other.detail;
    }
  };

} // namespace xb
