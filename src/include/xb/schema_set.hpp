#pragma once

#include <xb/schema.hpp>

#include <vector>

namespace xb {

  class schema_set {
    std::vector<schema> schemas_;
    bool resolved_ = false;

  public:
    schema_set() = default;

    void
    add(schema s);

    void
    resolve();

    const simple_type*
    find_simple_type(const qname& name) const;

    const complex_type*
    find_complex_type(const qname& name) const;

    const element_decl*
    find_element(const qname& name) const;

    const attribute_decl*
    find_attribute(const qname& name) const;

    const model_group_def*
    find_model_group_def(const qname& name) const;

    const attribute_group_def*
    find_attribute_group_def(const qname& name) const;

    const std::vector<schema>&
    schemas() const {
      return schemas_;
    }

    std::vector<schema>&
    schemas() {
      return schemas_;
    }

    std::vector<schema>
    take_schemas() {
      resolved_ = false;
      return std::move(schemas_);
    }
  };

} // namespace xb
