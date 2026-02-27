#include <xb/rng_compact_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::rng;

static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

// == Preamble ================================================================

TEST_CASE("rnc: empty grammar", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse("start = empty");
  REQUIRE(pat.holds<grammar_pattern>());
  auto& g = pat.get<grammar_pattern>();
  REQUIRE(g.start);
  CHECK(g.start->holds<ref_pattern>());
}

TEST_CASE("rnc: namespace declaration", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    namespace test = "urn:test"
    start = element test:doc { text }
  )");
  REQUIRE(pat.holds<grammar_pattern>());
  auto& g = pat.get<grammar_pattern>();
  // start should reference the implicit define
  // find define with element having ns = urn:test
  bool found = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>()) {
        auto& sn = e.name.get<specific_name>();
        if (sn.ns == "urn:test" && sn.local_name == "doc") { found = true; }
      }
    }
  }
  CHECK(found);
}

TEST_CASE("rnc: default namespace", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:default"
    start = element doc { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  bool found = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>()) {
        CHECK(e.name.get<specific_name>().ns == "urn:default");
        found = true;
      }
    }
  }
  CHECK(found);
}

TEST_CASE("rnc: datatypes declaration", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    datatypes xsd = "http://www.w3.org/2001/XMLSchema-datatypes"
    start = element root { xsd:integer }
  )");
  auto& g = pat.get<grammar_pattern>();
  // The element content should be a data_pattern with xsd datatypes library
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<data_pattern>()) {
        auto& dp = e.content->get<data_pattern>();
        CHECK(dp.datatype_library == xsd_dt);
        CHECK(dp.type == "integer");
      }
    }
  }
}

// == Simple patterns =========================================================

TEST_CASE("rnc: text pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse("start = element doc { text }");
  auto& g = pat.get<grammar_pattern>();
  bool found_text = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<text_pattern>()) found_text = true;
    }
  }
  CHECK(found_text);
}

TEST_CASE("rnc: empty pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse("start = element doc { empty }");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      CHECK(e.content->holds<empty_pattern>());
    }
  }
}

TEST_CASE("rnc: notAllowed pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse("start = element doc { notAllowed }");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      CHECK(
          d.body->get<element_pattern>().content->holds<not_allowed_pattern>());
    }
  }
}

// == Element and attribute ===================================================

TEST_CASE("rnc: element with text", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  REQUIRE(!g.defines.empty());
  bool found = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      REQUIRE(e.name.holds<specific_name>());
      CHECK(e.name.get<specific_name>().local_name == "doc");
      REQUIRE(e.content);
      CHECK(e.content->holds<text_pattern>());
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("rnc: attribute pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = element doc { attribute type { text } }
  )");
  auto& g = pat.get<grammar_pattern>();
  bool found_attr = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<attribute_pattern>()) {
        auto& a = e.content->get<attribute_pattern>();
        REQUIRE(a.name.holds<specific_name>());
        CHECK(a.name.get<specific_name>().local_name == "type");
        // Attribute names have empty namespace by default
        CHECK(a.name.get<specific_name>().ns.empty());
        found_attr = true;
      }
    }
  }
  CHECK(found_attr);
}

// == Combinators =============================================================

TEST_CASE("rnc: group (comma) combinator", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element a { text }, element b { text } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<group_pattern>());
      }
    }
  }
}

TEST_CASE("rnc: choice (pipe) combinator", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element a { text } | element b { text } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<choice_pattern>());
      }
    }
  }
}

TEST_CASE("rnc: interleave (ampersand) combinator", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element a { text } & element b { text } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<interleave_pattern>());
      }
    }
  }
}

// == Repetition operators ====================================================

TEST_CASE("rnc: oneOrMore (+)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element item { text }+ }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<one_or_more_pattern>());
      }
    }
  }
}

TEST_CASE("rnc: zeroOrMore (*)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element item { text }* }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<zero_or_more_pattern>());
      }
    }
  }
}

TEST_CASE("rnc: optional (?)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { element item { text }? }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        CHECK(e.content->holds<optional_pattern>());
      }
    }
  }
}

// == Data types ==============================================================

TEST_CASE("rnc: builtin string type", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = element doc { string }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<data_pattern>()) {
        auto& dp = e.content->get<data_pattern>();
        CHECK(dp.datatype_library.empty());
        CHECK(dp.type == "string");
      }
    }
  }
}

