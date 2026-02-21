#include <xb/decimal.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <compare>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

TEST_CASE("decimal default construction is zero", "[decimal]") {
  xb::decimal d;
  CHECK(d.is_zero());
  CHECK(d.to_string() == "0.0");
}

TEST_CASE("decimal construction from string", "[decimal]") {
  SECTION("zero forms all normalize") {
    CHECK(xb::decimal("0").to_string() == "0.0");
    CHECK(xb::decimal("0.0").to_string() == "0.0");
    CHECK(xb::decimal("-0.0").to_string() == "0.0");
    CHECK(xb::decimal("0.00").to_string() == "0.0");
  }
  SECTION("integer input gets .0 suffix") {
    CHECK(xb::decimal("100").to_string() == "100.0");
    CHECK(xb::decimal("-42").to_string() == "-42.0");
  }
  SECTION("trailing zeros absorbed") {
    CHECK(xb::decimal("1.50").to_string() == "1.5");
    CHECK(xb::decimal("1.00").to_string() == "1.0");
    CHECK(xb::decimal("10.0").to_string() == "10.0");
  }
  SECTION("small fractional") {
    CHECK(xb::decimal("0.1").to_string() == "0.1");
    CHECK(xb::decimal("0.01").to_string() == "0.01");
    CHECK(xb::decimal("0.001").to_string() == "0.001");
  }
  SECTION("negative fractional") {
    CHECK(xb::decimal("-3.14").to_string() == "-3.14");
  }
  SECTION("large value") {
    CHECK(xb::decimal("123456789.987654321").to_string() ==
          "123456789.987654321");
  }
  SECTION("plus sign allowed") {
    CHECK(xb::decimal("+1.5").to_string() == "1.5");
  }
}

