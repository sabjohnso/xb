#include <xb/year_month_duration.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <compare>
#include <sstream>
#include <string>
#include <unordered_map>

// Step 1: Default ctor, string parse, to_string

TEST_CASE("year_month_duration default construction is zero",
          "[year_month_duration]") {
  xb::year_month_duration d;
  CHECK(d.is_zero());
  CHECK(d.to_string() == "P0M");
  CHECK_FALSE(d.is_negative());
  CHECK(d.years() == 0);
  CHECK(d.months() == 0);
  CHECK(d.total_months() == 0);
}

TEST_CASE("year_month_duration string parsing", "[year_month_duration]") {
  SECTION("years only") {
    xb::year_month_duration d("P1Y");
    CHECK(d.years() == 1);
    CHECK(d.months() == 0);
    CHECK(d.total_months() == 12);
    CHECK(d.to_string() == "P1Y");
  }
  SECTION("months only") {
    xb::year_month_duration d("P2M");
    CHECK(d.years() == 0);
    CHECK(d.months() == 2);
    CHECK(d.total_months() == 2);
    CHECK(d.to_string() == "P2M");
  }
  SECTION("years and months") {
    xb::year_month_duration d("P1Y2M");
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.total_months() == 14);
    CHECK(d.to_string() == "P1Y2M");
  }
  SECTION("normalization: 14 months becomes 1Y2M") {
    xb::year_month_duration d("P14M");
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.total_months() == 14);
    CHECK(d.to_string() == "P1Y2M");
  }
  SECTION("negative duration") {
    xb::year_month_duration d("-P1Y2M");
    CHECK(d.is_negative());
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.total_months() == 14);
    CHECK(d.to_string() == "-P1Y2M");
  }
  SECTION("zero forms") {
    CHECK(xb::year_month_duration("P0Y").is_zero());
    CHECK(xb::year_month_duration("P0M").is_zero());
    CHECK(xb::year_month_duration("P0Y0M").is_zero());
  }
  SECTION("negative zero normalizes to positive zero") {
    xb::year_month_duration d("-P0M");
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
    CHECK(d.to_string() == "P0M");
  }
  SECTION("large values") {
    xb::year_month_duration d("P999999Y");
    CHECK(d.years() == 999999);
    CHECK(d.total_months() == 999999 * 12);
  }
}

// Step 2: Invalid strings, (years,months) ctor

