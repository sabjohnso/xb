#pragma once

#include <string>
#include <vector>

namespace xb::xpath {

  /// XPath axis kinds (subset for schema navigation).
  enum class axis_kind {
    child,              // default: direct child
    descendant_or_self, // // abbreviation
  };

  /// Predicate comparison operators.
  enum class predicate_op {
    eq, // =
    ne, // !=
  };

  /// Operand in a predicate expression.
  enum class operand_kind {
    attribute, // @name
    literal,   // 'value'
  };

  struct operand {
    operand_kind kind;
    std::string value;
  };

  /// A predicate: [@attr='value'] or [@attr!='value']
  struct predicate {
    operand lhs;
    predicate_op op = predicate_op::eq;
    operand rhs;
  };

  /// A single step in an XPath path expression.
  struct step_expr {
    axis_kind axis = axis_kind::child;
    std::string node_test; // element name or "*"
    std::vector<predicate> predicates;
  };

  /// A path expression: sequence of steps separated by / or //.
  struct path_expr {
    std::vector<step_expr> steps;
  };

} // namespace xb::xpath