TEST_CASE("decimal construction from invalid string throws", "[decimal]") {
  CHECK_THROWS_AS(xb::decimal(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::decimal("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::decimal("1.2.3"), std::invalid_argument);
  CHECK_THROWS_AS(xb::decimal("."), std::invalid_argument);
  CHECK_THROWS_AS(xb::decimal("-."), std::invalid_argument);
}

TEST_CASE("decimal equality and comparison", "[decimal]") {
  SECTION("equal values") {
    CHECK(xb::decimal("1.5") == xb::decimal("1.5"));
    CHECK(xb::decimal("1.50") == xb::decimal("1.5"));
    CHECK(xb::decimal("0") == xb::decimal("0.0"));
    CHECK(xb::decimal("-0") == xb::decimal("0"));
  }
  SECTION("ordering") {
    CHECK(xb::decimal("1.0") < xb::decimal("2.0"));
    CHECK(xb::decimal("-1.0") < xb::decimal("1.0"));
    CHECK(xb::decimal("1.1") < xb::decimal("1.2"));
    CHECK(xb::decimal("1.9") < xb::decimal("10.0"));
    CHECK(xb::decimal("-2.0") < xb::decimal("-1.0"));
  }
  SECTION("spaceship returns strong_ordering") {
    auto cmp = xb::decimal("1.0") <=> xb::decimal("2.0");
    CHECK(cmp == std::strong_ordering::less);
  }
}

TEST_CASE("decimal copy and move", "[decimal]") {
  xb::decimal original("3.14159");
  xb::decimal copied = original;
  CHECK(copied == original);
  CHECK(copied.to_string() == "3.14159");

  xb::decimal moved = std::move(original);
  CHECK(moved.to_string() == "3.14159");
}

TEST_CASE("decimal unary negation", "[decimal]") {
  CHECK((-xb::decimal("3.14")).to_string() == "-3.14");
  CHECK((-xb::decimal("-3.14")).to_string() == "3.14");
  CHECK((-xb::decimal("0")).is_zero());
}

TEST_CASE("decimal addition and subtraction", "[decimal]") {
  SECTION("exact: 0.1 + 0.2 = 0.3") {
    CHECK((xb::decimal("0.1") + xb::decimal("0.2")).to_string() == "0.3");
  }
  SECTION("integer-like") {
    CHECK((xb::decimal("1.0") + xb::decimal("2.0")).to_string() == "3.0");
  }
  SECTION("different exponents") {
    CHECK((xb::decimal("1.5") + xb::decimal("0.25")).to_string() == "1.75");
  }
  SECTION("subtraction") {
    CHECK((xb::decimal("1.0") - xb::decimal("0.3")).to_string() == "0.7");
  }
  SECTION("subtraction to zero") {
    CHECK((xb::decimal("1.5") - xb::decimal("1.5")).is_zero());
  }
  SECTION("negative result") {
    CHECK((xb::decimal("1.0") - xb::decimal("3.0")).to_string() == "-2.0");
  }
}

TEST_CASE("decimal multiplication", "[decimal]") {
  SECTION("simple") {
    CHECK((xb::decimal("2.0") * xb::decimal("3.0")).to_string() == "6.0");
  }
  SECTION("fractional") {
    CHECK((xb::decimal("1.5") * xb::decimal("2.5")).to_string() == "3.75");
  }
  SECTION("zero factor") {
    CHECK((xb::decimal("42.0") * xb::decimal("0")).is_zero());
  }
  SECTION("negative") {
    CHECK((xb::decimal("2.0") * xb::decimal("-3.0")).to_string() == "-6.0");
  }
}

TEST_CASE("decimal division", "[decimal]") {
  SECTION("exact") {
    CHECK((xb::decimal("6.0") / xb::decimal("2.0")).to_string() == "3.0");
  }
  SECTION("non-terminating truncated to 28 digits") {
    xb::decimal result = xb::decimal("1.0") / xb::decimal("3.0");
    std::string s = result.to_string();
    // Should start with "0.333..." and have 28 significant digits
    CHECK(s.substr(0, 5) == "0.333");
    // Count digits after decimal point
    auto dot = s.find('.');
    CHECK(dot != std::string::npos);
    // 28 significant digits of 0.333... = 28 digits after decimal point
    CHECK(s.size() - dot - 1 == 28);
  }
  SECTION("division by zero throws") {
    CHECK_THROWS_AS(xb::decimal("1.0") / xb::decimal("0"), std::domain_error);
  }
}

TEST_CASE("decimal construction from double", "[decimal]") {
  SECTION("zero") { CHECK(xb::decimal(0.0).is_zero()); }
  SECTION("simple") { CHECK(xb::decimal(1.5).to_string() == "1.5"); }
  SECTION("negative") { CHECK(xb::decimal(-2.25).to_string() == "-2.25"); }
  SECTION("large value does not crash") {
    xb::decimal d(1e20);
    double round = static_cast<double>(d);
    CHECK(std::abs(round - 1e20) / 1e20 < 1e-10);
  }
  SECTION("small value does not crash") {
    xb::decimal d(1e-10);
    double round = static_cast<double>(d);
    CHECK(std::abs(round - 1e-10) / 1e-10 < 1e-5);
  }
  SECTION("negative zero is zero") { CHECK(xb::decimal(-0.0).is_zero()); }
}

TEST_CASE("decimal explicit conversion to double", "[decimal]") {
  SECTION("zero") { CHECK(static_cast<double>(xb::decimal()) == 0.0); }
  SECTION("simple") { CHECK(static_cast<double>(xb::decimal("1.5")) == 1.5); }
  SECTION("approximate for non-representable") {
    double d = static_cast<double>(xb::decimal("0.1"));
    CHECK(std::abs(d - 0.1) < 1e-15);
  }
}

TEST_CASE("decimal stream output", "[decimal]") {
  std::ostringstream os;
  os << xb::decimal("-3.14");
  CHECK(os.str() == "-3.14");
}

TEST_CASE("decimal hash", "[decimal]") {
  SECTION("equal values hash equal") {
    std::hash<xb::decimal> hasher;
    CHECK(hasher(xb::decimal("1.5")) == hasher(xb::decimal("1.50")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::decimal, int> map;
    map[xb::decimal("3.14")] = 42;
    CHECK(map.at(xb::decimal("3.14")) == 42);
  }
}

TEST_CASE("decimal compound assignment", "[decimal]") {
  xb::decimal a("10.0");
  a += xb::decimal("2.5");
  CHECK(a == xb::decimal("12.5"));

  a -= xb::decimal("3.0");
  CHECK(a == xb::decimal("9.5"));

  a *= xb::decimal("2.0");
  CHECK(a == xb::decimal("19.0"));

  a /= xb::decimal("4.0");
  CHECK(a == xb::decimal("4.75"));
}
