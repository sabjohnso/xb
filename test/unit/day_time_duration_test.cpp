#include <xb/day_time_duration.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <compare>
#include <sstream>
#include <unordered_map>

// Step 5: Default ctor, string parse, to_string

TEST_CASE("day_time_duration default construction is zero",
          "[day_time_duration]") {
  xb::day_time_duration d;
  CHECK(d.is_zero());
  CHECK(d.to_string() == "PT0S");
  CHECK_FALSE(d.is_negative());
  CHECK(d.days() == 0);
  CHECK(d.hours() == 0);
  CHECK(d.minutes() == 0);
  CHECK(d.seconds() == 0);
  CHECK(d.nanoseconds() == 0);
}

TEST_CASE("day_time_duration string parsing", "[day_time_duration]") {
  SECTION("days only") {
    xb::day_time_duration d("P1D");
    CHECK(d.days() == 1);
    CHECK(d.hours() == 0);
    CHECK(d.to_string() == "P1D");
  }
  SECTION("hours only") {
    xb::day_time_duration d("PT1H");
    CHECK(d.hours() == 1);
    CHECK(d.to_string() == "PT1H");
  }
  SECTION("minutes only") {
    xb::day_time_duration d("PT30M");
    CHECK(d.minutes() == 30);
    CHECK(d.to_string() == "PT30M");
  }
  SECTION("seconds only") {
    xb::day_time_duration d("PT45S");
    CHECK(d.seconds() == 45);
    CHECK(d.to_string() == "PT45S");
  }
  SECTION("hours and minutes") {
    xb::day_time_duration d("PT1H30M");
    CHECK(d.hours() == 1);
    CHECK(d.minutes() == 30);
    CHECK(d.to_string() == "PT1H30M");
  }
  SECTION("days hours minutes seconds") {
    xb::day_time_duration d("P1DT2H3M4S");
    CHECK(d.days() == 1);
    CHECK(d.hours() == 2);
    CHECK(d.minutes() == 3);
    CHECK(d.seconds() == 4);
    CHECK(d.to_string() == "P1DT2H3M4S");
  }
  SECTION("normalization: 90 minutes becomes 1H30M") {
    xb::day_time_duration d("PT90M");
    CHECK(d.hours() == 1);
    CHECK(d.minutes() == 30);
    CHECK(d.to_string() == "PT1H30M");
  }
  SECTION("normalization: 25 hours becomes 1D1H") {
    xb::day_time_duration d("PT25H");
    CHECK(d.days() == 1);
    CHECK(d.hours() == 1);
    CHECK(d.to_string() == "P1DT1H");
  }
  SECTION("negative duration") {
    xb::day_time_duration d("-P1DT2H");
    CHECK(d.is_negative());
    CHECK(d.days() == 1);
    CHECK(d.hours() == 2);
    CHECK(d.to_string() == "-P1DT2H");
  }
  SECTION("zero forms") {
    CHECK(xb::day_time_duration("P0D").is_zero());
    CHECK(xb::day_time_duration("PT0S").is_zero());
    CHECK(xb::day_time_duration("PT0H0M0S").is_zero());
  }
  SECTION("negative zero normalizes to positive zero") {
    xb::day_time_duration d("-PT0S");
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
    CHECK(d.to_string() == "PT0S");
  }
}

// Step 6: Fractional seconds, invalid strings

TEST_CASE("day_time_duration fractional seconds", "[day_time_duration]") {
  SECTION("PT1.5S") {
    xb::day_time_duration d("PT1.5S");
    CHECK(d.seconds() == 1);
    CHECK(d.nanoseconds() == 500000000);
    CHECK(d.to_string() == "PT1.5S");
  }
  SECTION("PT1.123456789S — full nanosecond precision") {
    xb::day_time_duration d("PT1.123456789S");
    CHECK(d.seconds() == 1);
    CHECK(d.nanoseconds() == 123456789);
    CHECK(d.to_string() == "PT1.123456789S");
  }
  SECTION("trailing zeros stripped: PT1.100S → PT1.1S") {
    xb::day_time_duration d("PT1.100S");
    CHECK(d.nanoseconds() == 100000000);
    CHECK(d.to_string() == "PT1.1S");
  }
  SECTION("more than 9 fractional digits truncated") {
    xb::day_time_duration d("PT1.1234567891S");
    CHECK(d.nanoseconds() == 123456789);
  }
  SECTION("PT0.001S") {
    xb::day_time_duration d("PT0.001S");
    CHECK(d.seconds() == 0);
    CHECK(d.nanoseconds() == 1000000);
    CHECK(d.to_string() == "PT0.001S");
  }
}

