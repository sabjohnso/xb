#pragma once

#include <xb/model_group.hpp>
#include <xb/qname.hpp>

namespace xb {

  class model_group_def {
    qname name_;
    model_group group_;

  public:
    model_group_def() = default;

    model_group_def(qname name, model_group group)
        : name_(std::move(name)), group_(std::move(group)) {}

    model_group_def(const model_group_def&) = delete;
    model_group_def&
    operator=(const model_group_def&) = delete;

    model_group_def(model_group_def&&) = default;
    model_group_def&
    operator=(model_group_def&&) = default;

    const qname&
    name() const {
      return name_;
    }

    const model_group&
    group() const {
      return group_;
    }

    bool
    operator==(const model_group_def& other) const {
      return name_ == other.name_ && group_ == other.group_;
    }
  };

} // namespace xb
