#pragma once

#include <xb/any_attribute.hpp>
#include <xb/xml_escape.hpp>

#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace xb {

  class xml_reader;
  class xml_writer;

  class any_element {
    qname name_;
    std::vector<any_attribute> attributes_;
    std::vector<std::variant<std::string, any_element>> children_;

  public:
    using child = std::variant<std::string, any_element>;

    any_element() = default;

    any_element(qname name, std::vector<any_attribute> attributes,
                std::vector<child> children)
        : name_(std::move(name)), attributes_(std::move(attributes)),
          children_(std::move(children)) {}

    explicit any_element(xml_reader& reader);

    void
    write(xml_writer& writer) const;

    const qname&
    name() const {
      return name_;
    }

    const std::vector<any_attribute>&
    attributes() const {
      return attributes_;
    }

    const std::vector<child>&
    children() const {
      return children_;
    }

    // Declared here, defaulted out-of-line after the class is complete.
    // Ordering (operator<=>) is intentionally omitted: three-way comparison
    // of std::variant<std::string, any_element> is broken on clang with
    // libstdc++ due to the recursive type, and ordering a tree is not useful.
    bool
    operator==(const any_element&) const;

    friend std::ostream&
    operator<<(std::ostream& os, const any_element& e) {
      os << '<' << e.name_;
      for (const auto& attr : e.attributes_) {
        os << ' ' << attr;
      }
      if (e.children_.empty()) { return os << "/>"; }
      os << '>';
      for (const auto& child : e.children_) {
        std::visit(
            [&os](const auto& v) {
              using T = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<T, std::string>) {
                escape_text(os, v);
              } else {
                os << v;
              }
            },
            child);
      }
      return os << "</" << e.name_ << '>';
    }
  };

  // Defined out-of-line: any_element must be complete before the compiler
  // instantiates equality comparison of std::variant<std::string, any_element>.
  inline bool
  any_element::operator==(const any_element& other) const {
    return name_ == other.name_ && attributes_ == other.attributes_ &&
           children_ == other.children_;
  }

} // namespace xb
