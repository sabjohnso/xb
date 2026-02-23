#include <xb/xml_value.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace xb;

// ===== bool parse/format =====

// TDD step 1: parse<bool>("true") -> true, format(true) -> "true"
TEST_CASE("parse bool true", "[xml_value]") {
  CHECK(parse<bool>("true") == true);
  CHECK(parse<bool>("false") == false);
  CHECK(format(true) == "true");
  CHECK(format(false) == "false");
}

// TDD step 2: parse<bool>("1") -> true, parse<bool>("0") -> false
TEST_CASE("parse bool numeric", "[xml_value]") {
  CHECK(parse<bool>("1") == true);
  CHECK(parse<bool>("0") == false);
}

// TDD step 3: parse<bool>("invalid") -> throws
TEST_CASE("parse bool invalid throws", "[xml_value]") {
  CHECK_THROWS_AS(parse<bool>("invalid"), std::runtime_error);
  CHECK_THROWS_AS(parse<bool>("TRUE"), std::runtime_error);
  CHECK_THROWS_AS(parse<bool>(""), std::runtime_error);
}

// ===== integer parse/format =====

// TDD step 4: parse<int32_t>("42") -> 42, format(42) -> "42"
TEST_CASE("parse and format int32", "[xml_value]") {
  CHECK(parse<int32_t>("42") == 42);
  CHECK(parse<int32_t>("-100") == -100);
  CHECK(parse<int32_t>("0") == 0);
  CHECK(format(int32_t{42}) == "42");
  CHECK(format(int32_t{-100}) == "-100");
}

// TDD step 5: parse<int32_t> overflow/underflow -> throws
TEST_CASE("parse int32 overflow throws", "[xml_value]") {
  CHECK_THROWS_AS(parse<int32_t>("2147483648"), std::runtime_error);
  CHECK_THROWS_AS(parse<int32_t>("-2147483649"), std::runtime_error);
  CHECK_THROWS_AS(parse<int32_t>("not_a_number"), std::runtime_error);
}

// TDD step 6: All integer types
TEST_CASE("parse and format int8", "[xml_value]") {
  CHECK(parse<int8_t>("127") == 127);
  CHECK(parse<int8_t>("-128") == -128);
  CHECK_THROWS_AS(parse<int8_t>("128"), std::runtime_error);
  CHECK(format(int8_t{42}) == "42");
}

TEST_CASE("parse and format int16", "[xml_value]") {
  CHECK(parse<int16_t>("32767") == 32767);
  CHECK(parse<int16_t>("-32768") == -32768);
  CHECK_THROWS_AS(parse<int16_t>("32768"), std::runtime_error);
  CHECK(format(int16_t{1000}) == "1000");
}

TEST_CASE("parse and format int64", "[xml_value]") {
  CHECK(parse<int64_t>("9223372036854775807") == INT64_MAX);
  CHECK(parse<int64_t>("-9223372036854775808") == INT64_MIN);
  CHECK(format(int64_t{9223372036854775807LL}) == "9223372036854775807");
}

TEST_CASE("parse and format uint8", "[xml_value]") {
  CHECK(parse<uint8_t>("255") == 255);
  CHECK(parse<uint8_t>("0") == 0);
  CHECK_THROWS_AS(parse<uint8_t>("256"), std::runtime_error);
  CHECK_THROWS_AS(parse<uint8_t>("-1"), std::runtime_error);
  CHECK(format(uint8_t{200}) == "200");
}

TEST_CASE("parse and format uint16", "[xml_value]") {
  CHECK(parse<uint16_t>("65535") == 65535);
  CHECK_THROWS_AS(parse<uint16_t>("65536"), std::runtime_error);
  CHECK(format(uint16_t{5000}) == "5000");
}

TEST_CASE("parse and format uint32", "[xml_value]") {
  CHECK(parse<uint32_t>("4294967295") == UINT32_MAX);
  CHECK_THROWS_AS(parse<uint32_t>("4294967296"), std::runtime_error);
  CHECK(format(uint32_t{123456}) == "123456");
}

