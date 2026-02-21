#include <xb/time.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <unordered_map>

// Step 15: Default ctor, string parse

TEST_CASE("time default construction", "[time]") {
  xb::time t;
  CHECK(t.hour() == 0);
  CHECK(t.minute() == 0);
  CHECK(t.second() == 0);
  CHECK(t.nanosecond() == 0);
  CHECK_FALSE(t.has_timezone());
  CHECK(t.to_string() == "00:00:00");
}

TEST_CASE("time string parsing", "[time]") {
  SECTION("basic time") {
    xb::time t("13:20:00");
    CHECK(t.hour() == 13);
    CHECK(t.minute() == 20);
    CHECK(t.second() == 0);
    CHECK(t.to_string() == "13:20:00");
  }
  SECTION("with fractional seconds") {
    xb::time t("13:20:30.5");
    CHECK(t.hour() == 13);
    CHECK(t.minute() == 20);
    CHECK(t.second() == 30);
    CHECK(t.nanosecond() == 500000000);
    CHECK(t.to_string() == "13:20:30.5");
  }
  SECTION("full nanosecond precision") {
    xb::time t("12:00:00.123456789");
    CHECK(t.nanosecond() == 123456789);
    CHECK(t.to_string() == "12:00:00.123456789");
  }
  SECTION("with UTC timezone") {
    xb::time t("13:20:00Z");
    CHECK(t.has_timezone());
    CHECK(t.tz_offset_minutes() == 0);
    CHECK(t.to_string() == "13:20:00Z");
  }
  SECTION("with positive timezone") {
    xb::time t("13:20:00+05:30");
    CHECK(t.has_timezone());
    CHECK(t.tz_offset_minutes() == 330);
    CHECK(t.to_string() == "13:20:00+05:30");
  }
  SECTION("with negative timezone") {
    xb::time t("13:20:00-05:00");
    CHECK(t.has_timezone());
    CHECK(t.tz_offset_minutes() == -300);
    CHECK(t.to_string() == "13:20:00-05:00");
  }
  SECTION("fractional seconds and timezone") {
    xb::time t("13:20:30.5Z");
    CHECK(t.nanosecond() == 500000000);
    CHECK(t.has_timezone());
  }
  SECTION("trailing zeros stripped in fractional") {
    xb::time t("12:00:00.100");
    CHECK(t.to_string() == "12:00:00.1");
  }
}

// Step 16: 24:00:00 canonicalization, invalid strings

TEST_CASE("time 24:00:00 canonicalization", "[time]") {
  SECTION("24:00:00 becomes 00:00:00") {
    xb::time t("24:00:00");
    CHECK(t.hour() == 0);
    CHECK(t.minute() == 0);
    CHECK(t.second() == 0);
    CHECK(t.to_string() == "00:00:00");
  }
  SECTION("24:00:00Z preserved timezone") {
    xb::time t("24:00:00Z");
    CHECK(t.hour() == 0);
    CHECK(t.has_timezone());
    CHECK(t.to_string() == "00:00:00Z");
  }
}

TEST_CASE("time invalid strings", "[time]") {
  CHECK_THROWS_AS(xb::time(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("25:00:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("12:60:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("12:00:60"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("24:00:01"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("24:01:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("1:00:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::time("12:0:00"), std::invalid_argument);
}

// Step 17: Equality (with UTC normalization), hash, stream

TEST_CASE("time equality", "[time]") {
  SECTION("same time, no timezone") {
    CHECK(xb::time("13:20:00") == xb::time("13:20:00"));
  }
  SECTION("different times") {
    CHECK_FALSE(xb::time("13:20:00") == xb::time("13:21:00"));
  }
  SECTION("both have timezone: UTC normalize") {
    CHECK(xb::time("13:00:00Z") == xb::time("08:00:00-05:00"));
  }
  SECTION("neither has timezone: field compare") {
    CHECK(xb::time("13:00:00") == xb::time("13:00:00"));
  }
  SECTION("mixed timezone: not equal") {
    CHECK_FALSE(xb::time("13:00:00Z") == xb::time("13:00:00"));
  }
  SECTION("fractional seconds affect equality") {
    CHECK_FALSE(xb::time("12:00:00.1") == xb::time("12:00:00.2"));
    CHECK(xb::time("12:00:00.100") == xb::time("12:00:00.1"));
  }
}

TEST_CASE("time hash", "[time]") {
  SECTION("equal values hash equal") {
    std::hash<xb::time> hasher;
    CHECK(hasher(xb::time("13:20:00")) == hasher(xb::time("13:20:00")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::time, int> map;
    map[xb::time("13:20:00")] = 1;
    map[xb::time("14:00:00")] = 2;
    CHECK(map.at(xb::time("13:20:00")) == 1);
  }
}

TEST_CASE("time stream output", "[time]") {
  xb::time t("13:20:30.5+05:30");
  std::ostringstream os;
  os << t;
  CHECK(os.str() == "13:20:30.5+05:30");
}

TEST_CASE("time string round-trip", "[time]") {
  auto roundtrip = [](const char* s) {
    return xb::time(xb::time(s).to_string()).to_string() ==
           xb::time(s).to_string();
  };
  CHECK(roundtrip("00:00:00"));
  CHECK(roundtrip("13:20:00"));
  CHECK(roundtrip("13:20:30.5"));
  CHECK(roundtrip("13:20:00Z"));
  CHECK(roundtrip("13:20:00+05:30"));
  CHECK(roundtrip("13:20:00-05:00"));
  CHECK(roundtrip("23:59:59.999999999"));
}
