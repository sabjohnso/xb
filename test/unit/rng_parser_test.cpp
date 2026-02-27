#include <xb/expat_reader.hpp>
#include <xb/rng_parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;
using namespace xb::rng;

static const std::string rng_ns = "http://relaxng.org/ns/structure/1.0";
static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

static pattern
parse_rng(const std::string& xml) {
  expat_reader reader(xml);
  rng_xml_parser parser;
  return parser.parse(reader);
}

// -- empty / text / notAllowed -----------------------------------------------

TEST_CASE("rng_parser: empty pattern", "[rng_parser]") {
  auto p = parse_rng(R"(<empty xmlns=")" + rng_ns + R"("/>)");
  CHECK(p.holds<empty_pattern>());
}

TEST_CASE("rng_parser: text pattern", "[rng_parser]") {
  auto p = parse_rng(R"(<text xmlns=")" + rng_ns + R"("/>)");
  CHECK(p.holds<text_pattern>());
}

TEST_CASE("rng_parser: notAllowed pattern", "[rng_parser]") {
  auto p = parse_rng(R"(<notAllowed xmlns=")" + rng_ns + R"("/>)");
  CHECK(p.holds<not_allowed_pattern>());
}

// -- element / attribute with name attribute ----------------------------------

TEST_CASE("rng_parser: element with name attribute", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element name="card" xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  REQUIRE(elem.name.holds<specific_name>());
  CHECK(elem.name.get<specific_name>().local_name == "card");
  REQUIRE(elem.content != nullptr);
  CHECK(elem.content->holds<text_pattern>());
}

TEST_CASE("rng_parser: attribute with name attribute", "[rng_parser]") {
  auto p = parse_rng(R"(
    <attribute name="type" xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </attribute>
  )");
  REQUIRE(p.holds<attribute_pattern>());
  auto& attr = p.get<attribute_pattern>();
  REQUIRE(attr.name.holds<specific_name>());
  CHECK(attr.name.get<specific_name>().local_name == "type");
}

TEST_CASE("rng_parser: attribute defaults to text when empty", "[rng_parser]") {
  auto p = parse_rng(R"(
    <attribute name="type" xmlns=")" +
                     rng_ns + R"("/>
  )");
  REQUIRE(p.holds<attribute_pattern>());
  auto& attr = p.get<attribute_pattern>();
  REQUIRE(attr.content != nullptr);
  CHECK(attr.content->holds<text_pattern>());
}

// -- element with ns attribute ------------------------------------------------

TEST_CASE("rng_parser: element with ns attribute", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element name="card" ns="urn:test" xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  REQUIRE(elem.name.holds<specific_name>());
  CHECK(elem.name.get<specific_name>().ns == "urn:test");
  CHECK(elem.name.get<specific_name>().local_name == "card");
}

// -- group / interleave / choice ----------------------------------------------

TEST_CASE("rng_parser: group", "[rng_parser]") {
  auto p = parse_rng(R"(
    <group xmlns=")" +
                     rng_ns + R"(">
      <text/>
      <empty/>
    </group>
  )");
  REQUIRE(p.holds<group_pattern>());
  CHECK(p.get<group_pattern>().left->holds<text_pattern>());
  CHECK(p.get<group_pattern>().right->holds<empty_pattern>());
}

TEST_CASE("rng_parser: interleave", "[rng_parser]") {
  auto p = parse_rng(R"(
    <interleave xmlns=")" +
                     rng_ns + R"(">
      <text/>
      <empty/>
    </interleave>
  )");
  REQUIRE(p.holds<interleave_pattern>());
}

TEST_CASE("rng_parser: choice", "[rng_parser]") {
  auto p = parse_rng(R"(
    <choice xmlns=")" +
                     rng_ns + R"(">
      <text/>
      <empty/>
    </choice>
  )");
  REQUIRE(p.holds<choice_pattern>());
}

TEST_CASE("rng_parser: group with three children folds right", "[rng_parser]") {
  auto p = parse_rng(R"(
    <group xmlns=")" +
                     rng_ns + R"(">
      <text/>
      <empty/>
      <notAllowed/>
    </group>
  )");
  // group(text, group(empty, notAllowed))
  REQUIRE(p.holds<group_pattern>());
  CHECK(p.get<group_pattern>().left->holds<text_pattern>());
  REQUIRE(p.get<group_pattern>().right->holds<group_pattern>());
  auto& inner = p.get<group_pattern>().right->get<group_pattern>();
  CHECK(inner.left->holds<empty_pattern>());
  CHECK(inner.right->holds<not_allowed_pattern>());
}

// -- optional / zeroOrMore / oneOrMore / mixed --------------------------------

TEST_CASE("rng_parser: optional", "[rng_parser]") {
  auto p = parse_rng(R"(
    <optional xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </optional>
  )");
  REQUIRE(p.holds<optional_pattern>());
  CHECK(p.get<optional_pattern>().content->holds<text_pattern>());
}

