#include <xb/json_value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace xb;

// ===== xb::integer =====

TEST_CASE("json round-trip integer small", "[json_value]") {
  integer val(int64_t{42});
  nlohmann::json j = val;
  CHECK(j.is_number_integer());
  CHECK(j.get<int64_t>() == 42);
  auto rt = j.get<integer>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip integer negative", "[json_value]") {
  integer val(int64_t{-999});
  nlohmann::json j = val;
  CHECK(j.is_number_integer());
  CHECK(j.get<int64_t>() == -999);
  auto rt = j.get<integer>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip integer large as string", "[json_value]") {
  integer val("99999999999999999999999999999");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "99999999999999999999999999999");
  auto rt = j.get<integer>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip integer zero", "[json_value]") {
  integer val(int64_t{0});
  nlohmann::json j = val;
  CHECK(j.is_number_integer());
  CHECK(j.get<int64_t>() == 0);
  auto rt = j.get<integer>();
  CHECK(rt == val);
}

// ===== xb::decimal =====

TEST_CASE("json round-trip decimal", "[json_value]") {
  decimal val("123.456");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "123.456");
  auto rt = j.get<decimal>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip decimal zero", "[json_value]") {
  decimal val("0");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<decimal>();
  CHECK(rt == val);
}

// ===== xb::date =====

TEST_CASE("json round-trip date", "[json_value]") {
  date val("2024-03-15");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "2024-03-15");
  auto rt = j.get<date>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip date with timezone", "[json_value]") {
  date val("2024-03-15+05:30");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<date>();
  CHECK(rt == val);
}

// ===== xb::time =====

TEST_CASE("json round-trip time", "[json_value]") {
  xb::time val("14:30:00");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "14:30:00");
  auto rt = j.get<xb::time>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip time with tz", "[json_value]") {
  xb::time val("14:30:00Z");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<xb::time>();
  CHECK(rt == val);
}

// ===== xb::date_time =====

TEST_CASE("json round-trip date_time", "[json_value]") {
  date_time val("2024-03-15T14:30:00");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "2024-03-15T14:30:00");
  auto rt = j.get<date_time>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip date_time with tz", "[json_value]") {
  date_time val("2024-03-15T14:30:00Z");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<date_time>();
  CHECK(rt == val);
}

// ===== xb::duration =====

TEST_CASE("json round-trip duration", "[json_value]") {
  duration val("P1Y2M3DT4H5M6S");
  nlohmann::json j = val;
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == val.to_string());
  auto rt = j.get<duration>();
  CHECK(rt == val);
}

// ===== xb::year_month_duration =====

TEST_CASE("json round-trip year_month_duration", "[json_value]") {
  year_month_duration val("P1Y6M");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<year_month_duration>();
  CHECK(rt == val);
}

// ===== xb::day_time_duration =====

TEST_CASE("json round-trip day_time_duration", "[json_value]") {
  day_time_duration val("P3DT4H30M");
  nlohmann::json j = val;
  CHECK(j.is_string());
  auto rt = j.get<day_time_duration>();
  CHECK(rt == val);
}

// ===== xb::qname =====

TEST_CASE("json round-trip qname with namespace", "[json_value]") {
  qname val("http://example.com", "localPart");
  nlohmann::json j = val;
  CHECK(j.is_object());
  CHECK(j["namespace"] == "http://example.com");
  CHECK(j["local"] == "localPart");
  auto rt = j.get<qname>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip qname without namespace", "[json_value]") {
  qname val("", "localOnly");
  nlohmann::json j = val;
  CHECK(j.is_object());
  CHECK(j["namespace"] == "");
  CHECK(j["local"] == "localOnly");
  auto rt = j.get<qname>();
  CHECK(rt == val);
}

// ===== xb::any_attribute =====

TEST_CASE("json round-trip any_attribute", "[json_value]") {
  any_attribute val(qname("http://ns.example.com", "attr1"), "value1");
  nlohmann::json j = val;
  CHECK(j.is_object());
  CHECK(j["name"].is_object());
  CHECK(j["value"] == "value1");
  auto rt = j.get<any_attribute>();
  CHECK(rt == val);
}

// ===== std::vector<std::byte> =====

TEST_CASE("json round-trip binary data", "[json_value]") {
  std::vector<std::byte> val = {std::byte{0x48}, std::byte{0x65},
                                std::byte{0x6C}, std::byte{0x6C},
                                std::byte{0x6F}}; // "Hello"
  nlohmann::json j;
  xb::to_json(j, val);
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "SGVsbG8="); // base64("Hello")
  std::vector<std::byte> rt;
  xb::from_json(j, rt);
  CHECK(rt == val);
}

TEST_CASE("json round-trip empty binary", "[json_value]") {
  std::vector<std::byte> val;
  nlohmann::json j;
  xb::to_json(j, val);
  CHECK(j.is_string());
  CHECK(j.get<std::string>() == "");
  std::vector<std::byte> rt;
  xb::from_json(j, rt);
  CHECK(rt == val);
}

// ===== xb::any_element =====

TEST_CASE("json round-trip any_element simple", "[json_value]") {
  any_element val(qname("", "root"), {}, {std::string("text content")});
  nlohmann::json j = val;
  CHECK(j.is_object());
  CHECK(j["tag"].is_object());
  CHECK(j["tag"]["local"] == "root");
  CHECK(j["content"].is_array());
  CHECK(j["content"].size() == 1);
  CHECK(j["content"][0] == "text content");
  auto rt = j.get<any_element>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip any_element with attributes", "[json_value]") {
  any_element val(qname("http://example.com", "item"),
                  {any_attribute(qname("", "id"), "42"),
                   any_attribute(qname("", "name"), "test")},
                  {std::string("content")});
  nlohmann::json j = val;
  CHECK(j["attributes"].size() == 2);
  auto rt = j.get<any_element>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip any_element nested", "[json_value]") {
  any_element child(qname("", "child"), {}, {std::string("inner")});
  any_element val(
      qname("", "parent"), {},
      {std::string("before"), any_element::child(child), std::string("after")});
  nlohmann::json j = val;
  CHECK(j["content"].size() == 3);
  CHECK(j["content"][0] == "before");
  CHECK(j["content"][1].is_object());
  CHECK(j["content"][1]["tag"]["local"] == "child");
  CHECK(j["content"][2] == "after");
  auto rt = j.get<any_element>();
  CHECK(rt == val);
}

TEST_CASE("json round-trip any_element empty", "[json_value]") {
  any_element val(qname("", "empty"), {}, {});
  nlohmann::json j = val;
  CHECK(j["content"].empty());
  CHECK(j["attributes"].empty());
  auto rt = j.get<any_element>();
  CHECK(rt == val);
}