TEST_CASE("year_month_duration invalid string parsing throws",
          "[year_month_duration]") {
  CHECK_THROWS_AS(xb::year_month_duration(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("P"), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("PT1H"), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("1Y"), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("P1D"), std::invalid_argument);
  CHECK_THROWS_AS(xb::year_month_duration("P1Y2M3D"), std::invalid_argument);
}

TEST_CASE("year_month_duration component constructor",
          "[year_month_duration]") {
  SECTION("basic values") {
    xb::year_month_duration d(2, 6);
    CHECK(d.years() == 2);
    CHECK(d.months() == 6);
    CHECK(d.total_months() == 30);
  }
  SECTION("months overflow normalizes") {
    xb::year_month_duration d(0, 14);
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
  }
  SECTION("zero values") {
    xb::year_month_duration d(0, 0);
    CHECK(d.is_zero());
  }
}

// Step 3: Equality, comparison, negation, is_zero

TEST_CASE("year_month_duration equality", "[year_month_duration]") {
  CHECK(xb::year_month_duration("P1Y") == xb::year_month_duration("P12M"));
  CHECK(xb::year_month_duration("P1Y2M") == xb::year_month_duration(1, 2));
  CHECK_FALSE(xb::year_month_duration("P1Y") == xb::year_month_duration("P1M"));
  CHECK_FALSE(xb::year_month_duration("P1Y") ==
              xb::year_month_duration("-P1Y"));
}

TEST_CASE("year_month_duration comparison", "[year_month_duration]") {
  SECTION("positive values") {
    CHECK(xb::year_month_duration("P1M") < xb::year_month_duration("P2M"));
    CHECK(xb::year_month_duration("P1Y") > xb::year_month_duration("P11M"));
    CHECK(xb::year_month_duration("P1Y") <= xb::year_month_duration("P12M"));
    CHECK(xb::year_month_duration("P1Y") >= xb::year_month_duration("P12M"));
  }
  SECTION("negative values") {
    CHECK(xb::year_month_duration("-P2M") < xb::year_month_duration("-P1M"));
    CHECK(xb::year_month_duration("-P1Y") < xb::year_month_duration("P0M"));
  }
  SECTION("mixed signs") {
    CHECK(xb::year_month_duration("-P1M") < xb::year_month_duration("P1M"));
  }
  SECTION("strong_ordering") {
    auto cmp =
        xb::year_month_duration("P1M") <=> xb::year_month_duration("P2M");
    CHECK(cmp == std::strong_ordering::less);
  }
}

TEST_CASE("year_month_duration negation", "[year_month_duration]") {
  SECTION("negate positive") {
    auto d = -xb::year_month_duration("P1Y2M");
    CHECK(d.is_negative());
    CHECK(d.to_string() == "-P1Y2M");
  }
  SECTION("negate negative") {
    auto d = -xb::year_month_duration("-P1Y2M");
    CHECK_FALSE(d.is_negative());
    CHECK(d.to_string() == "P1Y2M");
  }
  SECTION("negate zero stays positive zero") {
    auto d = -xb::year_month_duration();
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
  }
}

// Step 4: Arithmetic, compound assignment, hash, stream, chrono

TEST_CASE("year_month_duration addition", "[year_month_duration]") {
  SECTION("basic addition") {
    auto result =
        xb::year_month_duration("P1Y") + xb::year_month_duration("P2M");
    CHECK(result == xb::year_month_duration("P1Y2M"));
  }
  SECTION("addition with normalization") {
    auto result =
        xb::year_month_duration("P11M") + xb::year_month_duration("P2M");
    CHECK(result == xb::year_month_duration("P1Y1M"));
  }
  SECTION("addition of negative values") {
    auto result =
        xb::year_month_duration("P2Y") + xb::year_month_duration("-P6M");
    CHECK(result == xb::year_month_duration("P1Y6M"));
  }
  SECTION("addition resulting in zero") {
    auto result =
        xb::year_month_duration("P1Y") + xb::year_month_duration("-P1Y");
    CHECK(result.is_zero());
  }
}

TEST_CASE("year_month_duration subtraction", "[year_month_duration]") {
  SECTION("basic subtraction") {
    auto result =
        xb::year_month_duration("P1Y2M") - xb::year_month_duration("P2M");
    CHECK(result == xb::year_month_duration("P1Y"));
  }
  SECTION("subtraction resulting in negative") {
    auto result =
        xb::year_month_duration("P1M") - xb::year_month_duration("P2M");
    CHECK(result == xb::year_month_duration("-P1M"));
  }
}

TEST_CASE("year_month_duration multiplication", "[year_month_duration]") {
  SECTION("multiply by scalar") {
    auto result = xb::year_month_duration("P3M") * 4;
    CHECK(result == xb::year_month_duration("P1Y"));
  }
  SECTION("scalar on left") {
    auto result = 4 * xb::year_month_duration("P3M");
    CHECK(result == xb::year_month_duration("P1Y"));
  }
  SECTION("multiply by zero") {
    auto result = xb::year_month_duration("P1Y") * 0;
    CHECK(result.is_zero());
  }
  SECTION("multiply negative") {
    auto result = xb::year_month_duration("P1Y") * -1;
    CHECK(result == xb::year_month_duration("-P1Y"));
  }
}

TEST_CASE("year_month_duration compound assignment", "[year_month_duration]") {
  xb::year_month_duration d("P1Y");
  d += xb::year_month_duration("P6M");
  CHECK(d == xb::year_month_duration("P1Y6M"));

  d -= xb::year_month_duration("P3M");
  CHECK(d == xb::year_month_duration("P1Y3M"));

  d *= 2;
  CHECK(d == xb::year_month_duration("P2Y6M"));
}

TEST_CASE("year_month_duration hash", "[year_month_duration]") {
  SECTION("equal values hash equal") {
    std::hash<xb::year_month_duration> hasher;
    CHECK(hasher(xb::year_month_duration("P1Y")) ==
          hasher(xb::year_month_duration("P12M")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::year_month_duration, int> map;
    map[xb::year_month_duration("P1Y")] = 1;
    map[xb::year_month_duration("P6M")] = 2;
    CHECK(map.at(xb::year_month_duration("P12M")) == 1);
    CHECK(map.at(xb::year_month_duration("P6M")) == 2);
  }
}

TEST_CASE("year_month_duration stream output", "[year_month_duration]") {
  xb::year_month_duration d("P1Y2M");
  std::ostringstream os;
  os << d;
  CHECK(os.str() == "P1Y2M");
}

TEST_CASE("year_month_duration chrono interop", "[year_month_duration]") {
  SECTION("to chrono::months") {
    xb::year_month_duration d("P1Y2M");
    auto m = static_cast<std::chrono::months>(d);
    CHECK(m.count() == 14);
  }
  SECTION("negative to chrono::months") {
    xb::year_month_duration d("-P1Y2M");
    auto m = static_cast<std::chrono::months>(d);
    CHECK(m.count() == -14);
  }
  SECTION("from chrono::months") {
    xb::year_month_duration d(std::chrono::months{14});
    CHECK(d == xb::year_month_duration("P1Y2M"));
  }
  SECTION("from negative chrono::months") {
    xb::year_month_duration d(std::chrono::months{-14});
    CHECK(d == xb::year_month_duration("-P1Y2M"));
  }
}

TEST_CASE("year_month_duration string round-trip", "[year_month_duration]") {
  auto roundtrip = [](const char* s) {
    return xb::year_month_duration(xb::year_month_duration(s).to_string())
               .to_string() == xb::year_month_duration(s).to_string();
  };
  CHECK(roundtrip("P0M"));
  CHECK(roundtrip("P1Y"));
  CHECK(roundtrip("P2M"));
  CHECK(roundtrip("P1Y2M"));
  CHECK(roundtrip("-P3Y11M"));
}