TEST_CASE("rng_parser: zeroOrMore", "[rng_parser]") {
  auto p = parse_rng(R"(
    <zeroOrMore xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </zeroOrMore>
  )");
  REQUIRE(p.holds<zero_or_more_pattern>());
}

TEST_CASE("rng_parser: oneOrMore", "[rng_parser]") {
  auto p = parse_rng(R"(
    <oneOrMore xmlns=")" +
                     rng_ns + R"(">
      <text/>
    </oneOrMore>
  )");
  REQUIRE(p.holds<one_or_more_pattern>());
}

TEST_CASE("rng_parser: mixed", "[rng_parser]") {
  auto p = parse_rng(R"(
    <mixed xmlns=")" +
                     rng_ns + R"(">
      <empty/>
    </mixed>
  )");
  REQUIRE(p.holds<mixed_pattern>());
}

// -- ref / parentRef ----------------------------------------------------------

TEST_CASE("rng_parser: ref", "[rng_parser]") {
  auto p = parse_rng(R"(
    <ref name="cardContent" xmlns=")" +
                     rng_ns + R"("/>
  )");
  REQUIRE(p.holds<ref_pattern>());
  CHECK(p.get<ref_pattern>().name == "cardContent");
}

TEST_CASE("rng_parser: parentRef", "[rng_parser]") {
  auto p = parse_rng(R"(
    <parentRef name="outer" xmlns=")" +
                     rng_ns + R"("/>
  )");
  REQUIRE(p.holds<parent_ref_pattern>());
  CHECK(p.get<parent_ref_pattern>().name == "outer");
}

// -- data / value / param / list ----------------------------------------------

TEST_CASE("rng_parser: data with type", "[rng_parser]") {
  auto p = parse_rng(R"(
    <data type="integer" datatypeLibrary=")" +
                     xsd_dt + R"(" xmlns=")" + rng_ns + R"("/>
  )");
  REQUIRE(p.holds<data_pattern>());
  CHECK(p.get<data_pattern>().type == "integer");
  CHECK(p.get<data_pattern>().datatype_library == xsd_dt);
}

TEST_CASE("rng_parser: data with params", "[rng_parser]") {
  auto p = parse_rng(R"(
    <data type="string" datatypeLibrary=")" +
                     xsd_dt + R"(" xmlns=")" + rng_ns + R"(">
      <param name="minLength">1</param>
      <param name="maxLength">100</param>
    </data>
  )");
  REQUIRE(p.holds<data_pattern>());
  auto& d = p.get<data_pattern>();
  REQUIRE(d.params.size() == 2);
  CHECK(d.params[0].name == "minLength");
  CHECK(d.params[0].value == "1");
  CHECK(d.params[1].name == "maxLength");
  CHECK(d.params[1].value == "100");
}

TEST_CASE("rng_parser: data with except", "[rng_parser]") {
  auto p = parse_rng(R"(
    <data type="token" datatypeLibrary=")" +
                     xsd_dt + R"(" xmlns=")" + rng_ns + R"(">
      <except>
        <value>forbidden</value>
      </except>
    </data>
  )");
  REQUIRE(p.holds<data_pattern>());
  REQUIRE(p.get<data_pattern>().except != nullptr);
}

TEST_CASE("rng_parser: value", "[rng_parser]") {
  auto p = parse_rng(R"(
    <value type="token" datatypeLibrary=")" +
                     xsd_dt + R"(" xmlns=")" + rng_ns + R"(">personal</value>
  )");
  REQUIRE(p.holds<value_pattern>());
  CHECK(p.get<value_pattern>().value == "personal");
  CHECK(p.get<value_pattern>().type == "token");
}

TEST_CASE("rng_parser: value with default type token", "[rng_parser]") {
  auto p = parse_rng(R"(
    <value xmlns=")" +
                     rng_ns + R"(">hello</value>
  )");
  REQUIRE(p.holds<value_pattern>());
  CHECK(p.get<value_pattern>().value == "hello");
  CHECK(p.get<value_pattern>().type == "token");
}

TEST_CASE("rng_parser: list", "[rng_parser]") {
  auto p = parse_rng(R"(
    <list xmlns=")" + rng_ns +
                     R"(">
      <oneOrMore>
        <data type="double" datatypeLibrary=")" +
                     xsd_dt + R"("/>
      </oneOrMore>
    </list>
  )");
  REQUIRE(p.holds<list_pattern>());
}

// -- name classes: anyName, nsName, choice ------------------------------------

TEST_CASE("rng_parser: element with anyName", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element xmlns=")" +
                     rng_ns + R"(">
      <anyName/>
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  CHECK(p.get<element_pattern>().name.holds<any_name_nc>());
}

TEST_CASE("rng_parser: element with nsName", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element xmlns=")" +
                     rng_ns + R"(" ns="urn:test">
      <nsName/>
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  REQUIRE(elem.name.holds<ns_name_nc>());
  CHECK(elem.name.get<ns_name_nc>().ns == "urn:test");
}

