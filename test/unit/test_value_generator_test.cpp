#include <xb/test_value_generator.hpp>

#include <xb/facet_set.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

  const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
  const std::string test_ns = "http://example.com/test";

  xb::schema_set
  make_schema_set(xb::schema s) {
    xb::schema_set ss;
    ss.add(std::move(s));
    ss.resolve();
    return ss;
  }

  bool
  has_value(const xb::test_value_set& vs, const std::string& xml_text) {
    return std::any_of(
        vs.values.begin(), vs.values.end(),
        [&](const xb::test_value& v) { return v.xml_text == xml_text; });
  }

  bool
  has_reason(const xb::test_value_set& vs, const std::string& reason) {
    return std::any_of(
        vs.values.begin(), vs.values.end(),
        [&](const xb::test_value& v) { return v.reason == reason; });
  }

} // namespace

// ---------------------------------------------------------------------------
// Built-in bounded integer types
// ---------------------------------------------------------------------------

TEST_CASE("xs:byte produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "byte"});

  CHECK(has_value(vs, "-128")); // min
  CHECK(has_value(vs, "-127")); // min+1
  CHECK(has_value(vs, "0"));    // zero
  CHECK(has_value(vs, "1"));    // one
  CHECK(has_value(vs, "126"));  // max-1
  CHECK(has_value(vs, "127"));  // max
  CHECK(vs.values.size() >= 6);
}

TEST_CASE("xs:unsignedByte produces boundary values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "unsignedByte"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "254"));
  CHECK(has_value(vs, "255"));
  CHECK(vs.values.size() >= 4);
}

TEST_CASE("xs:short produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "short"});

  CHECK(has_value(vs, "-32768"));
  CHECK(has_value(vs, "32767"));
  CHECK(has_value(vs, "0"));
}

TEST_CASE("xs:unsignedShort produces boundary values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "unsignedShort"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "65535"));
}

TEST_CASE("xs:int produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "int"});

  CHECK(has_value(vs, "-2147483648"));
  CHECK(has_value(vs, "2147483647"));
  CHECK(has_value(vs, "0"));
}

TEST_CASE("xs:unsignedInt produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "unsignedInt"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "4294967295"));
}

TEST_CASE("xs:long produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "long"});

  CHECK(has_value(vs, "-9223372036854775808"));
  CHECK(has_value(vs, "9223372036854775807"));
  CHECK(has_value(vs, "0"));
}

TEST_CASE("xs:unsignedLong produces boundary values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "unsignedLong"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "18446744073709551615"));
}

// ---------------------------------------------------------------------------
// Unbounded integer types
// ---------------------------------------------------------------------------

TEST_CASE("xs:integer produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "integer"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "-1"));
}

TEST_CASE("xs:positiveInteger produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "positiveInteger"});

  CHECK(has_value(vs, "1"));
  CHECK(!has_value(vs, "0"));
  CHECK(!has_value(vs, "-1"));
}

TEST_CASE("xs:nonNegativeInteger produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "nonNegativeInteger"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(!has_value(vs, "-1"));
}

TEST_CASE("xs:negativeInteger produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "negativeInteger"});

  CHECK(has_value(vs, "-1"));
  CHECK(!has_value(vs, "0"));
}

TEST_CASE("xs:nonPositiveInteger produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "nonPositiveInteger"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "-1"));
  CHECK(!has_value(vs, "1"));
}

// ---------------------------------------------------------------------------
// Float / double
// ---------------------------------------------------------------------------

TEST_CASE("xs:float produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "float"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "-1"));
}

TEST_CASE("xs:double produces boundary values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "double"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "-1"));
}

// ---------------------------------------------------------------------------
// Boolean
// ---------------------------------------------------------------------------

TEST_CASE("xs:boolean produces both values", "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "boolean"});

  REQUIRE(vs.values.size() == 2);
  CHECK(has_value(vs, "true"));
  CHECK(has_value(vs, "false"));
}