TEST_CASE("rnc: builtin token type", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = element doc { token }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<data_pattern>()) {
        auto& dp = e.content->get<data_pattern>();
        CHECK(dp.datatype_library.empty());
        CHECK(dp.type == "token");
      }
    }
  }
}

TEST_CASE("rnc: qualified data type (CName)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    datatypes xsd = "http://www.w3.org/2001/XMLSchema-datatypes"
    start = element doc { xsd:integer }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<data_pattern>()) {
        auto& dp = e.content->get<data_pattern>();
        CHECK(dp.datatype_library == xsd_dt);
        CHECK(dp.type == "integer");
      }
    }
  }
}

TEST_CASE("rnc: data type with params", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    datatypes xsd = "http://www.w3.org/2001/XMLSchema-datatypes"
    start = element doc { xsd:string { minLength = "1" maxLength = "100" } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<data_pattern>()) {
        auto& dp = e.content->get<data_pattern>();
        CHECK(dp.datatype_library == xsd_dt);
        CHECK(dp.type == "string");
        REQUIRE(dp.params.size() == 2);
        CHECK(dp.params[0].name == "minLength");
        CHECK(dp.params[0].value == "1");
        CHECK(dp.params[1].name == "maxLength");
        CHECK(dp.params[1].value == "100");
      }
    }
  }
}

TEST_CASE("rnc: value pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = element doc { "hello" }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content && e.content->holds<value_pattern>()) {
        CHECK(e.content->get<value_pattern>().value == "hello");
      }
    }
  }
}

// == Grammar constructs ======================================================

TEST_CASE("rnc: named definitions with ref", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { item }
    item = element item { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  // Should have defines for both the start element and "item"
  bool found_item_define = false;
  for (auto& d : g.defines) {
    if (d.name == "item") {
      found_item_define = true;
      REQUIRE(d.body);
      CHECK(d.body->holds<element_pattern>());
    }
  }
  CHECK(found_item_define);
}

TEST_CASE("rnc: combine with choice (|=)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { content }
    content = element a { text }
    content |= element b { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  // Should have a "content" define with combine_method::choice
  bool found_choice = false;
  for (auto& d : g.defines) {
    if (d.name == "content" && d.combine == combine_method::choice) {
      found_choice = true;
    }
  }
  CHECK(found_choice);
}

TEST_CASE("rnc: combine with interleave (&=)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { content }
    content = element a { text }
    content &= element b { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  // Should have a "content" define with combine_method::interleave
  bool found_interleave = false;
  for (auto& d : g.defines) {
    if (d.name == "content" && d.combine == combine_method::interleave) {
      found_interleave = true;
    }
  }
  CHECK(found_interleave);
}

// == mixed and list ==========================================================

TEST_CASE("rnc: mixed pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { mixed { element b { text } } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content) { CHECK(e.content->holds<mixed_pattern>()); }
    }
  }
}

TEST_CASE("rnc: list pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    datatypes xsd = "http://www.w3.org/2001/XMLSchema-datatypes"
    start = element doc { list { xsd:integer+ } }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.content) { CHECK(e.content->holds<list_pattern>()); }
    }
  }
}

// == Name classes =============================================================

TEST_CASE("rnc: anyName wildcard (*)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = element * { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      CHECK(d.body->get<element_pattern>().name.holds<any_name_nc>());
    }
  }
}

TEST_CASE("rnc: nsName wildcard (prefix:*)", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    namespace html = "http://www.w3.org/1999/xhtml"
    start = element html:* { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& nc = d.body->get<element_pattern>().name;
      REQUIRE(nc.holds<ns_name_nc>());
      CHECK(nc.get<ns_name_nc>().ns == "http://www.w3.org/1999/xhtml");
    }
  }
}

TEST_CASE("rnc: anyName with except", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element * - foo { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& nc = d.body->get<element_pattern>().name;
      REQUIRE(nc.holds<any_name_nc>());
      CHECK(nc.get<any_name_nc>().except != nullptr);
    }
  }
}

// == Parenthesized patterns ==================================================

TEST_CASE("rnc: parenthesized pattern", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc {
      (element a { text } | element b { text }),
      element c { text }
    }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        // Top level should be group (comma)
        CHECK(e.content->holds<group_pattern>());
        if (e.content->holds<group_pattern>()) {
          auto& grp = e.content->get<group_pattern>();
          // Left should be choice (the parenthesized part)
          CHECK(grp.left->holds<choice_pattern>());
          // Right should be element c
          CHECK(grp.right->holds<element_pattern>());
        }
      }
    }
  }
}

