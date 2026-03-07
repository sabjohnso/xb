#include <xb/expat_reader.hpp>
#include <xb/rng_compact_parser.hpp>
#include <xb/rng_compact_writer.hpp>
#include <xb/rng_parser.hpp>
#include <xb/rng_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;
using namespace xb::rng;

static const std::string rng_ns = "http://relaxng.org/ns/structure/1.0";

// -- RNG XML fixpoint: write -> re-parse -> write must be stable --------------

static void
check_rng_fixpoint(const std::string& xml) {
  expat_reader r1(xml);
  rng_xml_parser p1;
  auto first = rng_write_string(p1.parse(r1));

  expat_reader r2(first);
  rng_xml_parser p2;
  auto second = rng_write_string(p2.parse(r2));

  CHECK(first == second);
}

TEST_CASE("roundtrip: RNG XML fixpoint (leaves)", "[rng_roundtrip]") {
  check_rng_fixpoint(R"(<empty xmlns=")" + rng_ns + R"("/>)");
  check_rng_fixpoint(R"(<text xmlns=")" + rng_ns + R"("/>)");
  check_rng_fixpoint(R"(<notAllowed xmlns=")" + rng_ns + R"("/>)");
}

TEST_CASE("roundtrip: RNG XML fixpoint (element)", "[rng_roundtrip]") {
  check_rng_fixpoint(R"(<element name="card" xmlns=")" + rng_ns +
                     R"("><text/></element>)");
}

TEST_CASE("roundtrip: RNG XML fixpoint (combinators)", "[rng_roundtrip]") {
  check_rng_fixpoint(R"(<group xmlns=")" + rng_ns +
                     R"("><empty/><text/><notAllowed/></group>)");
  check_rng_fixpoint(R"(<choice xmlns=")" + rng_ns +
                     R"("><empty/><text/></choice>)");
  check_rng_fixpoint(R"(<interleave xmlns=")" + rng_ns +
                     R"("><empty/><text/></interleave>)");
}

TEST_CASE("roundtrip: RNG XML fixpoint (repetitions)", "[rng_roundtrip]") {
  check_rng_fixpoint(R"(<oneOrMore xmlns=")" + rng_ns +
                     R"("><text/></oneOrMore>)");
  check_rng_fixpoint(R"(<zeroOrMore xmlns=")" + rng_ns +
                     R"("><text/></zeroOrMore>)");
  check_rng_fixpoint(R"(<optional xmlns=")" + rng_ns +
                     R"("><text/></optional>)");
  check_rng_fixpoint(R"(<mixed xmlns=")" + rng_ns + R"("><text/></mixed>)");
}

TEST_CASE("roundtrip: RNG XML fixpoint (grammar)", "[rng_roundtrip]") {
  check_rng_fixpoint(
      R"(<grammar xmlns=")" + rng_ns +
      R"(">)"
      R"(<start><ref name="root"/></start>)"
      R"(<define name="root">)"
      R"(<element name="doc" ns="http://example.com">)"
      R"(<oneOrMore><choice>)"
      R"(<element name="p"><text/></element>)"
      R"(<element name="img"><attribute name="src"><text/></attribute></element>)"
      R"(</choice></oneOrMore>)"
      R"(</element></define></grammar>)");
}

// -- Cross-format: RNG XML -> RNC -> re-parseable ----------------------------
// Verify the compact writer output can be parsed back without error.

static void
check_rng_to_rnc_parseable(const std::string& xml) {
  expat_reader reader(xml);
  rng_xml_parser parser;
  auto p = parser.parse(reader);

  auto rnc = rng_compact_write(p);
  REQUIRE_FALSE(rnc.empty());

  // Must parse without throwing
  rng_compact_parser cp;
  REQUIRE_NOTHROW(cp.parse(rnc));
}

TEST_CASE("roundtrip: RNG XML -> RNC parseable (element)", "[rng_roundtrip]") {
  check_rng_to_rnc_parseable(R"(<element name="card" xmlns=")" + rng_ns +
                             R"("><text/></element>)");
}