// ---------------------------------------------------------------------------
// String
// ---------------------------------------------------------------------------

TEST_CASE("xs:string produces representative values",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "string"});

  CHECK(has_value(vs, ""));     // empty
  CHECK(has_value(vs, "test")); // typical
  CHECK(vs.values.size() >= 2);
}

// ---------------------------------------------------------------------------
// User-defined types with facets
// ---------------------------------------------------------------------------

TEST_CASE("Enumeration facet: all values produced", "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.enumeration = {"red", "green", "blue"};

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Color"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "string"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "Color"});

  REQUIRE(vs.values.size() == 3);
  CHECK(has_value(vs, "red"));
  CHECK(has_value(vs, "green"));
  CHECK(has_value(vs, "blue"));
}

TEST_CASE("min_inclusive + max_inclusive facets produce boundary values",
          "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.min_inclusive = "10";
  facets.max_inclusive = "100";

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Score"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "integer"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "Score"});

  CHECK(has_value(vs, "10"));  // min_inclusive
  CHECK(has_value(vs, "11"));  // min+1
  CHECK(has_value(vs, "99"));  // max-1
  CHECK(has_value(vs, "100")); // max_inclusive
}

TEST_CASE("min_exclusive + max_exclusive facets produce boundary values",
          "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.min_exclusive = "0";
  facets.max_exclusive = "10";

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Positive1to9"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "integer"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "Positive1to9"});

  CHECK(has_value(vs, "1"));   // min_exclusive + 1
  CHECK(has_value(vs, "2"));   // min+2
  CHECK(has_value(vs, "8"));   // max-2
  CHECK(has_value(vs, "9"));   // max_exclusive - 1
  CHECK(!has_value(vs, "0"));  // excluded
  CHECK(!has_value(vs, "10")); // excluded
}

TEST_CASE("String with length facet", "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.length = 5;

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Code5"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "string"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "Code5"});

  REQUIRE(!vs.values.empty());
  CHECK(vs.values.front().xml_text.size() == 5);
}

TEST_CASE("String with minLength + maxLength facets",
          "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.min_length = 2;
  facets.max_length = 8;

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Name"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "string"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "Name"});

  // Should have values at min_length, min_length+1, max_length-1, max_length
  bool has_len2 =
      std::any_of(vs.values.begin(), vs.values.end(),
                  [](const auto& v) { return v.xml_text.size() == 2; });
  bool has_len8 =
      std::any_of(vs.values.begin(), vs.values.end(),
                  [](const auto& v) { return v.xml_text.size() == 8; });
  CHECK(has_len2);
  CHECK(has_len8);
}

TEST_CASE("total_digits facet produces boundary values",
          "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.total_digits = 3;

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "SmallNum"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "integer"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "SmallNum"});

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "999"));  // all-9s
  CHECK(has_value(vs, "-999")); // all-9s negative
}

TEST_CASE("User-defined type inherits base type boundaries",
          "[test_value_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Restrict xs:unsignedByte with min_inclusive=10
  xb::facet_set facets;
  facets.min_inclusive = "10";

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "SmallByte"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "unsignedByte"}, facets});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{test_ns, "SmallByte"});

  CHECK(has_value(vs, "10"));  // min from facet
  CHECK(has_value(vs, "11"));  // min+1
  CHECK(has_value(vs, "254")); // max-1 from base type
  CHECK(has_value(vs, "255")); // max from base type
  CHECK(!has_value(vs, "0"));  // below min_inclusive
}

// ---------------------------------------------------------------------------
// Reason metadata
// ---------------------------------------------------------------------------

TEST_CASE("Generated values include reason metadata",
          "[test_value_generator]") {
  xb::schema_set ss;
  ss.resolve();
  xb::test_value_generator gen(ss);

  auto vs = gen.generate(xb::qname{xs_ns, "byte"});

  CHECK(has_reason(vs, "boundary-min"));
  CHECK(has_reason(vs, "boundary-max"));
  CHECK(has_reason(vs, "zero"));
}