TEST_CASE("parse and format uint64", "[xml_value]") {
  CHECK(parse<uint64_t>("18446744073709551615") == UINT64_MAX);
  CHECK_THROWS_AS(parse<uint64_t>("-1"), std::runtime_error);
  CHECK(format(uint64_t{18446744073709551615ULL}) == "18446744073709551615");
}

// ===== float/double parse/format =====

// TDD step 7: parse<float/double> round-trip
TEST_CASE("parse and format float", "[xml_value]") {
  CHECK(parse<float>("3.14") == Catch::Approx(3.14f));
  CHECK(parse<double>("3.14159265358979") == Catch::Approx(3.14159265358979));
  // format round-trips
  auto f = format(3.14f);
  CHECK(parse<float>(f) == Catch::Approx(3.14f));
  auto d = format(3.14159265358979);
  CHECK(parse<double>(d) == Catch::Approx(3.14159265358979));
}

// TDD step 8: Float special values
TEST_CASE("parse float special values", "[xml_value]") {
  CHECK(std::isinf(parse<float>("INF")));
  CHECK(parse<float>("INF") > 0);
  CHECK(std::isinf(parse<float>("-INF")));
  CHECK(parse<float>("-INF") < 0);
  CHECK(std::isnan(parse<float>("NaN")));

  CHECK(std::isinf(parse<double>("INF")));
  CHECK(std::isinf(parse<double>("-INF")));
  CHECK(std::isnan(parse<double>("NaN")));

  CHECK(format(std::numeric_limits<float>::infinity()) == "INF");
  CHECK(format(-std::numeric_limits<float>::infinity()) == "-INF");
  CHECK(format(std::numeric_limits<double>::infinity()) == "INF");
  CHECK(format(-std::numeric_limits<double>::infinity()) == "-INF");
}

// ===== string parse/format =====

// TDD step 9: parse<std::string>("hello") -> "hello"
TEST_CASE("parse and format string", "[xml_value]") {
  CHECK(parse<std::string>("hello") == "hello");
  CHECK(parse<std::string>("") == "");
  CHECK(parse<std::string>("  spaces  ") == "  spaces  ");
  CHECK(format(std::string("hello")) == "hello");
}

// ===== whitespace =====

// TDD step 10: apply_whitespace replace mode
TEST_CASE("apply_whitespace replace", "[xml_value]") {
  CHECK(apply_whitespace("hello\tworld\n", whitespace_mode::replace) ==
        "hello world ");
  CHECK(apply_whitespace("a\rb", whitespace_mode::replace) == "a b");
}

// TDD step 11: apply_whitespace collapse mode
TEST_CASE("apply_whitespace collapse", "[xml_value]") {
  CHECK(apply_whitespace("  hello   world  ", whitespace_mode::collapse) ==
        "hello world");
  CHECK(apply_whitespace("\t\n  a  b  \r\n", whitespace_mode::collapse) ==
        "a b");
  CHECK(apply_whitespace("", whitespace_mode::collapse) == "");
}

TEST_CASE("apply_whitespace preserve", "[xml_value]") {
  CHECK(apply_whitespace("  hello  ", whitespace_mode::preserve) ==
        "  hello  ");
}

// ===== xb::integer parse/format =====

// TDD step 12: xb::integer parse/format round-trip
TEST_CASE("parse and format xb::integer", "[xml_value]") {
  auto i = parse<xb::integer>("12345678901234567890");
  CHECK(format(i) == "12345678901234567890");

  auto neg = parse<xb::integer>("-42");
  CHECK(format(neg) == "-42");

  auto zero = parse<xb::integer>("0");
  CHECK(format(zero) == "0");
}

// ===== xb::decimal parse/format =====

// TDD step 13: xb::decimal parse/format round-trip
TEST_CASE("parse and format xb::decimal", "[xml_value]") {
  auto d = parse<xb::decimal>("123.456");
  CHECK(format(d) == "123.456");

  auto neg = parse<xb::decimal>("-0.001");
  CHECK(format(neg) == "-0.001");
}

