#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/xpath/ast.hpp>
#include <xb/xpath/parser.hpp>

#include <string>
#include <vector>

namespace xb::xpath {

  namespace detail {

    inline bool
    matches_predicate(const predicate& pred, const complex_type& ct) {
      std::string actual;
      if (pred.lhs.kind == operand_kind::attribute) {
        if (pred.lhs.value == "name")
          actual = ct.name().local_name();
        else if (pred.lhs.value == "namespace")
          actual = ct.name().namespace_uri();
        else
          return false;
      } else {
        return false;
      }

      if (pred.op == predicate_op::eq) return actual == pred.rhs.value;
      if (pred.op == predicate_op::ne) return actual != pred.rhs.value;
      return false;
    }

  } // namespace detail

  /// Check if a complex type matches a single path step.
  inline bool
  matches_step(const step_expr& step, const complex_type& ct) {
    if (step.node_test != "*" && step.node_test != ct.name().local_name())
      return false;

    for (const auto& pred : step.predicates) {
      if (!detail::matches_predicate(pred, ct)) return false;
    }

    return true;
  }

  /// Evaluate an XPath path expression against a schema_set,
  /// returning the qnames of matching complex types.
  inline std::vector<qname>
  eval_schema_path(std::string_view xpath_str, const schema_set& schemas) {
    auto expr = parse(xpath_str);
    if (!expr.has_value() || expr->steps.empty()) return {};

    std::vector<const complex_type*> candidates;
    for (const auto& s : schemas.schemas())
      for (const auto& ct : s.complex_types())
        candidates.push_back(&ct);

    const auto& final_step = expr->steps.back();

    std::vector<qname> results;
    for (const auto* ct : candidates) {
      if (!matches_step(final_step, *ct)) continue;
      results.push_back(ct->name());
    }

    return results;
  }

} // namespace xb::xpath
