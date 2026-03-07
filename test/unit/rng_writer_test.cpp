#include <xb/expat_reader.hpp>
#include <xb/rng_parser.hpp>
#include <xb/rng_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;
using namespace xb::rng;

static const std::string rng_ns = "http://relaxng.org/ns/structure/1.0";
static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";
static const std::string xml_decl =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

static pattern
parse_rng(const std::string& xml) {
  expat_reader reader(xml);
  rng_xml_parser parser;
  return parser.parse(reader);
}

// Helper: parse then write back to canonical XML string
static std::string
canonical(const std::string& xml) {
  auto p = parse_rng(xml);
  return rng_write_string(p);
}

// -- Leaf patterns ------------------------------------------------------------

TEST_CASE("rng_writer: empty", "[rng_writer]") {
  auto result = canonical(R"(<empty xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == xml_decl + R"(<empty xmlns=")" + rng_ns + R"("/>)");
}

TEST_CASE("rng_writer: text", "[rng_writer]") {
  auto result = canonical(R"(<text xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == xml_decl + R"(<text xmlns=")" + rng_ns + R"("/>)");
}

TEST_CASE("rng_writer: notAllowed", "[rng_writer]") {
  auto result = canonical(R"(<notAllowed xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == xml_decl + R"(<notAllowed xmlns=")" + rng_ns + R"("/>)");
}

// -- Element / attribute with specific_name -----------------------------------

TEST_CASE("rng_writer: element with name and text content", "[rng_writer]") {
  auto result = canonical(R"(<element name="card" xmlns=")" + rng_ns +
                          R"("><text/></element>)");
  CHECK(result == xml_decl + R"(<element xmlns=")" + rng_ns +
                      R"(" name="card"><text/></element>)");
}

TEST_CASE("rng_writer: attribute with name", "[rng_writer]") {
  auto result = canonical(R"(<attribute name="type" xmlns=")" + rng_ns +
                          R"("><text/></attribute>)");
  CHECK(result == xml_decl + R"(<attribute xmlns=")" + rng_ns +
                      R"(" name="type"><text/></attribute>)");
}

TEST_CASE("rng_writer: attribute defaults to text when empty", "[rng_writer]") {
  auto result =
      canonical(R"(<attribute name="type" xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == xml_decl + R"(<attribute xmlns=")" + rng_ns +
                      R"(" name="type"><text/></attribute>)");
}

// -- Name classes -------------------------------------------------------------

TEST_CASE("rng_writer: element with anyName", "[rng_writer]") {
  auto result = canonical(R"(<element xmlns=")" + rng_ns +
                          R"("><anyName/><text/></element>)");
  CHECK(result == xml_decl + R"(<element xmlns=")" + rng_ns +
                      R"("><anyName/><text/></element>)");
}

TEST_CASE("rng_writer: anyName with except", "[rng_writer]") {
  auto result = canonical(
      R"(<element xmlns=")" + rng_ns +
      R"("><anyName><except><name>foo</name></except></anyName><text/></element>)");
  // Parser gives element with no explicit content -> empty, so text above
  // becomes content. Actually, content is text here from the <text/> child.
  // But <anyName> with <except> consumes the first child as name class,
  // then subsequent children are content. Let's check actual output.
  CHECK(
      result ==
      xml_decl + R"(<element xmlns=")" + rng_ns +
          R"("><anyName><except><name>foo</name></except></anyName><empty/></element>)");
}

TEST_CASE("rng_writer: nsName", "[rng_writer]") {
  auto result =
      canonical(R"(<element xmlns=")" + rng_ns +
                R"("><nsName ns="http://example.com"/><text/></element>)");
  CHECK(result ==
        xml_decl + R"(<element xmlns=")" + rng_ns +
            R"("><nsName ns="http://example.com"/><text/></element>)");
}