TEST_CASE("rng_parser: anyName with except", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element xmlns=")" +
                     rng_ns + R"(">
      <anyName>
        <except>
          <name>forbidden</name>
        </except>
      </anyName>
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& nc = p.get<element_pattern>().name;
  REQUIRE(nc.holds<any_name_nc>());
  REQUIRE(nc.get<any_name_nc>().except != nullptr);
}

// -- externalRef --------------------------------------------------------------

TEST_CASE("rng_parser: externalRef", "[rng_parser]") {
  auto p = parse_rng(R"(
    <externalRef href="other.rng" xmlns=")" +
                     rng_ns + R"("/>
  )");
  REQUIRE(p.holds<external_ref_pattern>());
  CHECK(p.get<external_ref_pattern>().href == "other.rng");
}

// -- grammar ------------------------------------------------------------------

TEST_CASE("rng_parser: grammar with start and define", "[rng_parser]") {
  auto p = parse_rng(R"(
    <grammar xmlns=")" +
                     rng_ns + R"(">
      <start>
        <ref name="doc"/>
      </start>
      <define name="doc">
        <element name="doc">
          <text/>
        </element>
      </define>
    </grammar>
  )");
  REQUIRE(p.holds<grammar_pattern>());
  auto& g = p.get<grammar_pattern>();
  REQUIRE(g.start != nullptr);
  CHECK(g.start->holds<ref_pattern>());
  REQUIRE(g.defines.size() == 1);
  CHECK(g.defines[0].name == "doc");
}

TEST_CASE("rng_parser: define with combine", "[rng_parser]") {
  auto p = parse_rng(R"(
    <grammar xmlns=")" +
                     rng_ns + R"(">
      <start>
        <ref name="inline"/>
      </start>
      <define name="inline">
        <text/>
      </define>
      <define name="inline" combine="choice">
        <element name="code"><text/></element>
      </define>
    </grammar>
  )");
  REQUIRE(p.holds<grammar_pattern>());
  auto& g = p.get<grammar_pattern>();
  CHECK(g.defines.size() == 2);
  CHECK(g.defines[1].combine == combine_method::choice);
}

TEST_CASE("rng_parser: include", "[rng_parser]") {
  auto p = parse_rng(R"(
    <grammar xmlns=")" +
                     rng_ns + R"(">
      <start>
        <ref name="doc"/>
      </start>
      <include href="base.rng">
        <define name="inline">
          <text/>
        </define>
      </include>
    </grammar>
  )");
  REQUIRE(p.holds<grammar_pattern>());
  auto& g = p.get<grammar_pattern>();
  REQUIRE(g.includes.size() == 1);
  CHECK(g.includes[0].href == "base.rng");
  REQUIRE(g.includes[0].overrides.size() == 1);
  CHECK(g.includes[0].overrides[0].name == "inline");
}

// -- implicit group (multiple children in element) ----------------------------

TEST_CASE("rng_parser: element with implicit group", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element name="card" xmlns=")" +
                     rng_ns + R"(">
      <element name="name"><text/></element>
      <element name="email"><text/></element>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  // Multiple children without explicit group -> implicit group
  REQUIRE(elem.content->holds<group_pattern>());
}

// -- datatypeLibrary inheritance ----------------------------------------------

TEST_CASE("rng_parser: datatypeLibrary inherited from ancestor",
          "[rng_parser]") {
  auto p = parse_rng(R"(
    <element name="qty" datatypeLibrary=")" +
                     xsd_dt + R"(" xmlns=")" + rng_ns + R"(">
      <data type="integer"/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  auto& content = *p.get<element_pattern>().content;
  REQUIRE(content.holds<data_pattern>());
  CHECK(content.get<data_pattern>().datatype_library == xsd_dt);
}

// -- annotation skipping ------------------------------------------------------

TEST_CASE("rng_parser: annotations are skipped", "[rng_parser]") {
  auto p = parse_rng(R"(
    <element name="card" xmlns=")" +
                     rng_ns + R"("
             xmlns:a="http://relaxng.org/ns/compatibility/annotations/1.0">
      <a:documentation>A card element</a:documentation>
      <text/>
    </element>
  )");
  REQUIRE(p.holds<element_pattern>());
  CHECK(p.get<element_pattern>().content->holds<text_pattern>());
}

// -- div ----------------------------------------------------------------------

TEST_CASE("rng_parser: div in grammar", "[rng_parser]") {
  auto p = parse_rng(R"(
    <grammar xmlns=")" +
                     rng_ns + R"(">
      <start>
        <ref name="doc"/>
      </start>
      <div>
        <define name="doc">
          <element name="doc"><text/></element>
        </define>
      </div>
    </grammar>
  )");
  REQUIRE(p.holds<grammar_pattern>());
  auto& g = p.get<grammar_pattern>();
  CHECK(g.defines.size() == 1);
  CHECK(g.defines[0].name == "doc");
}
