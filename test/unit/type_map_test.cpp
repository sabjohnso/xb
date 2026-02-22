#include <xb/expat_reader.hpp>
#include <xb/type_map.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;

// ---------------------------------------------------------------------------
// defaults and find
// ---------------------------------------------------------------------------

TEST_CASE("type_map defaults has 32 entries", "[type_map]") {
  auto map = type_map::defaults();
  CHECK(map.size() == 32);
}

TEST_CASE("type_map defaults: decimal maps to xb::decimal", "[type_map]") {
  auto map = type_map::defaults();
  auto* m = map.find("decimal");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "xb::decimal");
  CHECK(m->cpp_header == "<xb/decimal.hpp>");
}

TEST_CASE("type_map defaults: long maps to int64_t", "[type_map]") {
  auto map = type_map::defaults();
  auto* m = map.find("long");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "int64_t");
  CHECK(m->cpp_header == "<cstdint>");
}

TEST_CASE("type_map defaults: boolean maps to bool with empty header",
          "[type_map]") {
  auto map = type_map::defaults();
  auto* m = map.find("boolean");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "bool");
  CHECK(m->cpp_header.empty());
}

TEST_CASE("type_map defaults: string types map to std::string", "[type_map]") {
  auto map = type_map::defaults();

  for (const auto& name : {"string", "normalizedString", "token", "anyURI",
                           "ID", "IDREF", "NMTOKEN", "language"}) {
    SECTION(name) {
      auto* m = map.find(name);
      REQUIRE(m != nullptr);
      CHECK(m->cpp_type == "std::string");
      CHECK(m->cpp_header == "<string>");
    }
  }
}

TEST_CASE("type_map defaults: integer family maps to xb::integer",
          "[type_map]") {
  auto map = type_map::defaults();

  for (const auto& name : {"integer", "nonPositiveInteger", "negativeInteger",
                           "nonNegativeInteger", "positiveInteger"}) {
    SECTION(name) {
      auto* m = map.find(name);
      REQUIRE(m != nullptr);
      CHECK(m->cpp_type == "xb::integer");
      CHECK(m->cpp_header == "<xb/integer.hpp>");
    }
  }
}

TEST_CASE("type_map defaults: bounded integer types map to fixed-width",
          "[type_map]") {
  auto map = type_map::defaults();

  SECTION("signed types") {
    CHECK(map.find("long")->cpp_type == "int64_t");
    CHECK(map.find("int")->cpp_type == "int32_t");
    CHECK(map.find("short")->cpp_type == "int16_t");
    CHECK(map.find("byte")->cpp_type == "int8_t");
  }

  SECTION("unsigned types") {
    CHECK(map.find("unsignedLong")->cpp_type == "uint64_t");
    CHECK(map.find("unsignedInt")->cpp_type == "uint32_t");
    CHECK(map.find("unsignedShort")->cpp_type == "uint16_t");
    CHECK(map.find("unsignedByte")->cpp_type == "uint8_t");
  }

  for (const auto& name : {"long", "int", "short", "byte", "unsignedLong",
                           "unsignedInt", "unsignedShort", "unsignedByte"}) {
    SECTION(std::string(name) + " header") {
      CHECK(map.find(name)->cpp_header == "<cstdint>");
    }
  }
}

TEST_CASE("type_map defaults: date/time types", "[type_map]") {
  auto map = type_map::defaults();

  auto* dt = map.find("dateTime");
  REQUIRE(dt != nullptr);
  CHECK(dt->cpp_type == "xb::date_time");
  CHECK(dt->cpp_header == "<xb/date_time.hpp>");

  auto* d = map.find("date");
  REQUIRE(d != nullptr);
  CHECK(d->cpp_type == "xb::date");
  CHECK(d->cpp_header == "<xb/date.hpp>");

  auto* t = map.find("time");
  REQUIRE(t != nullptr);
  CHECK(t->cpp_type == "xb::time");
  CHECK(t->cpp_header == "<xb/time.hpp>");

  auto* dur = map.find("duration");
  REQUIRE(dur != nullptr);
  CHECK(dur->cpp_type == "xb::duration");
  CHECK(dur->cpp_header == "<xb/duration.hpp>");
}