TEST_CASE("roundtrip: RNG XML -> RNC parseable (grammar)", "[rng_roundtrip]") {
  check_rng_to_rnc_parseable(R"(<grammar xmlns=")" + rng_ns +
                             R"(">)"
                             R"(<start><ref name="root"/></start>)"
                             R"(<define name="root">)"
                             R"(<element name="doc">)"
                             R"(<oneOrMore><ref name="para"/></oneOrMore>)"
                             R"(</element></define>)"
                             R"(<define name="para">)"
                             R"(<element name="p"><text/></element>)"
                             R"(</define></grammar>)");
}

TEST_CASE("roundtrip: RNG XML -> RNC parseable (choice)", "[rng_roundtrip]") {
  check_rng_to_rnc_parseable(R"(<choice xmlns=")" + rng_ns +
                             R"(">)"
                             R"(<element name="x"><text/></element>)"
                             R"(<element name="y"><empty/></element>)"
                             R"(</choice>)");
}

TEST_CASE("roundtrip: RNG XML -> RNC parseable (interleave)",
          "[rng_roundtrip]") {
  check_rng_to_rnc_parseable(R"(<interleave xmlns=")" + rng_ns +
                             R"(">)"
                             R"(<element name="a"><text/></element>)"
                             R"(<element name="b"><text/></element>)"
                             R"(</interleave>)");
}

// -- Cross-format: RNC -> RNG XML -> re-parseable ----------------------------

static void
check_rnc_to_rng_parseable(const std::string& rnc) {
  rng_compact_parser cp;
  auto p = cp.parse(rnc);

  auto xml = rng_write_string(p);
  REQUIRE_FALSE(xml.empty());

  // Must parse without throwing
  expat_reader reader(xml);
  rng_xml_parser rp;
  REQUIRE_NOTHROW(rp.parse(reader));
}

TEST_CASE("roundtrip: RNC -> RNG XML parseable (element)", "[rng_roundtrip]") {
  check_rnc_to_rng_parseable("element card { text }");
}

TEST_CASE("roundtrip: RNC -> RNG XML parseable (grammar)", "[rng_roundtrip]") {
  check_rnc_to_rng_parseable("start = root\n"
                             "root = element doc { para+ }\n"
                             "para = element p { text }\n");
}

TEST_CASE("roundtrip: RNC -> RNG XML parseable (operators)",
          "[rng_roundtrip]") {
  check_rnc_to_rng_parseable("element a { text } | element b { empty }");
  check_rnc_to_rng_parseable("element a { text } & element b { empty }");
  check_rnc_to_rng_parseable("element a { text }, element b { empty }");
}

TEST_CASE("roundtrip: RNC -> RNG XML parseable (repetitions)",
          "[rng_roundtrip]") {
  check_rnc_to_rng_parseable("element item { text }+");
  check_rnc_to_rng_parseable("element item { text }*");
  check_rnc_to_rng_parseable("element item { text }?");
}

// -- Cross-format equivalence (grammar input, both formats agree) -------------

TEST_CASE("roundtrip: RNG and RNC grammar produce same canonical XML",
          "[rng_roundtrip]") {
  // Same schema described in both formats; since both start with a grammar
  // using ref, no __start__ synthetic define is created by the compact parser
  // when the start body is an element (it extracts the element name as define).
  std::string xml = R"(<grammar xmlns=")" + rng_ns +
                    R"(">)"
                    R"(<start><ref name="doc"/></start>)"
                    R"(<define name="doc">)"
                    R"(<element name="doc"><text/></element>)"
                    R"(</define></grammar>)";

  // The compact parser for `start = element doc { text }` creates:
  //   start -> ref "doc", define "doc" -> element doc { text }
  // which matches the XML grammar above.
  std::string rnc = "start = element doc { text }";

  expat_reader reader(xml);
  rng_xml_parser rp;
  auto c_rng = rng_write_string(rp.parse(reader));

  rng_compact_parser cp;
  auto c_rnc = rng_write_string(cp.parse(rnc));

  CHECK(c_rng == c_rnc);
}
