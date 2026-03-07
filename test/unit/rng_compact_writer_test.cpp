#include <xb/expat_reader.hpp>
#include <xb/rng_compact_writer.hpp>
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

// Helper: parse RNG XML then write as compact syntax
static std::string
to_rnc(const std::string& xml) {
  auto p = parse_rng(xml);
  return rng_compact_write(p);
}

static std::string
to_rnc(const std::string& xml, int indent) {
  auto p = parse_rng(xml);
  return rng_compact_write(p, indent);
}

// -- Leaf patterns ------------------------------------------------------------

TEST_CASE("rnc_writer: empty", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<empty xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == "empty\n");
}

TEST_CASE("rnc_writer: text", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<text xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == "text\n");
}

TEST_CASE("rnc_writer: notAllowed", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<notAllowed xmlns=")" + rng_ns + R"("/>)");
  CHECK(result == "notAllowed\n");
}

// -- Element / attribute ------------------------------------------------------

TEST_CASE("rnc_writer: element with text content", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<element name="card" xmlns=")" + rng_ns +
                       R"("><text/></element>)");
  CHECK(result == "element card { text }\n");
}

TEST_CASE("rnc_writer: attribute", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<attribute name="type" xmlns=")" + rng_ns +
                       R"("><text/></attribute>)");
  CHECK(result == "attribute type { text }\n");
}

TEST_CASE("rnc_writer: element with ns", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<element name="item" ns="http://example.com" xmlns=")" +
             rng_ns + R"("><text/></element>)");
  CHECK(result == "default namespace = \"http://example.com\"\n"
                  "\n"
                  "element item { text }\n");
}

// -- Name classes -------------------------------------------------------------

TEST_CASE("rnc_writer: element with anyName", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<element xmlns=")" + rng_ns +
                       R"("><anyName/><empty/></element>)");
  CHECK(result == "element * { empty }\n");
}

TEST_CASE("rnc_writer: nsName", "[rng_compact_writer]") {
  // When nsName's URI is the only namespace, it becomes default
  auto result =
      to_rnc(R"(<element xmlns=")" + rng_ns +
             R"("><nsName ns="http://example.com"/><empty/></element>)");
  CHECK(result == "default namespace = \"http://example.com\"\n"
                  "\n"
                  "element * { empty }\n");
}

TEST_CASE("rnc_writer: anyName with except", "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<element xmlns=")" + rng_ns +
      R"("><anyName><except><name>foo</name></except></anyName><empty/></element>)");
  CHECK(result == "element * - foo { empty }\n");
}

TEST_CASE("rnc_writer: name class choice", "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<element xmlns=")" + rng_ns +
      R"("><choice><name>a</name><name>b</name></choice><text/></element>)");
  CHECK(result == "element (a | b) { text }\n");
}

// -- Binary combinators -------------------------------------------------------

TEST_CASE("rnc_writer: group", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<group xmlns=")" + rng_ns + R"("><empty/><text/></group>)");
  CHECK(result == "empty, text\n");
}

TEST_CASE("rnc_writer: interleave", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<interleave xmlns=")" + rng_ns +
                       R"("><empty/><text/></interleave>)");
  CHECK(result == "empty & text\n");
}

TEST_CASE("rnc_writer: choice pattern", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<choice xmlns=")" + rng_ns + R"("><empty/><text/></choice>)");
  CHECK(result == "empty | text\n");
}

TEST_CASE("rnc_writer: three-way group unfolds", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<group xmlns=")" + rng_ns +
                       R"("><empty/><text/><notAllowed/></group>)");
  CHECK(result == "empty, text, notAllowed\n");
}

// -- Parenthesization ---------------------------------------------------------

TEST_CASE("rnc_writer: nested different combinators get parens",
          "[rng_compact_writer]") {
  // group(choice(a, b), c) => (a | b), c
  auto result =
      to_rnc(R"(<group xmlns=")" + rng_ns +
             R"("><choice><empty/><text/></choice><notAllowed/></group>)");
  CHECK(result == "(empty | text), notAllowed\n");
}

// -- Postfix operators --------------------------------------------------------

TEST_CASE("rnc_writer: oneOrMore", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<oneOrMore xmlns=")" + rng_ns + R"("><text/></oneOrMore>)");
  CHECK(result == "text+\n");
}