// ===== date/time types =====

// TDD step 14: Date/time types parse/format round-trips
TEST_CASE("parse and format xb::date", "[xml_value]") {
  auto d = parse<xb::date>("2024-01-15");
  CHECK(format(d) == "2024-01-15");
}

TEST_CASE("parse and format xb::time", "[xml_value]") {
  auto t = parse<xb::time>("13:45:30");
  CHECK(format(t) == "13:45:30");
}

TEST_CASE("parse and format xb::date_time", "[xml_value]") {
  auto dt = parse<xb::date_time>("2024-01-15T13:45:30");
  CHECK(format(dt) == "2024-01-15T13:45:30");
}

TEST_CASE("parse and format xb::duration", "[xml_value]") {
  auto dur = parse<xb::duration>("P1Y2M3DT4H5M6S");
  CHECK(format(dur) == "P1Y2M3DT4H5M6S");
}

TEST_CASE("parse and format xb::year_month_duration", "[xml_value]") {
  auto ymd = parse<xb::year_month_duration>("P1Y6M");
  CHECK(format(ymd) == "P1Y6M");
}

TEST_CASE("parse and format xb::day_time_duration", "[xml_value]") {
  auto dtd = parse<xb::day_time_duration>("P3DT4H");
  CHECK(format(dtd) == "P3DT4H");
}

// ===== hex binary =====

// TDD step 15: Hex binary parse/format
TEST_CASE("parse and format hex binary", "[xml_value]") {
  auto bytes = parse_hex_binary("48656C6C6F");
  REQUIRE(bytes.size() == 5);
  CHECK(static_cast<unsigned char>(bytes[0]) == 0x48); // 'H'
  CHECK(static_cast<unsigned char>(bytes[1]) == 0x65); // 'e'
  CHECK(static_cast<unsigned char>(bytes[2]) == 0x6C); // 'l'
  CHECK(static_cast<unsigned char>(bytes[3]) == 0x6C); // 'l'
  CHECK(static_cast<unsigned char>(bytes[4]) == 0x6F); // 'o'

  CHECK(format_hex_binary(bytes) == "48656C6C6F");
}

TEST_CASE("parse hex binary empty", "[xml_value]") {
  auto bytes = parse_hex_binary("");
  CHECK(bytes.empty());
}

TEST_CASE("parse hex binary lowercase", "[xml_value]") {
  auto bytes = parse_hex_binary("ff00");
  REQUIRE(bytes.size() == 2);
  CHECK(static_cast<unsigned char>(bytes[0]) == 0xFF);
  CHECK(static_cast<unsigned char>(bytes[1]) == 0x00);
}

// ===== base64 binary =====

// TDD step 16: Base64 binary parse/format
TEST_CASE("parse and format base64 binary", "[xml_value]") {
  auto bytes = parse_base64_binary("SGVsbG8=");
  REQUIRE(bytes.size() == 5);
  CHECK(static_cast<unsigned char>(bytes[0]) == 'H');
  CHECK(static_cast<unsigned char>(bytes[1]) == 'e');
  CHECK(static_cast<unsigned char>(bytes[2]) == 'l');
  CHECK(static_cast<unsigned char>(bytes[3]) == 'l');
  CHECK(static_cast<unsigned char>(bytes[4]) == 'o');

  CHECK(format_base64_binary(bytes) == "SGVsbG8=");
}

TEST_CASE("parse base64 binary empty", "[xml_value]") {
  auto bytes = parse_base64_binary("");
  CHECK(bytes.empty());
}

TEST_CASE("parse base64 binary no padding", "[xml_value]") {
  // "Ma" -> base64 "TWE="
  auto bytes = parse_base64_binary("TWE=");
  REQUIRE(bytes.size() == 2);
  CHECK(static_cast<unsigned char>(bytes[0]) == 'M');
  CHECK(static_cast<unsigned char>(bytes[1]) == 'a');
}