TEST_CASE("type_map defaults: binary types map to vector<byte>", "[type_map]") {
  auto map = type_map::defaults();

  for (const auto& name : {"hexBinary", "base64Binary"}) {
    SECTION(name) {
      auto* m = map.find(name);
      REQUIRE(m != nullptr);
      CHECK(m->cpp_type == "std::vector<std::byte>");
      CHECK(m->cpp_header == "<vector> <cstddef>");
    }
  }
}

TEST_CASE("type_map defaults: QName maps to xb::qname", "[type_map]") {
  auto map = type_map::defaults();
  auto* m = map.find("QName");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "xb::qname");
  CHECK(m->cpp_header == "<xb/qname.hpp>");
}

TEST_CASE("type_map defaults: float and double map to built-ins",
          "[type_map]") {
  auto map = type_map::defaults();

  auto* f = map.find("float");
  REQUIRE(f != nullptr);
  CHECK(f->cpp_type == "float");
  CHECK(f->cpp_header.empty());

  auto* d = map.find("double");
  REQUIRE(d != nullptr);
  CHECK(d->cpp_type == "double");
  CHECK(d->cpp_header.empty());
}

TEST_CASE("type_map find returns nullptr for unknown type", "[type_map]") {
  auto map = type_map::defaults();
  CHECK(map.find("nonexistent") == nullptr);
  CHECK(map.find("") == nullptr);
  CHECK(map.find("xs:string") == nullptr);
}

TEST_CASE("type_map contains", "[type_map]") {
  auto map = type_map::defaults();
  CHECK(map.contains("string"));
  CHECK(map.contains("decimal"));
  CHECK_FALSE(map.contains("nonexistent"));
}

TEST_CASE("type_map set inserts new entry", "[type_map]") {
  type_map map;
  CHECK(map.size() == 0);

  map.set("test", {"my::type", "<my/type.hpp>"});
  CHECK(map.size() == 1);

  auto* m = map.find("test");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "my::type");
  CHECK(m->cpp_header == "<my/type.hpp>");
}

TEST_CASE("type_map set replaces existing entry", "[type_map]") {
  type_map map;
  map.set("test", {"first", "h1"});
  map.set("test", {"second", "h2"});

  CHECK(map.size() == 1);
  CHECK(map.find("test")->cpp_type == "second");
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------

TEST_CASE("type_map load: single mapping", "[type_map]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="decimal"
                  cpp-type="double"
                  cpp-header="&lt;cmath&gt;"/>
    </xb:typemap>
  )";

  expat_reader reader(doc);
  auto map = type_map::load(reader);

  CHECK(map.size() == 1);
  auto* m = map.find("decimal");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "double");
  CHECK(m->cpp_header == "<cmath>");
}

TEST_CASE("type_map load: multiple mappings", "[type_map]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="decimal"
                  cpp-type="double"
                  cpp-header="&lt;cmath&gt;"/>
      <xb:mapping xsd-type="integer"
                  cpp-type="int64_t"
                  cpp-header="&lt;cstdint&gt;"/>
      <xb:mapping xsd-type="dateTime"
                  cpp-type="my::timestamp"
                  cpp-header="&quot;my/timestamp.hpp&quot;"/>
    </xb:typemap>
  )";

  expat_reader reader(doc);
  auto map = type_map::load(reader);

  CHECK(map.size() == 3);

  CHECK(map.find("decimal")->cpp_type == "double");
  CHECK(map.find("decimal")->cpp_header == "<cmath>");

  CHECK(map.find("integer")->cpp_type == "int64_t");
  CHECK(map.find("integer")->cpp_header == "<cstdint>");

  CHECK(map.find("dateTime")->cpp_type == "my::timestamp");
  CHECK(map.find("dateTime")->cpp_header == "\"my/timestamp.hpp\"");
}

TEST_CASE("type_map load: empty typemap", "[type_map]") {
  std::string doc = R"(<xb:typemap xmlns:xb="http://xb.dev/typemap"/>)";

  expat_reader reader(doc);
  auto map = type_map::load(reader);

  CHECK(map.size() == 0);
}