TEST_CASE("rnc_writer: zeroOrMore", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<zeroOrMore xmlns=")" + rng_ns + R"("><text/></zeroOrMore>)");
  CHECK(result == "text*\n");
}

TEST_CASE("rnc_writer: optional", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<optional xmlns=")" + rng_ns + R"("><text/></optional>)");
  CHECK(result == "text?\n");
}

TEST_CASE("rnc_writer: mixed", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<mixed xmlns=")" + rng_ns + R"("><empty/></mixed>)");
  CHECK(result == "mixed { empty }\n");
}

// -- Data types ---------------------------------------------------------------

TEST_CASE("rnc_writer: data with type", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" +
                       xsd_dt + R"(" type="integer"/>)");
  CHECK(result == "datatypes xsd = \"" + xsd_dt +
                      "\"\n"
                      "\n"
                      "xsd:integer\n");
}

TEST_CASE("rnc_writer: data with params", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<data xmlns=")" + rng_ns + R"(" datatypeLibrary=")" + xsd_dt +
             R"(" type="string"><param name="maxLength">10</param></data>)");
  CHECK(result == "datatypes xsd = \"" + xsd_dt +
                      "\"\n"
                      "\n"
                      "xsd:string { maxLength = \"10\" }\n");
}

TEST_CASE("rnc_writer: value with datatype", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<value xmlns=")" + rng_ns + R"(" datatypeLibrary=")" +
                       xsd_dt + R"(" type="string">hello</value>)");
  CHECK(result == "datatypes xsd = \"" + xsd_dt +
                      "\"\n"
                      "\n"
                      "xsd:string \"hello\"\n");
}

TEST_CASE("rnc_writer: value with empty dt lib (token)",
          "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<value xmlns=")" + rng_ns + R"(" type="token">abc</value>)");
  CHECK(result == "\"abc\"\n");
}

TEST_CASE("rnc_writer: list", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<list xmlns=")" + rng_ns + R"("><text/></list>)");
  CHECK(result == "list { text }\n");
}

TEST_CASE("rnc_writer: ref", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<ref xmlns=")" + rng_ns + R"(" name="foo"/>)");
  CHECK(result == "foo\n");
}

TEST_CASE("rnc_writer: parentRef", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<parentRef xmlns=")" + rng_ns + R"(" name="bar"/>)");
  CHECK(result == "parent bar\n");
}

TEST_CASE("rnc_writer: externalRef", "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<externalRef xmlns=")" + rng_ns + R"(" href="other.rng"/>)");
  CHECK(result == "external \"other.rng\"\n");
}

// -- Grammar ------------------------------------------------------------------

TEST_CASE("rnc_writer: grammar with start and define", "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<grammar xmlns=")" + rng_ns +
      R"(">)"
      R"(<start><ref name="root"/></start>)"
      R"(<define name="root"><element name="doc"><text/></element></define>)"
      R"(</grammar>)");
  CHECK(result == "start = root\n"
                  "root = element doc { text }\n");
}

TEST_CASE("rnc_writer: define with combine=choice", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<grammar xmlns=")" + rng_ns +
                       R"(">)"
                       R"(<start><ref name="r"/></start>)"
                       R"(<define name="r" combine="choice"><text/></define>)"
                       R"(</grammar>)");
  CHECK(result == "start = r\n"
                  "r |= text\n");
}

TEST_CASE("rnc_writer: define with combine=interleave",
          "[rng_compact_writer]") {
  auto result =
      to_rnc(R"(<grammar xmlns=")" + rng_ns +
             R"(">)"
             R"(<start><ref name="r"/></start>)"
             R"(<define name="r" combine="interleave"><text/></define>)"
             R"(</grammar>)");
  CHECK(result == "start = r\n"
                  "r &= text\n");
}

TEST_CASE("rnc_writer: include", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<grammar xmlns=")" + rng_ns +
                       R"(">)"
                       R"(<start><ref name="root"/></start>)"
                       R"(<include href="other.rng"/>)"
                       R"(</grammar>)");
  CHECK(result == "start = root\n"
                  "include \"other.rng\"\n");
}

// -- Keyword escaping ---------------------------------------------------------