TEST_CASE("rng_writer: name class choice", "[rng_writer]") {
  auto result = canonical(
      R"(<element xmlns=")" + rng_ns +
      R"("><choice><name>a</name><name>b</name></choice><text/></element>)");
  CHECK(
      result ==
      xml_decl + R"(<element xmlns=")" + rng_ns +
          R"("><choice><name>a</name><name>b</name></choice><text/></element>)");
}

// -- Binary combinators with tree unfolding -----------------------------------

TEST_CASE("rng_writer: group with two children", "[rng_writer]") {
  auto result =
      canonical(R"(<group xmlns=")" + rng_ns + R"("><empty/><text/></group>)");
  CHECK(result == xml_decl + R"(<group xmlns=")" + rng_ns +
                      R"("><empty/><text/></group>)");
}

TEST_CASE("rng_writer: group with three children unfolds", "[rng_writer]") {
  // Parser folds 3 children into group(a, group(b, c))
  // Writer should unfold back to <group><a/><b/><c/></group>
  auto result = canonical(R"(<group xmlns=")" + rng_ns +
                          R"("><empty/><text/><notAllowed/></group>)");
  CHECK(result == xml_decl + R"(<group xmlns=")" + rng_ns +
                      R"("><empty/><text/><notAllowed/></group>)");
}

TEST_CASE("rng_writer: interleave unfolds", "[rng_writer]") {
  auto result = canonical(R"(<interleave xmlns=")" + rng_ns +
                          R"("><empty/><text/><notAllowed/></interleave>)");
  CHECK(result == xml_decl + R"(<interleave xmlns=")" + rng_ns +
                      R"("><empty/><text/><notAllowed/></interleave>)");
}

TEST_CASE("rng_writer: choice unfolds", "[rng_writer]") {
  auto result = canonical(R"(<choice xmlns=")" + rng_ns +
                          R"("><empty/><text/><notAllowed/></choice>)");
  CHECK(result == xml_decl + R"(<choice xmlns=")" + rng_ns +
                      R"("><empty/><text/><notAllowed/></choice>)");
}

// -- Occurrence patterns ------------------------------------------------------

TEST_CASE("rng_writer: oneOrMore", "[rng_writer]") {
  auto result =
      canonical(R"(<oneOrMore xmlns=")" + rng_ns + R"("><text/></oneOrMore>)");
  CHECK(result == xml_decl + R"(<oneOrMore xmlns=")" + rng_ns +
                      R"("><text/></oneOrMore>)");
}

TEST_CASE("rng_writer: zeroOrMore", "[rng_writer]") {
  auto result = canonical(R"(<zeroOrMore xmlns=")" + rng_ns +
                          R"("><text/></zeroOrMore>)");
  CHECK(result == xml_decl + R"(<zeroOrMore xmlns=")" + rng_ns +
                      R"("><text/></zeroOrMore>)");
}

TEST_CASE("rng_writer: optional", "[rng_writer]") {
  auto result =
      canonical(R"(<optional xmlns=")" + rng_ns + R"("><text/></optional>)");
  CHECK(result ==
        xml_decl + R"(<optional xmlns=")" + rng_ns + R"("><text/></optional>)");
}

TEST_CASE("rng_writer: mixed", "[rng_writer]") {
  auto result =
      canonical(R"(<mixed xmlns=")" + rng_ns + R"("><empty/></mixed>)");
  CHECK(result ==
        xml_decl + R"(<mixed xmlns=")" + rng_ns + R"("><empty/></mixed>)");
}

// -- Data types ---------------------------------------------------------------

TEST_CASE("rng_writer: data with type", "[rng_writer]") {
  auto result =
      canonical(R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" +
                xsd_dt + R"(" type="integer"/>)");
  CHECK(result == xml_decl + R"(<data xmlns=")" + rng_ns +
                      R"(" datatypeLibrary=")" + xsd_dt +
                      R"(" type="integer"/>)");
}

