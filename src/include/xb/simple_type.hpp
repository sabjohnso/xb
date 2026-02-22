#pragma once

#include <xb/facet_set.hpp>
#include <xb/qname.hpp>
#include <xb/schema_fwd.hpp>

#include <optional>
#include <vector>

namespace xb {

  class simple_type {
    qname name_;
    simple_type_variety variety_ = simple_type_variety::atomic;
    qname base_type_name_;
    facet_set facets_;
    std::optional<qname> item_type_name_;
    std::vector<qname> member_type_names_;

  public:
    simple_type() = default;

    simple_type(qname name, simple_type_variety variety, qname base_type_name,
                facet_set facets = {},
                std::optional<qname> item_type_name = std::nullopt,
                std::vector<qname> member_type_names = {})
        : name_(std::move(name)), variety_(variety),
          base_type_name_(std::move(base_type_name)),
          facets_(std::move(facets)),
          item_type_name_(std::move(item_type_name)),
          member_type_names_(std::move(member_type_names)) {}

    const qname&
    name() const {
      return name_;
    }

    simple_type_variety
    variety() const {
      return variety_;
    }

    const qname&
    base_type_name() const {
      return base_type_name_;
    }

    const facet_set&
    facets() const {
      return facets_;
    }

    const std::optional<qname>&
    item_type_name() const {
      return item_type_name_;
    }

    const std::vector<qname>&
    member_type_names() const {
      return member_type_names_;
    }

    bool
    operator==(const simple_type&) const = default;
  };

} // namespace xb
