#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace xb::rng {

  // Forward declarations
  class pattern;
  class name_class;

  // ---------------------------------------------------------------------------
  // Name class node types
  // ---------------------------------------------------------------------------

  struct specific_name {
    std::string ns;
    std::string local_name;
  };

  struct any_name_nc {
    std::unique_ptr<name_class> except;
  };

  struct ns_name_nc {
    std::string ns;
    std::unique_ptr<name_class> except;
  };

  struct choice_name_class {
    std::unique_ptr<name_class> left;
    std::unique_ptr<name_class> right;
  };

  // ---------------------------------------------------------------------------
  // Name class
  // ---------------------------------------------------------------------------

  class name_class {
  public:
    using variant_type =
        std::variant<specific_name, any_name_nc, ns_name_nc, choice_name_class>;

    name_class(variant_type v) : data_(std::move(v)) {}

    name_class(specific_name v) : data_(std::move(v)) {}

    name_class(any_name_nc v) : data_(std::move(v)) {}

    name_class(ns_name_nc v) : data_(std::move(v)) {}

    name_class(choice_name_class v) : data_(std::move(v)) {}

    name_class(const name_class&) = delete;
    name_class&
    operator=(const name_class&) = delete;
    name_class(name_class&&) = default;
    name_class&
    operator=(name_class&&) = default;

    const variant_type&
    data() const {
      return data_;
    }

    variant_type&
    data() {
      return data_;
    }

    template <typename T>
    bool
    holds() const {
      return std::holds_alternative<T>(data_);
    }

    template <typename T>
    const T&
    get() const {
      return std::get<T>(data_);
    }

    template <typename T>
    T&
    get() {
      return std::get<T>(data_);
    }

  private:
    variant_type data_;
  };

  // ---------------------------------------------------------------------------
  // Pattern node types
  // ---------------------------------------------------------------------------

  struct element_pattern {
    name_class name;
    std::unique_ptr<pattern> content;
  };

  struct attribute_pattern {
    name_class name;
    std::unique_ptr<pattern> content;
  };

  struct group_pattern {
    std::unique_ptr<pattern> left;
    std::unique_ptr<pattern> right;
  };

  struct interleave_pattern {
    std::unique_ptr<pattern> left;
    std::unique_ptr<pattern> right;
  };

  struct choice_pattern {
    std::unique_ptr<pattern> left;
    std::unique_ptr<pattern> right;
  };

  struct one_or_more_pattern {
    std::unique_ptr<pattern> content;
  };

  struct zero_or_more_pattern {
    std::unique_ptr<pattern> content;
  };

  struct optional_pattern {
    std::unique_ptr<pattern> content;
  };

  struct mixed_pattern {
    std::unique_ptr<pattern> content;
  };

  struct ref_pattern {
    std::string name;
  };

  struct parent_ref_pattern {
    std::string name;
  };

  struct empty_pattern {};

  struct text_pattern {};

  struct not_allowed_pattern {};

  struct data_param {
    std::string name;
    std::string value;
  };

  struct data_pattern {
    std::string datatype_library;
    std::string type;
    std::vector<data_param> params;
    std::unique_ptr<pattern> except;
  };

  struct value_pattern {
    std::string datatype_library;
    std::string type;
    std::string value;
    std::string ns;
  };

  struct list_pattern {
    std::unique_ptr<pattern> content;
  };

  struct external_ref_pattern {
    std::string href;
    std::string ns;
  };

  // ---------------------------------------------------------------------------
  // Grammar components
  // ---------------------------------------------------------------------------

  enum class combine_method { none, choice, interleave };

  struct define {
    std::string name;
    combine_method combine = combine_method::none;
    std::unique_ptr<pattern> body;
  };

  struct include_directive {
    std::string href;
    std::string ns;
    std::vector<define> overrides;
    std::unique_ptr<pattern> start_override;
  };

  struct grammar_pattern {
    std::unique_ptr<pattern> start;
    std::vector<define> defines;
    std::vector<include_directive> includes;
  };

  // ---------------------------------------------------------------------------
  // Pattern
  // ---------------------------------------------------------------------------

  class pattern {
  public:
    using variant_type = std::variant<
        element_pattern, attribute_pattern, group_pattern, interleave_pattern,
        choice_pattern, one_or_more_pattern, zero_or_more_pattern,
        optional_pattern, mixed_pattern, ref_pattern, parent_ref_pattern,
        empty_pattern, text_pattern, not_allowed_pattern, data_pattern,
        value_pattern, list_pattern, external_ref_pattern, grammar_pattern>;

    pattern(variant_type v) : data_(std::move(v)) {}

    pattern(element_pattern v) : data_(std::move(v)) {}

    pattern(attribute_pattern v) : data_(std::move(v)) {}

    pattern(group_pattern v) : data_(std::move(v)) {}

    pattern(interleave_pattern v) : data_(std::move(v)) {}

    pattern(choice_pattern v) : data_(std::move(v)) {}

    pattern(one_or_more_pattern v) : data_(std::move(v)) {}

    pattern(zero_or_more_pattern v) : data_(std::move(v)) {}

    pattern(optional_pattern v) : data_(std::move(v)) {}

    pattern(mixed_pattern v) : data_(std::move(v)) {}

    pattern(ref_pattern v) : data_(std::move(v)) {}

    pattern(parent_ref_pattern v) : data_(std::move(v)) {}

    pattern(empty_pattern v) : data_(std::move(v)) {}

    pattern(text_pattern v) : data_(std::move(v)) {}

    pattern(not_allowed_pattern v) : data_(std::move(v)) {}

    pattern(data_pattern v) : data_(std::move(v)) {}

    pattern(value_pattern v) : data_(std::move(v)) {}

    pattern(list_pattern v) : data_(std::move(v)) {}

    pattern(external_ref_pattern v) : data_(std::move(v)) {}

    pattern(grammar_pattern v) : data_(std::move(v)) {}

    pattern(const pattern&) = delete;
    pattern&
    operator=(const pattern&) = delete;
    pattern(pattern&&) = default;
    pattern&
    operator=(pattern&&) = default;

    const variant_type&
    data() const {
      return data_;
    }

    variant_type&
    data() {
      return data_;
    }

    template <typename T>
    bool
    holds() const {
      return std::holds_alternative<T>(data_);
    }

    template <typename T>
    const T&
    get() const {
      return std::get<T>(data_);
    }

    template <typename T>
    T&
    get() {
      return std::get<T>(data_);
    }

  private:
    variant_type data_;
  };

  // ---------------------------------------------------------------------------
  // Factory helpers
  // ---------------------------------------------------------------------------

  template <typename T>
  std::unique_ptr<pattern>
  make_pattern(T&& node) {
    return std::make_unique<pattern>(std::forward<T>(node));
  }

  template <typename T>
  std::unique_ptr<name_class>
  make_name_class(T&& node) {
    return std::make_unique<name_class>(std::forward<T>(node));
  }

} // namespace xb::rng
