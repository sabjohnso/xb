#pragma once

#include <xb/xpath/ast.hpp>

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace xb::xpath {

  /// Parse an XPath path expression (subset for BES selectors).
  ///
  /// Supported syntax:
  ///   a/b/c          child path
  ///   a//b           descendant-or-self
  ///   //a            absolute descendant
  ///   *              wildcard
  ///   step[@a='v']   predicate
  ///
  /// Returns nullopt on parse failure.
  inline std::optional<path_expr>
  parse(std::string_view input) {
    if (input.empty()) return std::nullopt;

    path_expr result;
    std::size_t pos = 0;

    auto skip_ws = [&] {
      while (pos < input.size() &&
             std::isspace(static_cast<unsigned char>(input[pos])))
        ++pos;
    };

    auto is_name_start = [](char c) {
      return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    };

    auto is_name_char = [](char c) {
      return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
             c == '-' || c == '.';
    };

    auto read_name = [&]() -> std::string {
      auto start = pos;
      if (pos < input.size() && input[pos] == '*') {
        ++pos;
        return "*";
      }
      if (pos >= input.size() || !is_name_start(input[pos])) return "";
      ++pos;
      while (pos < input.size() && is_name_char(input[pos]))
        ++pos;
      return std::string(input.substr(start, pos - start));
    };

    auto read_string_literal = [&]() -> std::optional<std::string> {
      if (pos >= input.size()) return std::nullopt;
      char quote = input[pos];
      if (quote != '\'' && quote != '"') return std::nullopt;
      ++pos;
      auto start = pos;
      while (pos < input.size() && input[pos] != quote)
        ++pos;
      if (pos >= input.size()) return std::nullopt;
      auto value = std::string(input.substr(start, pos - start));
      ++pos; // consume closing quote
      return value;
    };

    auto parse_predicate = [&]() -> std::optional<predicate> {
      // Expect [@attr op 'value']
      skip_ws();
      if (pos >= input.size() || input[pos] != '@') return std::nullopt;
      ++pos; // consume @

      // Read attribute name (allow ':' for namespace prefixes like xs:decimal)
      auto attr_start = pos;
      while (pos < input.size() &&
             (is_name_char(input[pos]) || input[pos] == ':'))
        ++pos;
      if (pos == attr_start) return std::nullopt;
      std::string attr(input.substr(attr_start, pos - attr_start));

      skip_ws();

      // Comparison operator
      predicate_op op = predicate_op::eq;
      if (pos < input.size() && input[pos] == '=') {
        ++pos;
        op = predicate_op::eq;
      } else if (pos + 1 < input.size() && input[pos] == '!' &&
                 input[pos + 1] == '=') {
        pos += 2;
        op = predicate_op::ne;
      } else {
        return std::nullopt;
      }

      skip_ws();

      // String literal value
      auto value = read_string_literal();
      if (!value.has_value()) return std::nullopt;

      return predicate{{operand_kind::attribute, std::move(attr)},
                       op,
                       {operand_kind::literal, std::move(*value)}};
    };

    auto parse_step = [&](axis_kind axis) -> std::optional<step_expr> {
      skip_ws();
      auto name = read_name();
      if (name.empty()) return std::nullopt;

      step_expr step;
      step.axis = axis;
      step.node_test = std::move(name);

      // Parse zero or more predicates
      while (pos < input.size() && input[pos] == '[') {
        ++pos; // consume [
        auto pred = parse_predicate();
        if (!pred.has_value()) return std::nullopt;
        step.predicates.push_back(std::move(*pred));
        skip_ws();
        if (pos >= input.size() || input[pos] != ']') return std::nullopt;
        ++pos; // consume ]
      }

      return step;
    };

    // Handle leading //
    axis_kind first_axis = axis_kind::child;
    if (input.size() >= 2 && input[0] == '/' && input[1] == '/') {
      first_axis = axis_kind::descendant_or_self;
      pos = 2;
    }

    // Parse first step
    auto first = parse_step(first_axis);
    if (!first.has_value()) return std::nullopt;
    result.steps.push_back(std::move(*first));

    // Parse remaining steps separated by / or //
    while (pos < input.size() && input[pos] == '/') {
      axis_kind axis = axis_kind::child;
      ++pos; // consume first /
      if (pos < input.size() && input[pos] == '/') {
        axis = axis_kind::descendant_or_self;
        ++pos; // consume second /
      }

      auto step = parse_step(axis);
      if (!step.has_value()) return std::nullopt;
      result.steps.push_back(std::move(*step));
    }

    // Must consume entire input
    skip_ws();
    if (pos != input.size()) return std::nullopt;

    return result;
  }

} // namespace xb::xpath
