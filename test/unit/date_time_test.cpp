#include <xb/date_time.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <unordered_map>

// Step 18: Default ctor, string parse

TEST_CASE("date_time default construction", "[date_time]") {
  xb::date_time dt;
  CHECK(dt.year() == 1);
  CHECK(dt.month() == 1);
  CHECK(dt.day() == 1);
  CHECK(dt.hour() == 0);
  CHECK(dt.minute() == 0);
  CHECK(dt.second() == 0);
  CHECK(dt.nanosecond() == 0);
  CHECK_FALSE(dt.has_timezone());
  CHECK(dt.to_string() == "0001-01-01T00:00:00");
}

TEST_CASE("date_time string parsing", "[date_time]") {
  SECTION("basic date_time") {
    xb::date_time dt("2024-01-15T13:20:00");
    CHECK(dt.year() == 2024);
    CHECK(dt.month() == 1);
    CHECK(dt.day() == 15);
    CHECK(dt.hour() == 13);
    CHECK(dt.minute() == 20);
    CHECK(dt.second() == 0);
    CHECK(dt.to_string() == "2024-01-15T13:20:00");
  }
  SECTION("with fractional seconds") {
    xb::date_time dt("2024-01-15T13:20:30.5");
    CHECK(dt.nanosecond() == 500000000);
    CHECK(dt.to_string() == "2024-01-15T13:20:30.5");
  }
  SECTION("full nanosecond precision") {
    xb::date_time dt("2024-01-15T12:00:00.123456789");
    CHECK(dt.nanosecond() == 123456789);
  }
  SECTION("with UTC timezone") {
    xb::date_time dt("2024-01-15T13:20:00Z");
    CHECK(dt.has_timezone());
    CHECK(dt.tz_offset_minutes() == 0);
    CHECK(dt.to_string() == "2024-01-15T13:20:00Z");
  }
  SECTION("with timezone offset") {
    xb::date_time dt("2024-01-15T13:20:00+05:30");
    CHECK(dt.has_timezone());
    CHECK(dt.tz_offset_minutes() == 330);
  }
  SECTION("year 0000") {
    xb::date_time dt("0000-06-15T12:00:00");
    CHECK(dt.year() == 0);
  }
  SECTION("negative year") {
    xb::date_time dt("-0001-01-01T00:00:00");
    CHECK(dt.year() == -1);
  }
}

// Step 19: 24:00:00 rollover, invalid strings

TEST_CASE("date_time 24:00:00 rollover", "[date_time]") {
  SECTION("rolls day forward") {
    xb::date_time dt("2024-01-15T24:00:00");
    CHECK(dt.day() == 16);
    CHECK(dt.hour() == 0);
    CHECK(dt.minute() == 0);
    CHECK(dt.second() == 0);
    CHECK(dt.to_string() == "2024-01-16T00:00:00");
  }
  SECTION("rolls month forward") {
    xb::date_time dt("2024-01-31T24:00:00");
    CHECK(dt.month() == 2);
    CHECK(dt.day() == 1);
    CHECK(dt.hour() == 0);
    CHECK(dt.to_string() == "2024-02-01T00:00:00");
  }
  SECTION("rolls year forward") {
    xb::date_time dt("2024-12-31T24:00:00");
    CHECK(dt.year() == 2025);
    CHECK(dt.month() == 1);
    CHECK(dt.day() == 1);
    CHECK(dt.to_string() == "2025-01-01T00:00:00");
  }
  SECTION("rolls with timezone") {
    xb::date_time dt("2024-01-15T24:00:00Z");
    CHECK(dt.day() == 16);
    CHECK(dt.has_timezone());
  }
}

TEST_CASE("date_time invalid strings", "[date_time]") {
  CHECK_THROWS_AS(xb::date_time(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("2024-01-15"), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("2024-01-15 13:20:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("2024-13-15T13:20:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("2024-01-15T25:00:00"), std::invalid_argument);
  CHECK_THROWS_AS(xb::date_time("2024-01-15T24:00:01"), std::invalid_argument);
}

// Step 20: Equality, date_part/time_part, hash, stream

TEST_CASE("date_time equality", "[date_time]") {
  SECTION("same datetime, no timezone") {
    CHECK(xb::date_time("2024-01-15T13:20:00") ==
          xb::date_time("2024-01-15T13:20:00"));
  }
  SECTION("different datetimes") {
    CHECK_FALSE(xb::date_time("2024-01-15T13:20:00") ==
                xb::date_time("2024-01-15T13:21:00"));
  }
  SECTION("UTC normalization") {
    CHECK(xb::date_time("2024-01-15T12:00:00Z") ==
          xb::date_time("2024-01-15T07:00:00-05:00"));
  }
  SECTION("mixed timezone: not equal") {
    CHECK_FALSE(xb::date_time("2024-01-15T12:00:00Z") ==
                xb::date_time("2024-01-15T12:00:00"));
  }
  SECTION("fractional seconds") {
    CHECK(xb::date_time("2024-01-15T12:00:00.100") ==
          xb::date_time("2024-01-15T12:00:00.1"));
    CHECK_FALSE(xb::date_time("2024-01-15T12:00:00.1") ==
                xb::date_time("2024-01-15T12:00:00.2"));
  }
}

TEST_CASE("date_time date_part", "[date_time]") {
  xb::date_time dt("2024-01-15T13:20:00+05:30");
  auto d = dt.date_part();
  CHECK(d.year() == 2024);
  CHECK(d.month() == 1);
  CHECK(d.day() == 15);
  CHECK(d.has_timezone());
  CHECK(d.tz_offset_minutes() == 330);
}

TEST_CASE("date_time time_part", "[date_time]") {
  xb::date_time dt("2024-01-15T13:20:30.5+05:30");
  auto t = dt.time_part();
  CHECK(t.hour() == 13);
  CHECK(t.minute() == 20);
  CHECK(t.second() == 30);
  CHECK(t.nanosecond() == 500000000);
  CHECK(t.has_timezone());
  CHECK(t.tz_offset_minutes() == 330);
}

TEST_CASE("date_time hash", "[date_time]") {
  SECTION("equal values hash equal") {
    std::hash<xb::date_time> hasher;
    CHECK(hasher(xb::date_time("2024-01-15T13:20:00")) ==
          hasher(xb::date_time("2024-01-15T13:20:00")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::date_time, int> map;
    map[xb::date_time("2024-01-15T13:20:00")] = 1;
    map[xb::date_time("2024-06-15T14:00:00")] = 2;
    CHECK(map.at(xb::date_time("2024-01-15T13:20:00")) == 1);
  }
}

TEST_CASE("date_time stream output", "[date_time]") {
  xb::date_time dt("2024-01-15T13:20:30.5+05:30");
  std::ostringstream os;
  os << dt;
  CHECK(os.str() == "2024-01-15T13:20:30.5+05:30");
}

TEST_CASE("date_time string round-trip", "[date_time]") {
  auto roundtrip = [](const char* s) {
    return xb::date_time(xb::date_time(s).to_string()).to_string() ==
           xb::date_time(s).to_string();
  };
  CHECK(roundtrip("0001-01-01T00:00:00"));
  CHECK(roundtrip("2024-01-15T13:20:00"));
  CHECK(roundtrip("2024-01-15T13:20:30.5"));
  CHECK(roundtrip("2024-01-15T13:20:00Z"));
  CHECK(roundtrip("2024-01-15T13:20:00+05:30"));
  CHECK(roundtrip("2024-01-15T13:20:00-05:00"));
  CHECK(roundtrip("0000-06-15T12:00:00"));
  CHECK(roundtrip("-0001-01-01T00:00:00"));
}
