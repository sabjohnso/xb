#include <xb/integer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <compare>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

TEST_CASE("integer default construction is zero", "[integer]") {
  xb::integer i;
  CHECK(i.is_zero());
  CHECK(i.to_string() == "0");
}

TEST_CASE("integer construction from int64_t", "[integer]") {
  SECTION("zero") {
    xb::integer i(int64_t{0});
    CHECK(i.is_zero());
    CHECK(i.to_string() == "0");
    CHECK(i.sign() == xb::integer::sign_type::positive);
  }
  SECTION("small positive") {
    xb::integer i(int64_t{42});
    CHECK_FALSE(i.is_zero());
    CHECK(i.to_string() == "42");
    CHECK(i.sign() == xb::integer::sign_type::positive);
  }
  SECTION("small negative") {
    xb::integer i(int64_t{-42});
    CHECK(i.to_string() == "-42");
    CHECK(i.sign() == xb::integer::sign_type::negative);
  }
  SECTION("max int64") {
    xb::integer i(INT64_MAX);
    CHECK(i.to_string() == "9223372036854775807");
  }
  SECTION("min int64") {
    xb::integer i(INT64_MIN);
    CHECK(i.to_string() == "-9223372036854775808");
  }
  SECTION("value fitting in one limb") {
    xb::integer i(int64_t{4294967295}); // UINT32_MAX
    CHECK(i.to_string() == "4294967295");
  }
  SECTION("value requiring two limbs") {
    xb::integer i(int64_t{4294967296}); // UINT32_MAX + 1
    CHECK(i.to_string() == "4294967296");
  }
}

TEST_CASE("integer construction from uint64_t", "[integer]") {
  SECTION("zero") {
    xb::integer i(uint64_t{0});
    CHECK(i.is_zero());
    CHECK(i.to_string() == "0");
  }
  SECTION("small value") {
    xb::integer i(uint64_t{123});
    CHECK(i.to_string() == "123");
  }
  SECTION("UINT64_MAX requires two limbs") {
    xb::integer i(UINT64_MAX);
    CHECK(i.to_string() == "18446744073709551615");
  }
  SECTION("uint64 always positive") {
    xb::integer i(uint64_t{42});
    CHECK(i.sign() == xb::integer::sign_type::positive);
  }
}

TEST_CASE("integer construction from string", "[integer]") {
  SECTION("zero") {
    xb::integer i("0");
    CHECK(i.is_zero());
    CHECK(i.to_string() == "0");
  }
  SECTION("positive") {
    xb::integer i("12345");
    CHECK(i.to_string() == "12345");
  }
  SECTION("negative") {
    xb::integer i("-67890");
    CHECK(i.to_string() == "-67890");
  }
  SECTION("leading zeros stripped") {
    xb::integer i("00042");
    CHECK(i.to_string() == "42");
  }
  SECTION("negative with leading zeros") {
    xb::integer i("-00042");
    CHECK(i.to_string() == "-42");
  }
  SECTION("negative zero normalizes to positive zero") {
    xb::integer i("-0");
    CHECK(i.is_zero());
    CHECK(i.to_string() == "0");
    CHECK(i.sign() == xb::integer::sign_type::positive);
  }
  SECTION("large number (100+ digits)") {
    std::string big(120, '9');
    xb::integer i(big);
    CHECK(i.to_string() == big);
  }
  SECTION("plus sign allowed") {
    xb::integer i("+42");
    CHECK(i.to_string() == "42");
  }
}

