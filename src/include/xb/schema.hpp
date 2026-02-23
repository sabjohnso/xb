#pragma once

#include <xb/attribute_group_def.hpp>
#include <xb/complex_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group_def.hpp>
#include <xb/open_content.hpp>
#include <xb/qname.hpp>
#include <xb/simple_type.hpp>

#include <optional>
#include <string>
#include <vector>

namespace xb {

  struct schema_import {
    std::string namespace_uri;
    std::string schema_location;

    bool
    operator==(const schema_import&) const = default;
  };

  struct schema_include {
    std::string schema_location;

    bool
    operator==(const schema_include&) const = default;
  };

  class schema {
    std::string target_namespace_;
    std::vector<simple_type> simple_types_;
    std::vector<complex_type> complex_types_;
    std::vector<element_decl> elements_;
    std::vector<attribute_decl> attributes_;
    std::vector<model_group_def> model_group_defs_;
    std::vector<attribute_group_def> attribute_group_defs_;
    std::vector<schema_import> imports_;
    std::vector<schema_include> includes_;
    std::optional<open_content> default_open_content_;
    bool default_open_content_applies_to_empty_ = false;

  public:
    schema() = default;

    void
    set_target_namespace(std::string ns) {
      target_namespace_ = std::move(ns);
    }

    const std::string&
    target_namespace() const {
      return target_namespace_;
    }

    void
    add_simple_type(simple_type t) {
      simple_types_.push_back(std::move(t));
    }

    const std::vector<simple_type>&
    simple_types() const {
      return simple_types_;
    }

    void
    add_complex_type(complex_type t) {
      complex_types_.push_back(std::move(t));
    }

    const std::vector<complex_type>&
    complex_types() const {
      return complex_types_;
    }

    void
    add_element(element_decl e) {
      elements_.push_back(std::move(e));
    }

    const std::vector<element_decl>&
    elements() const {
      return elements_;
    }

    void
    add_attribute(attribute_decl a) {
      attributes_.push_back(std::move(a));
    }

    const std::vector<attribute_decl>&
    attributes() const {
      return attributes_;
    }

    void
    add_model_group_def(model_group_def g) {
      model_group_defs_.push_back(std::move(g));
    }

    const std::vector<model_group_def>&
    model_group_defs() const {
      return model_group_defs_;
    }

    void
    add_attribute_group_def(attribute_group_def g) {
      attribute_group_defs_.push_back(std::move(g));
    }

    const std::vector<attribute_group_def>&
    attribute_group_defs() const {
      return attribute_group_defs_;
    }

    void
    add_import(schema_import i) {
      imports_.push_back(std::move(i));
    }

    const std::vector<schema_import>&
    imports() const {
      return imports_;
    }

    void
    add_include(schema_include i) {
      includes_.push_back(std::move(i));
    }

    const std::vector<schema_include>&
    includes() const {
      return includes_;
    }

    void
    set_default_open_content(open_content oc, bool applies_to_empty = false) {
      default_open_content_ = std::move(oc);
      default_open_content_applies_to_empty_ = applies_to_empty;
    }

    const std::optional<open_content>&
    default_open_content() const {
      return default_open_content_;
    }

    bool
    default_open_content_applies_to_empty() const {
      return default_open_content_applies_to_empty_;
    }
  };

} // namespace xb