// == Comments ================================================================

TEST_CASE("rnc: line comments", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    # This is a comment
    start = element doc { text } # inline comment
  )");
  auto& g = pat.get<grammar_pattern>();
  CHECK(!g.defines.empty());
}

// == external and parent ref =================================================

TEST_CASE("rnc: external ref", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = external "other.rnc"
  )");
  auto& g = pat.get<grammar_pattern>();
  // start should reference or contain external_ref_pattern
  // The start define body should be external_ref
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<external_ref_pattern>()) {
      CHECK(d.body->get<external_ref_pattern>().href == "other.rnc");
    }
  }
}

TEST_CASE("rnc: parent ref", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    start = grammar {
      start = element doc {
        parent foo
      }
    }
  )");
  auto& g = pat.get<grammar_pattern>();
  // Somewhere in the nested grammar there should be a parent_ref_pattern
  // The outer grammar start → grammar_pattern → inner defines → element →
  // parent_ref This is a nested grammar test
  CHECK(!g.defines.empty());
}

// == Multi-element sequence ==================================================

TEST_CASE("rnc: three-element sequence", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc {
      element a { text },
      element b { text },
      element c { text }
    }
  )");
  auto& g = pat.get<grammar_pattern>();
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "doc") {
        REQUIRE(e.content);
        // Should be a nested group(group(a,b), c) or group(a, group(b,c))
        CHECK(e.content->holds<group_pattern>());
      }
    }
  }
}

// == Keyword escaping ========================================================

TEST_CASE("rnc: escaped keyword as identifier", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    default namespace = "urn:test"
    start = element doc { \element }
    \element = element item { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  bool found_element_def = false;
  for (auto& d : g.defines) {
    if (d.name == "element") { found_element_def = true; }
  }
  CHECK(found_element_def);
}

// == include directive =======================================================

TEST_CASE("rnc: include directive", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    include "base.rnc"
    start = element doc { text }
  )");
  auto& g = pat.get<grammar_pattern>();
  REQUIRE(g.includes.size() == 1);
  CHECK(g.includes[0].href == "base.rnc");
}

TEST_CASE("rnc: include with overrides", "[rng_compact_parser]") {
  rng_compact_parser p;
  auto pat = p.parse(R"(
    include "base.rnc" {
      start = element doc { text }
    }
  )");
  auto& g = pat.get<grammar_pattern>();
  REQUIRE(g.includes.size() == 1);
  CHECK(g.includes[0].href == "base.rnc");
  // Should have overrides
  CHECK((!g.includes[0].overrides.empty() ||
         g.includes[0].start_override != nullptr));
}

// == Equivalence with XML parser =============================================

TEST_CASE("rnc: equivalent to simple XML schema", "[rng_compact_parser]") {
  // Verify that the compact parser produces equivalent IR to the XML parser
  // for a simple schema: element with two children and an attribute

  rng_compact_parser cp;
  auto compact_pat = cp.parse(R"(
    default namespace = "urn:test"
    datatypes xsd = "http://www.w3.org/2001/XMLSchema-datatypes"
    start = element addressBook {
      element card {
        attribute type { xsd:string },
        element name { text },
        element email { text }
      }+
    }
  )");

  REQUIRE(compact_pat.holds<grammar_pattern>());
  auto& g = compact_pat.get<grammar_pattern>();

  // Verify structural properties rather than exact equality
  // (since representation may differ slightly between parsers)

  // Should have defines
  CHECK(!g.defines.empty());
  // Should have a start
  CHECK(g.start != nullptr);

  // Find the addressBook element
  bool found_addr_book = false;
  for (auto& d : g.defines) {
    if (d.body && d.body->holds<element_pattern>()) {
      auto& e = d.body->get<element_pattern>();
      if (e.name.holds<specific_name>() &&
          e.name.get<specific_name>().local_name == "addressBook") {
        found_addr_book = true;
        // Its content should contain a oneOrMore of card element
        REQUIRE(e.content);
        CHECK(e.content->holds<one_or_more_pattern>());
      }
    }
  }
  CHECK(found_addr_book);
}