TEST_CASE("integer construction from invalid string throws", "[integer]") {
  CHECK_THROWS_AS(xb::integer(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::integer("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::integer("-"), std::invalid_argument);
  CHECK_THROWS_AS(xb::integer("+"), std::invalid_argument);
  CHECK_THROWS_AS(xb::integer("12.34"), std::invalid_argument);
  CHECK_THROWS_AS(xb::integer("12 34"), std::invalid_argument);
}

TEST_CASE("integer equality", "[integer]") {
  CHECK(xb::integer(int64_t{0}) == xb::integer(int64_t{0}));
  CHECK(xb::integer(int64_t{42}) == xb::integer("42"));
  CHECK_FALSE(xb::integer(int64_t{42}) == xb::integer(int64_t{43}));
  CHECK_FALSE(xb::integer(int64_t{42}) == xb::integer(int64_t{-42}));
  CHECK(xb::integer(UINT64_MAX) == xb::integer("18446744073709551615"));
}

TEST_CASE("integer comparison", "[integer]") {
  SECTION("same sign, different magnitude") {
    CHECK(xb::integer(int64_t{1}) < xb::integer(int64_t{2}));
    CHECK(xb::integer(int64_t{2}) > xb::integer(int64_t{1}));
    CHECK(xb::integer(int64_t{-2}) < xb::integer(int64_t{-1}));
    CHECK(xb::integer(int64_t{-1}) > xb::integer(int64_t{-2}));
  }
  SECTION("different signs") {
    CHECK(xb::integer(int64_t{-1}) < xb::integer(int64_t{1}));
    CHECK(xb::integer(int64_t{1}) > xb::integer(int64_t{-1}));
  }
  SECTION("zero comparisons") {
    CHECK(xb::integer(int64_t{0}) <= xb::integer(int64_t{0}));
    CHECK(xb::integer(int64_t{0}) >= xb::integer(int64_t{0}));
    CHECK(xb::integer(int64_t{-1}) < xb::integer(int64_t{0}));
    CHECK(xb::integer(int64_t{0}) < xb::integer(int64_t{1}));
  }
  SECTION("different number of limbs") {
    xb::integer small(int64_t{1});
    xb::integer big(UINT64_MAX);
    CHECK(small < big);
    CHECK(big > small);
  }
  SECTION("spaceship returns strong_ordering") {
    auto cmp = xb::integer(int64_t{5}) <=> xb::integer(int64_t{10});
    CHECK(cmp == std::strong_ordering::less);
  }
}

TEST_CASE("integer copy semantics", "[integer]") {
  xb::integer original(int64_t{123456789});
  xb::integer copied = original;
  CHECK(copied == original);
  CHECK(copied.to_string() == "123456789");
}

TEST_CASE("integer move semantics", "[integer]") {
  xb::integer source(int64_t{123456789});
  xb::integer moved = std::move(source);
  CHECK(moved.to_string() == "123456789");
}

TEST_CASE("integer unary negation", "[integer]") {
  SECTION("negate positive") {
    xb::integer i(int64_t{42});
    xb::integer neg = -i;
    CHECK(neg.to_string() == "-42");
  }
  SECTION("negate negative") {
    xb::integer i(int64_t{-42});
    xb::integer pos = -i;
    CHECK(pos.to_string() == "42");
  }
  SECTION("negate zero stays positive zero") {
    xb::integer i;
    xb::integer neg = -i;
    CHECK(neg.is_zero());
    CHECK(neg.sign() == xb::integer::sign_type::positive);
  }
  SECTION("unary plus is identity") {
    xb::integer i(int64_t{42});
    xb::integer same = +i;
    CHECK(same == i);
  }
}

TEST_CASE("integer addition", "[integer]") {
  SECTION("zero + zero") { CHECK((xb::integer() + xb::integer()).is_zero()); }
  SECTION("zero + x = x") {
    xb::integer x(int64_t{42});
    CHECK(xb::integer() + x == x);
    CHECK(x + xb::integer() == x);
  }
  SECTION("small positive + positive") {
    CHECK(xb::integer(int64_t{3}) + xb::integer(int64_t{4}) ==
          xb::integer(int64_t{7}));
  }
  SECTION("carry across limb boundary") {
    xb::integer a(uint64_t{UINT32_MAX});
    xb::integer b(int64_t{1});
    CHECK((a + b).to_string() == "4294967296");
  }
  SECTION("positive + negative (result positive)") {
    CHECK(xb::integer(int64_t{10}) + xb::integer(int64_t{-3}) ==
          xb::integer(int64_t{7}));
  }
  SECTION("positive + negative (result negative)") {
    CHECK(xb::integer(int64_t{3}) + xb::integer(int64_t{-10}) ==
          xb::integer(int64_t{-7}));
  }
  SECTION("positive + negative (result zero)") {
    CHECK((xb::integer(int64_t{5}) + xb::integer(int64_t{-5})).is_zero());
  }
  SECTION("negative + negative") {
    CHECK(xb::integer(int64_t{-3}) + xb::integer(int64_t{-4}) ==
          xb::integer(int64_t{-7}));
  }
  SECTION("large values") {
    xb::integer a("99999999999999999999999999999");
    xb::integer b("1");
    CHECK((a + b).to_string() == "100000000000000000000000000000");
  }
}

TEST_CASE("integer subtraction", "[integer]") {
  SECTION("x - x = 0") {
    xb::integer x(int64_t{42});
    CHECK((x - x).is_zero());
  }
  SECTION("x - 0 = x") {
    xb::integer x(int64_t{42});
    CHECK(x - xb::integer() == x);
  }
  SECTION("0 - x = -x") {
    xb::integer x(int64_t{42});
    CHECK(xb::integer() - x == -x);
  }
  SECTION("positive - positive, positive result") {
    CHECK(xb::integer(int64_t{10}) - xb::integer(int64_t{3}) ==
          xb::integer(int64_t{7}));
  }
  SECTION("positive - positive, negative result") {
    CHECK(xb::integer(int64_t{3}) - xb::integer(int64_t{10}) ==
          xb::integer(int64_t{-7}));
  }
  SECTION("negative - positive") {
    CHECK(xb::integer(int64_t{-3}) - xb::integer(int64_t{4}) ==
          xb::integer(int64_t{-7}));
  }
  SECTION("borrow across limb boundary") {
    xb::integer a("4294967296"); // 2^32
    xb::integer b(int64_t{1});
    CHECK((a - b).to_string() == "4294967295");
  }
  SECTION("negative - negative") {
    CHECK(xb::integer(int64_t{-3}) - xb::integer(int64_t{-4}) ==
          xb::integer(int64_t{1}));
    CHECK(xb::integer(int64_t{-4}) - xb::integer(int64_t{-3}) ==
          xb::integer(int64_t{-1}));
  }
}

TEST_CASE("integer multiplication", "[integer]") {
  SECTION("x * 0 = 0") {
    CHECK((xb::integer(int64_t{42}) * xb::integer()).is_zero());
    CHECK((xb::integer() * xb::integer(int64_t{42})).is_zero());
  }
  SECTION("x * 1 = x") {
    xb::integer x(int64_t{42});
    CHECK(x * xb::integer(int64_t{1}) == x);
  }
  SECTION("small values") {
    CHECK(xb::integer(int64_t{6}) * xb::integer(int64_t{7}) ==
          xb::integer(int64_t{42}));
  }
  SECTION("sign: positive * negative = negative") {
    CHECK(xb::integer(int64_t{6}) * xb::integer(int64_t{-7}) ==
          xb::integer(int64_t{-42}));
  }
  SECTION("sign: negative * negative = positive") {
    CHECK(xb::integer(int64_t{-6}) * xb::integer(int64_t{-7}) ==
          xb::integer(int64_t{42}));
  }
  SECTION("cross-limb multiplication") {
    xb::integer a(uint64_t{UINT32_MAX});
    xb::integer b(uint64_t{UINT32_MAX});
    // (2^32 - 1)^2 = 2^64 - 2^33 + 1 = 18446744065119617025
    CHECK((a * b).to_string() == "18446744065119617025");
  }
  SECTION("large factorials") {
    // 20! = 2432902008176640000
    xb::integer result(int64_t{1});
    for (int64_t i = 2; i <= 20; ++i) {
      result = result * xb::integer(i);
    }
    CHECK(result.to_string() == "2432902008176640000");
  }
}

TEST_CASE("integer division", "[integer]") {
  SECTION("x / 1 = x") {
    CHECK(xb::integer(int64_t{42}) / xb::integer(int64_t{1}) ==
          xb::integer(int64_t{42}));
  }
  SECTION("0 / x = 0") {
    CHECK((xb::integer() / xb::integer(int64_t{5})).is_zero());
  }
  SECTION("exact division") {
    CHECK(xb::integer(int64_t{42}) / xb::integer(int64_t{6}) ==
          xb::integer(int64_t{7}));
  }
  SECTION("truncation toward zero: positive / positive") {
    CHECK(xb::integer(int64_t{7}) / xb::integer(int64_t{2}) ==
          xb::integer(int64_t{3}));
  }
  SECTION("truncation toward zero: negative / positive") {
    CHECK(xb::integer(int64_t{-7}) / xb::integer(int64_t{2}) ==
          xb::integer(int64_t{-3}));
  }
  SECTION("truncation toward zero: positive / negative") {
    CHECK(xb::integer(int64_t{7}) / xb::integer(int64_t{-2}) ==
          xb::integer(int64_t{-3}));
  }
  SECTION("truncation toward zero: negative / negative") {
    CHECK(xb::integer(int64_t{-7}) / xb::integer(int64_t{-2}) ==
          xb::integer(int64_t{3}));
  }
  SECTION("division by zero throws") {
    CHECK_THROWS_AS(xb::integer(int64_t{42}) / xb::integer(),
                    std::domain_error);
  }
  SECTION("large dividend, small divisor") {
    xb::integer a("100000000000000000000");
    xb::integer b(int64_t{3});
    CHECK((a / b).to_string() == "33333333333333333333");
  }
}

TEST_CASE("integer modulus", "[integer]") {
  SECTION("x % 1 = 0") {
    CHECK((xb::integer(int64_t{42}) % xb::integer(int64_t{1})).is_zero());
  }
  SECTION("basic modulus") {
    CHECK(xb::integer(int64_t{7}) % xb::integer(int64_t{3}) ==
          xb::integer(int64_t{1}));
  }
  SECTION("modulus sign follows dividend") {
    CHECK(xb::integer(int64_t{-7}) % xb::integer(int64_t{3}) ==
          xb::integer(int64_t{-1}));
    CHECK(xb::integer(int64_t{7}) % xb::integer(int64_t{-3}) ==
          xb::integer(int64_t{1}));
  }
  SECTION("modulus by zero throws") {
    CHECK_THROWS_AS(xb::integer(int64_t{42}) % xb::integer(),
                    std::domain_error);
  }
  SECTION("division identity: a == (a/b)*b + a%b") {
    xb::integer a(int64_t{12345});
    xb::integer b(int64_t{67});
    CHECK(a == (a / b) * b + a % b);

    xb::integer c(int64_t{-12345});
    CHECK(c == (c / b) * b + c % b);
  }
}

TEST_CASE("integer explicit conversion to int64_t", "[integer]") {
  SECTION("zero") { CHECK(static_cast<int64_t>(xb::integer()) == 0); }
  SECTION("positive fits") {
    CHECK(static_cast<int64_t>(xb::integer(int64_t{42})) == 42);
  }
  SECTION("negative fits") {
    CHECK(static_cast<int64_t>(xb::integer(int64_t{-42})) == -42);
  }
  SECTION("INT64_MAX") {
    CHECK(static_cast<int64_t>(xb::integer(INT64_MAX)) == INT64_MAX);
  }
  SECTION("INT64_MIN") {
    CHECK(static_cast<int64_t>(xb::integer(INT64_MIN)) == INT64_MIN);
  }
  SECTION("overflow throws") {
    CHECK_THROWS_AS(static_cast<int64_t>(xb::integer(UINT64_MAX)),
                    std::overflow_error);
    xb::integer too_big("9223372036854775808"); // INT64_MAX + 1
    CHECK_THROWS_AS(static_cast<int64_t>(too_big), std::overflow_error);
  }
}

TEST_CASE("integer explicit conversion to uint64_t", "[integer]") {
  SECTION("zero") { CHECK(static_cast<uint64_t>(xb::integer()) == 0); }
  SECTION("positive fits") {
    CHECK(static_cast<uint64_t>(xb::integer(uint64_t{42})) == 42);
  }
  SECTION("UINT64_MAX") {
    CHECK(static_cast<uint64_t>(xb::integer(UINT64_MAX)) == UINT64_MAX);
  }
  SECTION("negative throws") {
    CHECK_THROWS_AS(static_cast<uint64_t>(xb::integer(int64_t{-1})),
                    std::overflow_error);
  }
  SECTION("too large throws") {
    xb::integer too_big("18446744073709551616"); // UINT64_MAX + 1
    CHECK_THROWS_AS(static_cast<uint64_t>(too_big), std::overflow_error);
  }
}

TEST_CASE("integer explicit conversion to double", "[integer]") {
  SECTION("zero") { CHECK(static_cast<double>(xb::integer()) == 0.0); }
  SECTION("small value is exact") {
    CHECK(static_cast<double>(xb::integer(int64_t{42})) == 42.0);
  }
  SECTION("negative") {
    CHECK(static_cast<double>(xb::integer(int64_t{-42})) == -42.0);
  }
  SECTION("large value is approximate") {
    xb::integer big("123456789012345678901234567890");
    double d = static_cast<double>(big);
    CHECK(d > 0);
    CHECK(std::abs(d - 1.2345678901234568e29) / 1.2345678901234568e29 < 1e-10);
  }
}

TEST_CASE("integer stream output", "[integer]") {
  xb::integer i(int64_t{-12345});
  std::ostringstream os;
  os << i;
  CHECK(os.str() == "-12345");
}

TEST_CASE("integer hash", "[integer]") {
  SECTION("equal values hash equal") {
    std::hash<xb::integer> hasher;
    CHECK(hasher(xb::integer(int64_t{42})) == hasher(xb::integer("42")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::integer, int> map;
    map[xb::integer(int64_t{42})] = 1;
    map[xb::integer(int64_t{-99})] = 2;
    CHECK(map.at(xb::integer("42")) == 1);
    CHECK(map.at(xb::integer("-99")) == 2);
  }
}

TEST_CASE("integer compound assignment", "[integer]") {
  xb::integer a(int64_t{10});
  a += xb::integer(int64_t{5});
  CHECK(a == xb::integer(int64_t{15}));

  a -= xb::integer(int64_t{3});
  CHECK(a == xb::integer(int64_t{12}));

  a *= xb::integer(int64_t{2});
  CHECK(a == xb::integer(int64_t{24}));

  a /= xb::integer(int64_t{4});
  CHECK(a == xb::integer(int64_t{6}));

  a %= xb::integer(int64_t{4});
  CHECK(a == xb::integer(int64_t{2}));
}

TEST_CASE("integer string round-trip", "[integer]") {
  auto roundtrip = [](int64_t v) {
    xb::integer original(v);
    xb::integer from_string(original.to_string());
    return from_string == original;
  };

  CHECK(roundtrip(0));
  CHECK(roundtrip(1));
  CHECK(roundtrip(-1));
  CHECK(roundtrip(INT64_MAX));
  CHECK(roundtrip(INT64_MIN));
  CHECK(roundtrip(42));
  CHECK(roundtrip(-42));

  // Large value round-trip.
  std::string big(200, '9');
  xb::integer from_big(big);
  CHECK(xb::integer(from_big.to_string()) == from_big);
}
