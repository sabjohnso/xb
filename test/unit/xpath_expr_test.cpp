#include <xb/xpath_expr.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

using namespace xb;

// Subgroup 3a: Simple value comparisons ($value)

TEST_CASE("xpath: $value > 0", "[xpath_expr]") {
  xpath_context ctx{"value"};
  auto result = translate_xpath_assertion("$value > 0", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value > 0)");
}

TEST_CASE("xpath: $value = 'hello'", "[xpath_expr]") {
  xpath_context ctx{"value"};
  auto result = translate_xpath_assertion("$value = 'hello'", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value == \"hello\")");
}

TEST_CASE("xpath: $value >= 3.14", "[xpath_expr]") {
  xpath_context ctx{"value"};
  auto result = translate_xpath_assertion("$value >= 3.14", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value >= 3.14)");
}

TEST_CASE("xpath: $value != 0", "[xpath_expr]") {
  xpath_context ctx{"value"};
  auto result = translate_xpath_assertion("$value != 0", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value != 0)");
}

// Subgroup 3b: Attribute references (@attr)

TEST_CASE("xpath: @attr = 'foo'", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("@attr = 'foo'", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.attr == \"foo\")");
}

TEST_CASE("xpath: @status != 'active'", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("@status != 'active'", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.status != \"active\")");
}

// Subgroup 3c: Field references

TEST_CASE("xpath: end >= start", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("end >= start", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.end >= value.start)");
}

TEST_CASE("xpath: amount > 0", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("amount > 0", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.amount > 0)");
}

TEST_CASE("xpath: x < 100", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("x < 100", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.x < 100)");
}

TEST_CASE("xpath: x <= y", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("x <= y", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.x <= value.y)");
}

// Subgroup 3d: Boolean connectives

TEST_CASE("xpath: and connective", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("end >= start and amount > 0", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "((value.end >= value.start) && (value.amount > 0))");
}

TEST_CASE("xpath: or connective", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("a = 1 or b = 2", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "((value.a == 1) || (value.b == 2))");
}

TEST_CASE("xpath: not()", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("not(x > 5)", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(!(value.x > 5))");
}

TEST_CASE("xpath: combined and/or/not", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result =
      translate_xpath_assertion("a > 0 and not(b = 0) or c < 10", ctx);
  REQUIRE(result.has_value());
  // 'or' has lower precedence than 'and': (a > 0 and not(b = 0)) or (c < 10)
  CHECK(result.value() ==
        "(((value.a > 0) && (!(value.b == 0))) || (value.c < 10))");
}

TEST_CASE("xpath: parenthesized expression", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("(a > 0)", ctx);
  REQUIRE(result.has_value());
  CHECK(result.value() == "(value.a > 0)");
}

// Subgroup 3e: Unsupported expressions

TEST_CASE("xpath: fn:string-length unsupported", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("fn:string-length($value) > 0", ctx);
  CHECK_FALSE(result.has_value());
}

TEST_CASE("xpath: // path unsupported", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("//element", ctx);
  CHECK_FALSE(result.has_value());
}

TEST_CASE("xpath: empty expression unsupported", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("", ctx);
  CHECK_FALSE(result.has_value());
}

TEST_CASE("xpath: whitespace-only unsupported", "[xpath_expr]") {
  xpath_context ctx{"value."};
  auto result = translate_xpath_assertion("   ", ctx);
  CHECK_FALSE(result.has_value());
}
