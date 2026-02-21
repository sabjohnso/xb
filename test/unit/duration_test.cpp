#include <xb/duration.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <unordered_map>

// Step 9: Default ctor, string parse

TEST_CASE("duration default construction is zero", "[duration]") {
  xb::duration d;
  CHECK(d.is_zero());
  CHECK(d.to_string() == "PT0S");
  CHECK_FALSE(d.is_negative());
  CHECK(d.years() == 0);
  CHECK(d.months() == 0);
  CHECK(d.days() == 0);
  CHECK(d.hours() == 0);
  CHECK(d.minutes() == 0);
  CHECK(d.seconds() == 0);
  CHECK(d.nanoseconds() == 0);
}

TEST_CASE("duration string parsing", "[duration]") {
  SECTION("year-month only") {
    xb::duration d("P1Y2M");
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.days() == 0);
    CHECK(d.to_string() == "P1Y2M");
  }
  SECTION("day-time only") {
    xb::duration d("P3DT4H5M6S");
    CHECK(d.years() == 0);
    CHECK(d.months() == 0);
    CHECK(d.days() == 3);
    CHECK(d.hours() == 4);
    CHECK(d.minutes() == 5);
    CHECK(d.seconds() == 6);
    CHECK(d.to_string() == "P3DT4H5M6S");
  }
  SECTION("full mixed form") {
    xb::duration d("P1Y2M3DT4H5M6S");
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.days() == 3);
    CHECK(d.hours() == 4);
    CHECK(d.minutes() == 5);
    CHECK(d.seconds() == 6);
    CHECK(d.to_string() == "P1Y2M3DT4H5M6S");
  }
  SECTION("fractional seconds") {
    xb::duration d("P1Y2M3DT4H5M6.789S");
    CHECK(d.nanoseconds() == 789000000);
    CHECK(d.to_string() == "P1Y2M3DT4H5M6.789S");
  }
  SECTION("negative duration") {
    xb::duration d("-P1Y2M3DT4H");
    CHECK(d.is_negative());
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
    CHECK(d.days() == 3);
    CHECK(d.hours() == 4);
  }
  SECTION("years only") {
    xb::duration d("P1Y");
    CHECK(d.years() == 1);
    CHECK(d.to_string() == "P1Y");
  }
  SECTION("months only") {
    xb::duration d("P2M");
    CHECK(d.months() == 2);
    CHECK(d.to_string() == "P2M");
  }
  SECTION("days only") {
    xb::duration d("P3D");
    CHECK(d.days() == 3);
    CHECK(d.to_string() == "P3D");
  }
  SECTION("time only") {
    xb::duration d("PT1H");
    CHECK(d.hours() == 1);
    CHECK(d.to_string() == "PT1H");
  }
  SECTION("zero forms") {
    CHECK(xb::duration("P0Y").is_zero());
    CHECK(xb::duration("P0D").is_zero());
    CHECK(xb::duration("PT0S").is_zero());
  }
  SECTION("negative zero normalizes") {
    xb::duration d("-P0Y");
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
  }
  SECTION("normalization: months overflow") {
    xb::duration d("P14M");
    CHECK(d.years() == 1);
    CHECK(d.months() == 2);
  }
  SECTION("normalization: time overflow") {
    xb::duration d("PT90M");
    CHECK(d.hours() == 1);
    CHECK(d.minutes() == 30);
  }
}

TEST_CASE("duration invalid string parsing throws", "[duration]") {
  CHECK_THROWS_AS(xb::duration(""), std::invalid_argument);
  CHECK_THROWS_AS(xb::duration("P"), std::invalid_argument);
  CHECK_THROWS_AS(xb::duration("abc"), std::invalid_argument);
  CHECK_THROWS_AS(xb::duration("PT"), std::invalid_argument);
}

// Step 10: Equality, no <=>, negation, is_zero

