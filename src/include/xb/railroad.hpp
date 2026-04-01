#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace xb::railroad {

  // -- Railroad diagram IR --

  struct node;

  /// A terminal: a named element or attribute shown as a box.
  struct terminal {
    std::string label;
    std::string type_label; // optional: shown smaller below
    bool is_attribute = false;
    bool required = true; // for attributes: required vs optional
  };

  /// A sequence of nodes rendered left-to-right.
  struct sequence {
    std::vector<node> children;
  };

  /// A choice between alternatives rendered as vertical branches.
  struct choice {
    std::vector<node> alternatives;
  };

  /// An optional bypass around a child node (minOccurs=0, maxOccurs=1).
  struct optional_node {
    std::unique_ptr<node> child;
  };

  /// A repeating loop around a child node (maxOccurs>1).
  struct repeat_node {
    std::unique_ptr<node> child;
    std::string count_label; // e.g., "1..*", "0..5", "0..*"
  };

  /// A reference to another type's diagram.
  struct reference {
    std::string label;
    qname type_name;
  };

  struct node {
    std::variant<terminal, sequence, choice, optional_node, repeat_node,
                 reference>
        content;
  };

  /// A complete diagram for one complex type.
  struct diagram {
    std::string name;
    qname type_name;
    node root;
    std::vector<terminal> attributes;
  };

  // -- Schema to railroad translation --

  /// Build a railroad diagram for a complex type.
  diagram
  build_diagram(const schema_set& schemas, const qname& type_name);

  /// Build diagrams for all complex types reachable from an element.
  std::vector<diagram>
  build_diagrams(const schema_set& schemas, const qname& element_name);

} // namespace xb::railroad