TEST_CASE("rnc_writer: keyword as define name gets escaped",
          "[rng_compact_writer]") {
  auto result = to_rnc(R"(<grammar xmlns=")" + rng_ns +
                       R"(">)"
                       R"(<start><ref name="element"/></start>)"
                       R"(<define name="element"><text/></define>)"
                       R"(</grammar>)");
  CHECK(result == "start = \\element\n"
                  "\\element = text\n");
}

// -- Built-in string/token datatypes ------------------------------------------

TEST_CASE("rnc_writer: builtin string data", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<data xmlns=")" + rng_ns + R"(" type="string"/>)");
  CHECK(result == "string\n");
}

TEST_CASE("rnc_writer: builtin token data", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<data xmlns=")" + rng_ns + R"(" type="token"/>)");
  CHECK(result == "token\n");
}

// -- Pretty-print (indent > 0) ------------------------------------------------

TEST_CASE("rnc_writer: indent element with simple content",
          "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<element name="card" xmlns=")" + rng_ns + R"("><text/></element>)", 2);
  CHECK(result == "element card { text }\n");
}

TEST_CASE("rnc_writer: indent element with complex content",
          "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<element name="card" xmlns=")" + rng_ns +
          R"(">)"
          R"(<group><attribute name="type"><text/></attribute><text/></group>)"
          R"(</element>)",
      2);
  CHECK(result == "element card {\n"
                  "  attribute type { text },\n"
                  "  text\n"
                  "}\n");
}

TEST_CASE("rnc_writer: indent grammar", "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<grammar xmlns=")" + rng_ns +
          R"(">)"
          R"(<start><ref name="root"/></start>)"
          R"(<define name="root">)"
          R"(<element name="doc"><oneOrMore><element name="p"><text/></element></oneOrMore></element>)"
          R"(</define></grammar>)",
      2);
  CHECK(result == "start = root\n"
                  "root =\n"
                  "  element doc {\n"
                  "    element p { text }+\n"
                  "  }\n");
}

TEST_CASE("rnc_writer: indent nested braces", "[rng_compact_writer]") {
  auto result = to_rnc(R"(<element name="doc" xmlns=")" + rng_ns +
                           R"(">)"
                           R"(<oneOrMore><choice>)"
                           R"(<element name="p"><text/></element>)"
                           R"(<element name="em"><text/></element>)"
                           R"(</choice></oneOrMore></element>)",
                       2);
  CHECK(result == "element doc {\n"
                  "  (element p { text } | element em { text })+\n"
                  "}\n");
}

TEST_CASE("rnc_writer: indent choice with leading pipe in define",
          "[rng_compact_writer]") {
  auto result = to_rnc(R"(<grammar xmlns=")" + rng_ns +
                           R"(">)"
                           R"(<start><ref name="Shape"/></start>)"
                           R"(<define name="Shape"><choice>)"
                           R"(<value type="token">rect</value>)"
                           R"(<value type="token">circle</value>)"
                           R"(<value type="token">poly</value>)"
                           R"(<value type="token">default</value>)"
                           R"(</choice></define></grammar>)",
                       2);
  CHECK(result == "start = Shape\n"
                  "Shape =\n"
                  "    \"rect\"\n"
                  "  | \"circle\"\n"
                  "  | \"poly\"\n"
                  "  | \"default\"\n");
}

TEST_CASE("rnc_writer: indent choice in element braces",
          "[rng_compact_writer]") {
  auto result = to_rnc(R"(<element name="x" xmlns=")" + rng_ns +
                           R"(">)"
                           R"(<choice><text/><empty/></choice>)"
                           R"(</element>)",
                       2);
  CHECK(result == "element x {\n"
                  "    text\n"
                  "  | empty\n"
                  "}\n");
}

TEST_CASE("rnc_writer: indent with indent 4", "[rng_compact_writer]") {
  auto result = to_rnc(
      R"(<element name="x" xmlns=")" + rng_ns +
          R"(">)"
          R"(<group><attribute name="a"><text/></attribute><text/></group>)"
          R"(</element>)",
      4);
  CHECK(result == "element x {\n"
                  "    attribute a { text },\n"
                  "    text\n"
                  "}\n");
}
