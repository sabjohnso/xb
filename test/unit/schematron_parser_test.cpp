#include <xb/expat_reader.hpp>
#include <xb/schematron_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace sch = xb::schematron;

static sch::schema
parse_sch(const std::string& xml) {
  expat_reader reader(xml);
  schematron_parser parser;
  return parser.parse(reader);
}

// -- Minimal schema -----------------------------------------------------------

TEST_CASE("schematron parser: minimal schema", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:title>Test Rules</sch:title>
    </sch:schema>
  )");
  CHECK(s.title == "Test Rules");
  CHECK(s.patterns.empty());
}

// -- Namespace bindings -------------------------------------------------------

TEST_CASE("schematron parser: namespace bindings", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:ns prefix="inv" uri="urn:example:invoice"/>
      <sch:ns prefix="addr" uri="urn:example:address"/>
    </sch:schema>
  )");
  REQUIRE(s.namespaces.size() == 2);
  CHECK(s.namespaces[0].prefix == "inv");
  CHECK(s.namespaces[0].uri == "urn:example:invoice");
  CHECK(s.namespaces[1].prefix == "addr");
}

// -- Pattern with rule and assert ---------------------------------------------

TEST_CASE("schematron parser: pattern with assert", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:pattern id="invoice-rules" name="Invoice">
        <sch:rule context="invoice">
          <sch:assert test="total > 0">Total must be positive</sch:assert>
        </sch:rule>
      </sch:pattern>
    </sch:schema>
  )");
  REQUIRE(s.patterns.size() == 1);
  CHECK(s.patterns[0].id == "invoice-rules");
  CHECK(s.patterns[0].name == "Invoice");
  REQUIRE(s.patterns[0].rules.size() == 1);
  CHECK(s.patterns[0].rules[0].context == "invoice");
  REQUIRE(s.patterns[0].rules[0].checks.size() == 1);
  CHECK(s.patterns[0].rules[0].checks[0].is_assert == true);
  CHECK(s.patterns[0].rules[0].checks[0].test == "total > 0");
  CHECK(s.patterns[0].rules[0].checks[0].message == "Total must be positive");
}

// -- Report -------------------------------------------------------------------

TEST_CASE("schematron parser: report element", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:pattern>
        <sch:rule context="order">
          <sch:report test="count(item) > 100">Large order detected</sch:report>
        </sch:rule>
      </sch:pattern>
    </sch:schema>
  )");
  REQUIRE(s.patterns[0].rules[0].checks.size() == 1);
  CHECK(s.patterns[0].rules[0].checks[0].is_assert == false);
  CHECK(s.patterns[0].rules[0].checks[0].test == "count(item) > 100");
  CHECK(s.patterns[0].rules[0].checks[0].message == "Large order detected");
}

// -- Multiple rules and asserts -----------------------------------------------

TEST_CASE("schematron parser: multiple rules", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:pattern id="p1">
        <sch:rule context="invoice">
          <sch:assert test="total > 0">Positive total</sch:assert>
          <sch:assert test="@currency">Currency required</sch:assert>
        </sch:rule>
        <sch:rule context="lineItem">
          <sch:assert test="quantity > 0">Positive qty</sch:assert>
        </sch:rule>
      </sch:pattern>
      <sch:pattern id="p2">
        <sch:rule context="address">
          <sch:assert test="city">City required</sch:assert>
        </sch:rule>
      </sch:pattern>
    </sch:schema>
  )");
  REQUIRE(s.patterns.size() == 2);
  CHECK(s.patterns[0].rules.size() == 2);
  CHECK(s.patterns[0].rules[0].checks.size() == 2);
  CHECK(s.patterns[0].rules[1].checks.size() == 1);
  CHECK(s.patterns[1].rules.size() == 1);
}

// -- Phase --------------------------------------------------------------------

TEST_CASE("schematron parser: phase element", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:phase id="basic">
        <sch:active pattern="p1"/>
        <sch:active pattern="p2"/>
      </sch:phase>
      <sch:pattern id="p1">
        <sch:rule context="x">
          <sch:assert test="y">msg</sch:assert>
        </sch:rule>
      </sch:pattern>
    </sch:schema>
  )");
  REQUIRE(s.phases.size() == 1);
  CHECK(s.phases[0].id == "basic");
  REQUIRE(s.phases[0].active_patterns.size() == 2);
  CHECK(s.phases[0].active_patterns[0] == "p1");
  CHECK(s.phases[0].active_patterns[1] == "p2");
}

// -- Assert with diagnostics reference ----------------------------------------

TEST_CASE("schematron parser: assert with diagnostics", "[schematron_parser]") {
  auto s = parse_sch(R"(
    <sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
      <sch:pattern>
        <sch:rule context="invoice">
          <sch:assert test="total > 0" diagnostics="d1">Positive</sch:assert>
        </sch:rule>
      </sch:pattern>
    </sch:schema>
  )");
  CHECK(s.patterns[0].rules[0].checks[0].diagnostics == "d1");
}
