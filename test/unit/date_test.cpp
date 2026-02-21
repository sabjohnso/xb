#include <xb/date.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <sstream>
#include <unordered_map>

// Step 12: Default ctor, string parse, to_string

TEST_CASE("date default construction", "[date]") {
  xb::date d;
  CHECK(d.year() == 1);
  CHECK(d.month() == 1);
  CHECK(d.day() == 1);
  CHECK_FALSE(d.has_timezone());
  CHECK(d.to_string() == "0001-01-01");
}

TEST_CASE("date string parsing", "[date]") {
  SECTION("basic date") {
    xb::date d("2024-01-15");
    CHECK(d.year() == 2024);
    CHECK(d.month() == 1);
    CHECK(d.day() == 15);
    CHECK_FALSE(d.has_timezone());
    CHECK(d.to_string() == "2024-01-15");
  }
  SECTION("with UTC timezone") {
    xb::date d("2024-01-15Z");
    CHECK(d.year() == 2024);
    CHECK(d.has_timezone());
    CHECK(d.tz_offset_minutes() == 0);
    CHECK(d.to_string() == "2024-01-15Z");
  }
  SECTION("with positive timezone") {
    xb::date d("2024-01-15+05:30");
    CHECK(d.has_timezone());
    CHECK(d.tz_offset_minutes() == 330);
    CHECK(d.to_string() == "2024-01-15+05:30");
  }
  SECTION("with negative timezone") {
    xb::date d("2024-01-15-05:00");
    CHECK(d.has_timezone());
    CHECK(d.tz_offset_minutes() == -300);
    CHECK(d.to_string() == "2024-01-15-05:00");
  }
  SECTION("year 0000 is valid (1 BCE)") {
    xb::date d("0000-06-15");
    CHECK(d.year() == 0);
    CHECK(d.month() == 6);
    CHECK(d.day() == 15);
    CHECK(d.to_string() == "0000-06-15");
  }
  SECTION("negative year") {
    xb::date d("-0001-01-01");
    CHECK(d.year() == -1);
    CHECK(d.to_string() == "-0001-01-01");
  }
  SECTION("five digit year") {
    xb::date d("10000-01-01");
    CHECK(d.year() == 10000);
    CHECK(d.to_string() == "10000-01-01");
  }
  SECTION("year padding: year 1") {
    xb::date d("0001-01-01");
    CHECK(d.year() == 1);
    CHECK(d.to_string() == "0001-01-01");
  }
  SECTION("leap year Feb 29") {
    xb::date d("2024-02-29");
    CHECK(d.month() == 2);
    CHECK(d.day() == 29);
  }
}

// Step 13: Validation

TEST_CASE("date validation", "[date]") {
  SECTION("invalid month") {
    CHECK_THROWS_AS(xb::date("2024-00-15"), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("2024-13-15"), std::invalid_argument);
  }
  SECTION("invalid day") {
    CHECK_THROWS_AS(xb::date("2024-01-00"), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("2024-01-32"), std::invalid_argument);
  }
  SECTION("Feb 29 on non-leap year") {
    CHECK_THROWS_AS(xb::date("2023-02-29"), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("1900-02-29"), std::invalid_argument);
  }
  SECTION("Feb 29 on leap year 2000") { CHECK_NOTHROW(xb::date("2000-02-29")); }
  SECTION("invalid format") {
    CHECK_THROWS_AS(xb::date(""), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("abc"), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("2024-1-15"), std::invalid_argument);
    CHECK_THROWS_AS(xb::date("2024/01/15"), std::invalid_argument);
  }
  SECTION("day range for each month") {
    CHECK_NOTHROW(xb::date("2024-01-31"));
    CHECK_THROWS_AS(xb::date("2024-04-31"), std::invalid_argument);
    CHECK_NOTHROW(xb::date("2024-04-30"));
  }
}

// Step 14: Equality, component ctor, hash, stream, chrono

TEST_CASE("date equality", "[date]") {
  SECTION("same date, no timezone") {
    CHECK(xb::date("2024-01-15") == xb::date("2024-01-15"));
  }
  SECTION("different dates") {
    CHECK_FALSE(xb::date("2024-01-15") == xb::date("2024-01-16"));
  }
  SECTION("both have timezone: UTC normalize") {
    CHECK(xb::date("2024-01-15Z") == xb::date("2024-01-15+00:00"));
  }
  SECTION("neither has timezone: field compare") {
    CHECK(xb::date("2024-01-15") == xb::date("2024-01-15"));
  }
  SECTION("mixed timezone: not equal") {
    CHECK_FALSE(xb::date("2024-01-15Z") == xb::date("2024-01-15"));
  }
}

TEST_CASE("date component constructor", "[date]") {
  SECTION("basic") {
    xb::date d(2024, 6, 15);
    CHECK(d.year() == 2024);
    CHECK(d.month() == 6);
    CHECK(d.day() == 15);
    CHECK_FALSE(d.has_timezone());
  }
  SECTION("with timezone") {
    xb::date d(2024, 6, 15, int16_t{330});
    CHECK(d.has_timezone());
    CHECK(d.tz_offset_minutes() == 330);
  }
  SECTION("invalid values throw") {
    CHECK_THROWS_AS(xb::date(2024, 13, 1), std::invalid_argument);
    CHECK_THROWS_AS(xb::date(2024, 2, 30), std::invalid_argument);
  }
}

TEST_CASE("date hash", "[date]") {
  SECTION("equal values hash equal") {
    std::hash<xb::date> hasher;
    CHECK(hasher(xb::date("2024-01-15")) == hasher(xb::date("2024-01-15")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::date, int> map;
    map[xb::date("2024-01-15")] = 1;
    map[xb::date("2024-06-15")] = 2;
    CHECK(map.at(xb::date("2024-01-15")) == 1);
  }
}

TEST_CASE("date stream output", "[date]") {
  xb::date d("2024-01-15+05:30");
  std::ostringstream os;
  os << d;
  CHECK(os.str() == "2024-01-15+05:30");
}

TEST_CASE("date chrono interop", "[date]") {
  SECTION("to chrono::year_month_day") {
    xb::date d("2024-06-15");
    auto ymd = static_cast<std::chrono::year_month_day>(d);
    CHECK(static_cast<int>(ymd.year()) == 2024);
    CHECK(static_cast<unsigned>(ymd.month()) == 6);
    CHECK(static_cast<unsigned>(ymd.day()) == 15);
  }
  SECTION("from chrono::year_month_day") {
    auto ymd =
        std::chrono::year{2024} / std::chrono::month{6} / std::chrono::day{15};
    xb::date d(ymd);
    CHECK(d.year() == 2024);
    CHECK(d.month() == 6);
    CHECK(d.day() == 15);
    CHECK_FALSE(d.has_timezone());
  }
}

TEST_CASE("date string round-trip", "[date]") {
  auto roundtrip = [](const char* s) {
    return xb::date(xb::date(s).to_string()).to_string() ==
           xb::date(s).to_string();
  };
  CHECK(roundtrip("0001-01-01"));
  CHECK(roundtrip("2024-01-15"));
  CHECK(roundtrip("2024-12-31Z"));
  CHECK(roundtrip("2024-01-15+05:30"));
  CHECK(roundtrip("2024-01-15-05:00"));
  CHECK(roundtrip("0000-06-15"));
  CHECK(roundtrip("-0001-01-01"));
}