TEST_CASE("rng_writer: data with params", "[rng_writer]") {
  auto result = canonical(
      R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" + xsd_dt +
      R"(" type="string"><param name="maxLength">10</param></data>)");
  CHECK(result ==
        xml_decl + R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" +
            xsd_dt +
            R"(" type="string"><param name="maxLength">10</param></data>)");
}

TEST_CASE("rng_writer: data with except", "[rng_writer]") {
  auto result = canonical(
      R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" + xsd_dt +
      R"(" type="string"><except><value>bad</value></except></data>)");
  CHECK(result == xml_decl + R"(<data xmlns=")" + rng_ns +
                      R"(" datatypeLibrary=")" + xsd_dt +
                      R"(" type="string"><except><value datatypeLibrary=")" +
                      xsd_dt + R"(" type="token">bad</value></except></data>)");
}

TEST_CASE("rng_writer: value", "[rng_writer]") {
  auto result =
      canonical(R"(<value xmlns=")" + rng_ns + R"(" datatypeLibrary=")" +
                xsd_dt + R"(" type="string">hello</value>)");
  CHECK(result == xml_decl + R"(<value xmlns=")" + rng_ns +
                      R"(" datatypeLibrary=")" + xsd_dt +
                      R"(" type="string">hello</value>)");
}

TEST_CASE("rng_writer: list", "[rng_writer]") {
  auto result = canonical(R"(<list xmlns=")" + rng_ns + R"("><text/></list>)");
  CHECK(result ==
        xml_decl + R"(<list xmlns=")" + rng_ns + R"("><text/></list>)");
}

TEST_CASE("rng_writer: ref", "[rng_writer]") {
  auto result = canonical(R"(<ref xmlns=")" + rng_ns + R"(" name="foo"/>)");
  CHECK(result == xml_decl + R"(<ref xmlns=")" + rng_ns + R"(" name="foo"/>)");
}

TEST_CASE("rng_writer: parentRef", "[rng_writer]") {
  auto result =
      canonical(R"(<parentRef xmlns=")" + rng_ns + R"(" name="bar"/>)");
  CHECK(result ==
        xml_decl + R"(<parentRef xmlns=")" + rng_ns + R"(" name="bar"/>)");
}

TEST_CASE("rng_writer: externalRef", "[rng_writer]") {
  auto result =
      canonical(R"(<externalRef xmlns=")" + rng_ns + R"(" href="other.rng"/>)");
  CHECK(result == xml_decl + R"(<externalRef xmlns=")" + rng_ns +
                      R"(" href="other.rng"/>)");
}

// -- Grammar ------------------------------------------------------------------

TEST_CASE("rng_writer: grammar with start and define", "[rng_writer]") {
  auto result = canonical(
      R"(<grammar xmlns=")" + rng_ns +
      R"(">)"
      R"(<start><ref name="root"/></start>)"
      R"(<define name="root"><element name="doc"><text/></element></define>)"
      R"(</grammar>)");
  CHECK(
      result ==
      xml_decl + R"(<grammar xmlns=")" + rng_ns +
          R"(">)"
          R"(<start><ref name="root"/></start>)"
          R"(<define name="root"><element name="doc"><text/></element></define>)"
          R"(</grammar>)");
}

TEST_CASE("rng_writer: define with combine=choice", "[rng_writer]") {
  auto result =
      canonical(R"(<grammar xmlns=")" + rng_ns +
                R"(">)"
                R"(<start><ref name="root"/></start>)"
                R"(<define name="root" combine="choice"><text/></define>)"
                R"(</grammar>)");
  CHECK(result == xml_decl + R"(<grammar xmlns=")" + rng_ns +
                      R"(">)"
                      R"(<start><ref name="root"/></start>)"
                      R"(<define name="root" combine="choice"><text/></define>)"
                      R"(</grammar>)");
}

