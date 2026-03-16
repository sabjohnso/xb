#include <xb/xpath/parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb::xpath;

// ---------------------------------------------------------------------------
// Simple name steps
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: single name step", "[xpath][parser]") {
  auto expr = parse("Order");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 1);
  CHECK(expr->steps[0].axis == axis_kind::child);
  CHECK(expr->steps[0].node_test == "Order");
  CHECK(expr->steps[0].predicates.empty());
}

TEST_CASE("xpath_parser: child path a/b/c", "[xpath][parser]") {
  auto expr = parse("Order/Instrument/symbol");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 3);
  CHECK(expr->steps[0].node_test == "Order");
  CHECK(expr->steps[1].node_test == "Instrument");
  CHECK(expr->steps[2].node_test == "symbol");
  for (const auto& s : expr->steps)
    CHECK(s.axis == axis_kind::child);
}

// ---------------------------------------------------------------------------
// Descendant axis (//)
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: descendant-or-self via //", "[xpath][parser]") {
  auto expr = parse("Order//symbol");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 2);
  CHECK(expr->steps[0].axis == axis_kind::child);
  CHECK(expr->steps[0].node_test == "Order");
  CHECK(expr->steps[1].axis == axis_kind::descendant_or_self);
  CHECK(expr->steps[1].node_test == "symbol");
}

TEST_CASE("xpath_parser: leading // means absolute descendant",
          "[xpath][parser]") {
  auto expr = parse("//symbol");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 1);
  CHECK(expr->steps[0].axis == axis_kind::descendant_or_self);
  CHECK(expr->steps[0].node_test == "symbol");
}

// ---------------------------------------------------------------------------
// Wildcard
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: wildcard *", "[xpath][parser]") {
  auto expr = parse("Order/*");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 2);
  CHECK(expr->steps[1].node_test == "*");
}

// ---------------------------------------------------------------------------
// Predicates
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: predicate [@name='price']", "[xpath][parser]") {
  auto expr = parse("*[@name='price']");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 1);
  CHECK(expr->steps[0].node_test == "*");
  REQUIRE(expr->steps[0].predicates.size() == 1);

  auto& pred = expr->steps[0].predicates[0];
  CHECK(pred.lhs.kind == operand_kind::attribute);
  CHECK(pred.lhs.value == "name");
  CHECK(pred.op == predicate_op::eq);
  CHECK(pred.rhs.kind == operand_kind::literal);
  CHECK(pred.rhs.value == "price");
}

TEST_CASE("xpath_parser: predicate [@type='xs:decimal']", "[xpath][parser]") {
  auto expr = parse("*[@type='xs:decimal']");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps[0].predicates.size() == 1);

  auto& pred = expr->steps[0].predicates[0];
  CHECK(pred.lhs.value == "type");
  CHECK(pred.rhs.value == "xs:decimal");
}

TEST_CASE("xpath_parser: step with multiple predicates", "[xpath][parser]") {
  auto expr = parse("*[@name='price'][@type='xs:decimal']");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps[0].predicates.size() == 2);
  CHECK(expr->steps[0].predicates[0].lhs.value == "name");
  CHECK(expr->steps[0].predicates[1].lhs.value == "type");
}

// ---------------------------------------------------------------------------
// Combined paths with predicates
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: path with predicate on intermediate step",
          "[xpath][parser]") {
  auto expr = parse("Order/Instrument[@name='equity']/symbol");
  REQUIRE(expr.has_value());
  REQUIRE(expr->steps.size() == 3);
  CHECK(expr->steps[1].node_test == "Instrument");
  REQUIRE(expr->steps[1].predicates.size() == 1);
  CHECK(expr->steps[1].predicates[0].rhs.value == "equity");
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST_CASE("xpath_parser: empty string returns nullopt", "[xpath][parser]") {
  auto expr = parse("");
  CHECK_FALSE(expr.has_value());
}

TEST_CASE("xpath_parser: unclosed predicate returns nullopt",
          "[xpath][parser]") {
  auto expr = parse("*[@name='foo'");
  CHECK_FALSE(expr.has_value());
}

TEST_CASE("xpath_parser: trailing slash returns nullopt", "[xpath][parser]") {
  auto expr = parse("Order/");
  CHECK_FALSE(expr.has_value());
}