TEST_CASE("day_time_duration invalid string parsing throws",
          "[day_time_duration]") {
  CHECK_THROWS_AS(xb::day_time_duration(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("P"), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("PT"), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("P1Y"), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("P1M"), std::invalid_argument);
  CHECK_THROWS_AS(xb::day_time_duration("P1Y2M3DT4H"), std::invalid_argument);
}

// Step 7: Equality, comparison, negation, is_zero

TEST_CASE("day_time_duration equality", "[day_time_duration]") {
  CHECK(xb::day_time_duration("PT60M") == xb::day_time_duration("PT1H"));
  CHECK(xb::day_time_duration("P1D") == xb::day_time_duration("PT24H"));
  CHECK_FALSE(xb::day_time_duration("PT1H") == xb::day_time_duration("PT2H"));
  CHECK_FALSE(xb::day_time_duration("PT1H") == xb::day_time_duration("-PT1H"));
}

TEST_CASE("day_time_duration comparison", "[day_time_duration]") {
  SECTION("positive values") {
    CHECK(xb::day_time_duration("PT1H") < xb::day_time_duration("PT2H"));
    CHECK(xb::day_time_duration("P1D") > xb::day_time_duration("PT23H"));
  }
  SECTION("fractional seconds comparison") {
    CHECK(xb::day_time_duration("PT1.5S") > xb::day_time_duration("PT1.4S"));
    CHECK(xb::day_time_duration("PT1.5S") < xb::day_time_duration("PT1.6S"));
  }
  SECTION("negative values") {
    CHECK(xb::day_time_duration("-PT2H") < xb::day_time_duration("-PT1H"));
  }
  SECTION("strong_ordering") {
    auto cmp = xb::day_time_duration("PT1H") <=> xb::day_time_duration("PT2H");
    CHECK(cmp == std::strong_ordering::less);
  }
}

TEST_CASE("day_time_duration negation", "[day_time_duration]") {
  SECTION("negate positive") {
    auto d = -xb::day_time_duration("P1DT2H");
    CHECK(d.is_negative());
    CHECK(d.to_string() == "-P1DT2H");
  }
  SECTION("negate negative") {
    auto d = -xb::day_time_duration("-P1DT2H");
    CHECK_FALSE(d.is_negative());
    CHECK(d.to_string() == "P1DT2H");
  }
  SECTION("negate zero") {
    auto d = -xb::day_time_duration();
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
  }
}

// Step 8: Arithmetic, compound assignment, hash, stream, chrono

TEST_CASE("day_time_duration addition", "[day_time_duration]") {
  SECTION("basic addition") {
    auto result =
        xb::day_time_duration("PT1H") + xb::day_time_duration("PT30M");
    CHECK(result == xb::day_time_duration("PT1H30M"));
  }
  SECTION("addition with normalization") {
    auto result =
        xb::day_time_duration("PT23H") + xb::day_time_duration("PT2H");
    CHECK(result == xb::day_time_duration("P1DT1H"));
  }
  SECTION("addition of fractional seconds") {
    auto result =
        xb::day_time_duration("PT0.5S") + xb::day_time_duration("PT0.7S");
    CHECK(result == xb::day_time_duration("PT1.2S"));
  }
  SECTION("nanosecond carry") {
    auto result = xb::day_time_duration("PT0.999999999S") +
                  xb::day_time_duration("PT0.000000001S");
    CHECK(result == xb::day_time_duration("PT1S"));
  }
}

TEST_CASE("day_time_duration subtraction", "[day_time_duration]") {
  SECTION("basic") {
    auto result =
        xb::day_time_duration("PT2H") - xb::day_time_duration("PT30M");
    CHECK(result == xb::day_time_duration("PT1H30M"));
  }
  SECTION("resulting in negative") {
    auto result = xb::day_time_duration("PT1H") - xb::day_time_duration("PT2H");
    CHECK(result == xb::day_time_duration("-PT1H"));
  }
}

TEST_CASE("day_time_duration multiplication", "[day_time_duration]") {
  SECTION("multiply by scalar") {
    auto result = xb::day_time_duration("PT30M") * int64_t{3};
    CHECK(result == xb::day_time_duration("PT1H30M"));
  }
  SECTION("scalar on left") {
    auto result = int64_t{3} * xb::day_time_duration("PT30M");
    CHECK(result == xb::day_time_duration("PT1H30M"));
  }
  SECTION("multiply by zero") {
    auto result = xb::day_time_duration("P1D") * int64_t{0};
    CHECK(result.is_zero());
  }
}

TEST_CASE("day_time_duration compound assignment", "[day_time_duration]") {
  xb::day_time_duration d("PT1H");
  d += xb::day_time_duration("PT30M");
  CHECK(d == xb::day_time_duration("PT1H30M"));

  d -= xb::day_time_duration("PT15M");
  CHECK(d == xb::day_time_duration("PT1H15M"));

  d *= int64_t{2};
  CHECK(d == xb::day_time_duration("PT2H30M"));
}

TEST_CASE("day_time_duration hash", "[day_time_duration]") {
  SECTION("equal values hash equal") {
    std::hash<xb::day_time_duration> hasher;
    CHECK(hasher(xb::day_time_duration("PT60M")) ==
          hasher(xb::day_time_duration("PT1H")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::day_time_duration, int> map;
    map[xb::day_time_duration("PT1H")] = 1;
    map[xb::day_time_duration("PT30M")] = 2;
    CHECK(map.at(xb::day_time_duration("PT60M")) == 1);
  }
}

TEST_CASE("day_time_duration stream output", "[day_time_duration]") {
  xb::day_time_duration d("P1DT2H3M4.5S");
  std::ostringstream os;
  os << d;
  CHECK(os.str() == "P1DT2H3M4.5S");
}

TEST_CASE("day_time_duration chrono interop", "[day_time_duration]") {
  SECTION("to chrono::nanoseconds") {
    xb::day_time_duration d("PT1H30M");
    auto ns = static_cast<std::chrono::nanoseconds>(d);
    CHECK(ns.count() == 5400000000000LL);
  }
  SECTION("negative to chrono::nanoseconds") {
    xb::day_time_duration d("-PT1S");
    auto ns = static_cast<std::chrono::nanoseconds>(d);
    CHECK(ns.count() == -1000000000LL);
  }
  SECTION("from chrono::nanoseconds") {
    xb::day_time_duration d(std::chrono::nanoseconds{5400000000000LL});
    CHECK(d == xb::day_time_duration("PT1H30M"));
  }
  SECTION("from negative chrono::nanoseconds") {
    xb::day_time_duration d(std::chrono::nanoseconds{-1500000000LL});
    CHECK(d == xb::day_time_duration("-PT1.5S"));
  }
  SECTION("fractional seconds via chrono") {
    xb::day_time_duration d(std::chrono::nanoseconds{1123456789LL});
    CHECK(d.seconds() == 1);
    CHECK(d.nanoseconds() == 123456789);
  }
}

TEST_CASE("day_time_duration string round-trip", "[day_time_duration]") {
  auto roundtrip = [](const char* s) {
    return xb::day_time_duration(xb::day_time_duration(s).to_string())
               .to_string() == xb::day_time_duration(s).to_string();
  };
  CHECK(roundtrip("PT0S"));
  CHECK(roundtrip("P1D"));
  CHECK(roundtrip("PT1H"));
  CHECK(roundtrip("PT30M"));
  CHECK(roundtrip("PT1.5S"));
  CHECK(roundtrip("P1DT2H3M4.123456789S"));
  CHECK(roundtrip("-P2DT12H"));
}