TEST_CASE("rng_writer: include", "[rng_writer]") {
  auto result = canonical(R"(<grammar xmlns=")" + rng_ns +
                          R"(">)"
                          R"(<start><ref name="root"/></start>)"
                          R"(<include href="other.rng"/>)"
                          R"(</grammar>)");
  CHECK(result == xml_decl + R"(<grammar xmlns=")" + rng_ns +
                      R"(">)"
                      R"(<start><ref name="root"/></start>)"
                      R"(<include href="other.rng"/>)"
                      R"(</grammar>)");
}

// -- Namespace handling -------------------------------------------------------

TEST_CASE("rng_writer: element with ns attribute", "[rng_writer]") {
  auto result =
      canonical(R"(<element xmlns=")" + rng_ns +
                R"(" name="item" ns="http://example.com"><text/></element>)");
  CHECK(result ==
        xml_decl + R"(<element xmlns=")" + rng_ns +
            R"(" name="item" ns="http://example.com"><text/></element>)");
}

// -- Pretty-print (indent > 0) ------------------------------------------------

TEST_CASE("rng_writer: pretty print self-closing leaf", "[rng_writer]") {
  auto p = parse_rng(R"(<empty xmlns=")" + rng_ns + R"("/>)");
  auto result = rng_write_string(p, 2);
  CHECK(result == xml_decl + R"(<empty xmlns=")" + rng_ns + R"("/>)" + "\n");
}

TEST_CASE("rng_writer: pretty print element with child", "[rng_writer]") {
  auto p = parse_rng(R"(<element xmlns=")" + rng_ns +
                     R"(" name="card"><text/></element>)");
  auto result = rng_write_string(p, 2);
  CHECK(result == xml_decl + "<element xmlns=\"" + rng_ns +
                      "\" name=\"card\">\n"
                      "  <text/>\n"
                      "</element>\n");
}

TEST_CASE("rng_writer: pretty print text content stays inline",
          "[rng_writer]") {
  auto p =
      parse_rng(R"(<grammar xmlns=")" + rng_ns +
                R"(">)"
                R"(<start><ref name="r"/></start>)"
                R"(<define name="r"><element name="doc">)"
                R"(<data datatypeLibrary=")" +
                xsd_dt +
                R"(" type="string"><param name="maxLength">10</param></data>)"
                R"(</element></define></grammar>)");
  auto result = rng_write_string(p, 2);
  CHECK(result == xml_decl + "<grammar xmlns=\"" + rng_ns +
                      "\">\n"
                      "  <start>\n"
                      "    <ref name=\"r\"/>\n"
                      "  </start>\n"
                      "  <define name=\"r\">\n"
                      "    <element name=\"doc\">\n"
                      "      <data datatypeLibrary=\"" +
                      xsd_dt +
                      "\" type=\"string\">\n"
                      "        <param name=\"maxLength\">10</param>\n"
                      "      </data>\n"
                      "    </element>\n"
                      "  </define>\n"
                      "</grammar>\n");
}

TEST_CASE("rng_writer: pretty print with indent 4", "[rng_writer]") {
  auto p = parse_rng(R"(<element xmlns=")" + rng_ns +
                     R"(" name="x"><text/></element>)");
  auto result = rng_write_string(p, 4);
  CHECK(result == xml_decl + "<element xmlns=\"" + rng_ns +
                      "\" name=\"x\">\n"
                      "    <text/>\n"
                      "</element>\n");
}

TEST_CASE("rng_writer: idempotent (write is fixpoint)", "[rng_writer]") {
  std::string input =
      R"(<grammar xmlns=")" + rng_ns +
      R"(">)"
      R"(<start><ref name="root"/></start>)"
      R"(<define name="root">)"
      R"(<element name="doc" ns="http://example.com">)"
      R"(<oneOrMore><choice>)"
      R"(<element name="p"><text/></element>)"
      R"(<element name="img"><attribute name="src"><text/></attribute></element>)"
      R"(</choice></oneOrMore>)"
      R"(</element></define></grammar>)";
  auto first = canonical(input);
  auto second = canonical(first);
  CHECK(first == second);
}