TEST_CASE("type_map load: wrong root element throws", "[type_map]") {
  std::string doc = R"(<wrong xmlns="http://xb.dev/typemap"/>)";

  expat_reader reader(doc);
  CHECK_THROWS_AS(type_map::load(reader), std::runtime_error);
}

TEST_CASE("type_map load: wrong namespace throws", "[type_map]") {
  std::string doc = R"(<typemap xmlns="http://wrong.example.com"/>)";

  expat_reader reader(doc);
  CHECK_THROWS_AS(type_map::load(reader), std::runtime_error);
}

TEST_CASE("type_map load: unknown xsd-type throws", "[type_map]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="unknownType"
                  cpp-type="foo"
                  cpp-header="bar"/>
    </xb:typemap>
  )";

  expat_reader reader(doc);
  CHECK_THROWS_AS(type_map::load(reader), std::runtime_error);
}

// ---------------------------------------------------------------------------
// merge
// ---------------------------------------------------------------------------

TEST_CASE("type_map merge: single override replaces entry", "[type_map]") {
  auto map = type_map::defaults();

  type_map overrides;
  overrides.set("decimal", {"double", "<cmath>"});

  map.merge(overrides);

  auto* m = map.find("decimal");
  REQUIRE(m != nullptr);
  CHECK(m->cpp_type == "double");
  CHECK(m->cpp_header == "<cmath>");

  // Other entries untouched
  CHECK(map.size() == 32);
  CHECK(map.find("integer")->cpp_type == "xb::integer");
  CHECK(map.find("string")->cpp_type == "std::string");
}

TEST_CASE("type_map merge: multiple overrides", "[type_map]") {
  auto map = type_map::defaults();

  type_map overrides;
  overrides.set("decimal", {"double", ""});
  overrides.set("integer", {"int64_t", "<cstdint>"});
  overrides.set("dateTime", {"my::ts", "\"my/ts.hpp\""});

  map.merge(overrides);

  CHECK(map.find("decimal")->cpp_type == "double");
  CHECK(map.find("integer")->cpp_type == "int64_t");
  CHECK(map.find("dateTime")->cpp_type == "my::ts");
  CHECK(map.size() == 32);
}

TEST_CASE("type_map merge: empty overrides is no-op", "[type_map]") {
  auto map = type_map::defaults();
  auto original_size = map.size();

  type_map empty;
  map.merge(empty);

  CHECK(map.size() == original_size);
  CHECK(map.find("decimal")->cpp_type == "xb::decimal");
}

TEST_CASE("type_map merge: unknown xsd-type throws", "[type_map]") {
  auto map = type_map::defaults();

  type_map overrides;
  overrides.set("unknownType", {"foo", "bar"});

  CHECK_THROWS_AS(map.merge(overrides), std::runtime_error);
}

// ---------------------------------------------------------------------------
// end-to-end: defaults -> load -> merge -> find
// ---------------------------------------------------------------------------

TEST_CASE("type_map end-to-end: defaults + load + merge", "[type_map]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="decimal"
                  cpp-type="double"
                  cpp-header=""/>
      <xb:mapping xsd-type="integer"
                  cpp-type="int64_t"
                  cpp-header="&lt;cstdint&gt;"/>
    </xb:typemap>
  )";

  auto map = type_map::defaults();
  CHECK(map.find("decimal")->cpp_type == "xb::decimal");
  CHECK(map.find("integer")->cpp_type == "xb::integer");

  expat_reader reader(doc);
  auto overrides = type_map::load(reader);
  map.merge(overrides);

  // Overridden entries have user values
  CHECK(map.find("decimal")->cpp_type == "double");
  CHECK(map.find("decimal")->cpp_header.empty());
  CHECK(map.find("integer")->cpp_type == "int64_t");
  CHECK(map.find("integer")->cpp_header == "<cstdint>");

  // Non-overridden entries retain defaults
  CHECK(map.find("string")->cpp_type == "std::string");
  CHECK(map.find("boolean")->cpp_type == "bool");
  CHECK(map.find("QName")->cpp_type == "xb::qname");
  CHECK(map.find("dateTime")->cpp_type == "xb::date_time");
  CHECK(map.find("hexBinary")->cpp_type == "std::vector<std::byte>");

  CHECK(map.size() == 32);
}