TEST_CASE("duration equality", "[duration]") {
  CHECK(xb::duration("P1Y2M3DT4H5M6S") == xb::duration("P1Y2M3DT4H5M6S"));
  CHECK_FALSE(xb::duration("P1Y") == xb::duration("P1M"));
  CHECK_FALSE(xb::duration("P1Y") == xb::duration("-P1Y"));
  SECTION("month normalization preserves equality") {
    CHECK(xb::duration("P14M") == xb::duration("P1Y2M"));
  }
  SECTION("time normalization preserves equality") {
    CHECK(xb::duration("PT90M") == xb::duration("PT1H30M"));
  }
}

TEST_CASE("duration negation", "[duration]") {
  SECTION("negate positive") {
    auto d = -xb::duration("P1Y2M3DT4H");
    CHECK(d.is_negative());
    CHECK(d.to_string() == "-P1Y2M3DT4H");
  }
  SECTION("negate negative") {
    auto d = -xb::duration("-P1Y");
    CHECK_FALSE(d.is_negative());
    CHECK(d.to_string() == "P1Y");
  }
  SECTION("negate zero") {
    auto d = -xb::duration();
    CHECK(d.is_zero());
    CHECK_FALSE(d.is_negative());
  }
}

// Step 11: year_month_part/day_time_part, component
//          accessors, hash, stream

TEST_CASE("duration year_month_part", "[duration]") {
  xb::duration d("P1Y2M3DT4H5M6S");
  auto ym = d.year_month_part();
  CHECK(ym.years() == 1);
  CHECK(ym.months() == 2);
  CHECK(ym.total_months() == 14);

  SECTION("negative propagates") {
    xb::duration neg("-P1Y3D");
    auto ym_neg = neg.year_month_part();
    CHECK(ym_neg.is_negative());
    CHECK(ym_neg.total_months() == 12);
  }

  SECTION("zero year_month_part") {
    xb::duration dt("P3DT4H");
    auto ym_zero = dt.year_month_part();
    CHECK(ym_zero.is_zero());
    CHECK_FALSE(ym_zero.is_negative());
  }
}

TEST_CASE("duration day_time_part", "[duration]") {
  xb::duration d("P1Y2M3DT4H5M6.789S");
  auto dt = d.day_time_part();
  CHECK(dt.days() == 3);
  CHECK(dt.hours() == 4);
  CHECK(dt.minutes() == 5);
  CHECK(dt.seconds() == 6);
  CHECK(dt.nanoseconds() == 789000000);

  SECTION("negative propagates") {
    xb::duration neg("-P1Y3DT2H");
    auto dt_neg = neg.day_time_part();
    CHECK(dt_neg.is_negative());
    CHECK(dt_neg.days() == 3);
    CHECK(dt_neg.hours() == 2);
  }

  SECTION("zero day_time_part") {
    xb::duration ym("P1Y2M");
    auto dt_zero = ym.day_time_part();
    CHECK(dt_zero.is_zero());
    CHECK_FALSE(dt_zero.is_negative());
  }
}

TEST_CASE("duration hash", "[duration]") {
  SECTION("equal values hash equal") {
    std::hash<xb::duration> hasher;
    CHECK(hasher(xb::duration("P14M")) == hasher(xb::duration("P1Y2M")));
  }
  SECTION("usable as unordered_map key") {
    std::unordered_map<xb::duration, int> map;
    map[xb::duration("P1Y2M")] = 1;
    map[xb::duration("PT1H")] = 2;
    CHECK(map.at(xb::duration("P14M")) == 1);
  }
}

TEST_CASE("duration stream output", "[duration]") {
  xb::duration d("P1Y2M3DT4H5M6.789S");
  std::ostringstream os;
  os << d;
  CHECK(os.str() == "P1Y2M3DT4H5M6.789S");
}

TEST_CASE("duration string round-trip", "[duration]") {
  auto roundtrip = [](const char* s) {
    return xb::duration(xb::duration(s).to_string()).to_string() ==
           xb::duration(s).to_string();
  };
  CHECK(roundtrip("PT0S"));
  CHECK(roundtrip("P1Y"));
  CHECK(roundtrip("P2M"));
  CHECK(roundtrip("P3D"));
  CHECK(roundtrip("PT4H"));
  CHECK(roundtrip("P1Y2M3DT4H5M6.789S"));
  CHECK(roundtrip("-P1Y2M3DT4H5M6S"));
}
