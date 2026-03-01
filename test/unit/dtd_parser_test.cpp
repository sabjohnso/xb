#include <xb/dtd_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::dtd;

// -- 1. Simplest element: EMPTY -----------------------------------------------

TEST_CASE("dtd parser: ELEMENT book EMPTY", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT book EMPTY>");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].name == "book");
  CHECK(doc.elements[0].content.kind == content_kind::empty);
}

// -- 2. Text-only element: (#PCDATA) ------------------------------------------

TEST_CASE("dtd parser: ELEMENT title (#PCDATA)", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT title (#PCDATA)>");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].name == "title");
  CHECK(doc.elements[0].content.kind == content_kind::mixed);
  CHECK(doc.elements[0].content.mixed_names.empty());
}

// -- 3. Children content: (chapter+) ------------------------------------------

TEST_CASE("dtd parser: ELEMENT book (chapter+)", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT book (chapter+)>");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].name == "book");
  CHECK(doc.elements[0].content.kind == content_kind::children);
  REQUIRE(doc.elements[0].content.particle.has_value());
  auto& cp = *doc.elements[0].content.particle;
  CHECK(cp.kind == particle_kind::sequence);
  REQUIRE(cp.children.size() == 1);
  CHECK(cp.children[0].name == "chapter");
  CHECK(cp.children[0].quantifier == quantifier::one_or_more);
}

// -- 4. Nested groups: (a, b, (c | d)*) ---------------------------------------

TEST_CASE("dtd parser: ELEMENT doc (a, b, (c | d)*)", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT doc (a, b, (c | d)*)>");
  REQUIRE(doc.elements.size() == 1);
  auto& cs = doc.elements[0].content;
  CHECK(cs.kind == content_kind::children);
  REQUIRE(cs.particle.has_value());
  auto& root = *cs.particle;
  CHECK(root.kind == particle_kind::sequence);
  REQUIRE(root.children.size() == 3);
  CHECK(root.children[0].name == "a");
  CHECK(root.children[1].name == "b");
  CHECK(root.children[2].kind == particle_kind::choice);
  CHECK(root.children[2].quantifier == quantifier::zero_or_more);
  REQUIRE(root.children[2].children.size() == 2);
  CHECK(root.children[2].children[0].name == "c");
  CHECK(root.children[2].children[1].name == "d");
}

// -- 5. Mixed content: (#PCDATA | em | strong)* -------------------------------

TEST_CASE("dtd parser: ELEMENT mixed (#PCDATA | em | strong)*",
          "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT mixed (#PCDATA | em | strong)*>");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].content.kind == content_kind::mixed);
  CHECK(doc.elements[0].content.mixed_names.size() == 2);
  CHECK(doc.elements[0].content.mixed_names[0] == "em");
  CHECK(doc.elements[0].content.mixed_names[1] == "strong");
}

// -- 6. Attribute declaration: ID #IMPLIED ------------------------------------

TEST_CASE("dtd parser: ATTLIST book id ID #IMPLIED", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ATTLIST book id ID #IMPLIED>");
  REQUIRE(doc.attlists.size() == 1);
  CHECK(doc.attlists[0].element_name == "book");
  REQUIRE(doc.attlists[0].attributes.size() == 1);
  auto& ad = doc.attlists[0].attributes[0];
  CHECK(ad.name == "id");
  CHECK(ad.type == attribute_type::id);
  CHECK(ad.default_kind == default_kind::implied);
}

// -- 7. Enumeration attribute: (fiction | nonfiction) "fiction"
// ----------------

TEST_CASE("dtd parser: ATTLIST enumeration with default", "[dtd_parser]") {
  dtd_parser p;
  auto doc =
      p.parse(R"(<!ATTLIST book type (fiction | nonfiction) "fiction">)");
  REQUIRE(doc.attlists.size() == 1);
  auto& ad = doc.attlists[0].attributes[0];
  CHECK(ad.name == "type");
  CHECK(ad.type == attribute_type::enumeration);
  CHECK(ad.enum_values.size() == 2);
  CHECK(ad.enum_values[0] == "fiction");
  CHECK(ad.enum_values[1] == "nonfiction");
  CHECK(ad.default_kind == default_kind::value);
  CHECK(ad.default_value == "fiction");
}

// -- 8. Parameter entity expansion --------------------------------------------

TEST_CASE("dtd parser: parameter entity expansion", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <!ENTITY % inline "em | strong">
    <!ELEMENT p (#PCDATA | %inline;)*>
  )");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].name == "p");
  CHECK(doc.elements[0].content.kind == content_kind::mixed);
  CHECK(doc.elements[0].content.mixed_names.size() == 2);
  CHECK(doc.elements[0].content.mixed_names[0] == "em");
  CHECK(doc.elements[0].content.mixed_names[1] == "strong");
}

// -- 9. Multi-declaration document --------------------------------------------

TEST_CASE("dtd parser: multi-declaration document", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <!ELEMENT book (title, chapter+)>
    <!ELEMENT title (#PCDATA)>
    <!ELEMENT chapter (#PCDATA | em)*>
    <!ELEMENT em (#PCDATA)>
    <!ATTLIST book isbn CDATA #REQUIRED>
    <!ATTLIST book lang CDATA #IMPLIED>
    <!ATTLIST chapter id ID #IMPLIED>
    <!ENTITY % version "1.0">
  )");

  CHECK(doc.elements.size() == 4);
  CHECK(doc.attlists.size() == 3);
  CHECK(doc.entities.size() == 1);
}

// -- ANY content spec ---------------------------------------------------------

TEST_CASE("dtd parser: ELEMENT any ANY", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT any ANY>");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].content.kind == content_kind::any);
}

// -- ATTLIST with multiple attributes -----------------------------------------

TEST_CASE("dtd parser: ATTLIST with multiple attributes", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <!ATTLIST img
      src CDATA #REQUIRED
      alt CDATA #IMPLIED
      width NMTOKEN #IMPLIED>
  )");
  REQUIRE(doc.attlists.size() == 1);
  REQUIRE(doc.attlists[0].attributes.size() == 3);
  CHECK(doc.attlists[0].attributes[0].name == "src");
  CHECK(doc.attlists[0].attributes[0].type == attribute_type::cdata);
  CHECK(doc.attlists[0].attributes[0].default_kind == default_kind::required);
  CHECK(doc.attlists[0].attributes[1].name == "alt");
  CHECK(doc.attlists[0].attributes[2].name == "width");
  CHECK(doc.attlists[0].attributes[2].type == attribute_type::nmtoken);
}

// -- FIXED attribute ----------------------------------------------------------

TEST_CASE("dtd parser: ATTLIST with FIXED value", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(<!ATTLIST doc version CDATA #FIXED "1.0">)");
  REQUIRE(doc.attlists.size() == 1);
  auto& ad = doc.attlists[0].attributes[0];
  CHECK(ad.name == "version");
  CHECK(ad.default_kind == default_kind::fixed);
  CHECK(ad.default_value == "1.0");
}

// -- Attribute types: IDREF, IDREFS, NMTOKENS, ENTITY, ENTITIES, NOTATION -----

TEST_CASE("dtd parser: ATTLIST IDREF and IDREFS", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <!ATTLIST ref target IDREF #IMPLIED>
    <!ATTLIST refs targets IDREFS #IMPLIED>
  )");
  CHECK(doc.attlists[0].attributes[0].type == attribute_type::idref);
  CHECK(doc.attlists[1].attributes[0].type == attribute_type::idrefs);
}

TEST_CASE("dtd parser: ATTLIST NMTOKENS", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ATTLIST x classes NMTOKENS #IMPLIED>");
  CHECK(doc.attlists[0].attributes[0].type == attribute_type::nmtokens);
}

// -- Sequence with optional: (a, b?) ------------------------------------------

TEST_CASE("dtd parser: ELEMENT with optional child (a, b?)", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse("<!ELEMENT x (a, b?)>");
  auto& root = *doc.elements[0].content.particle;
  CHECK(root.kind == particle_kind::sequence);
  REQUIRE(root.children.size() == 2);
  CHECK(root.children[0].quantifier == quantifier::one);
  CHECK(root.children[1].quantifier == quantifier::optional);
}

// -- General entity -----------------------------------------------------------

TEST_CASE("dtd parser: general entity declaration", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(<!ENTITY copyright "Copyright 2026">)");
  REQUIRE(doc.entities.size() == 1);
  CHECK(doc.entities[0].name == "copyright");
  CHECK(doc.entities[0].is_parameter == false);
  CHECK(doc.entities[0].value == "Copyright 2026");
}

// -- Comments -----------------------------------------------------------------

TEST_CASE("dtd parser: comments are skipped", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <!-- This is a comment -->
    <!ELEMENT book EMPTY>
    <!-- Another comment -->
  )");
  REQUIRE(doc.elements.size() == 1);
  CHECK(doc.elements[0].name == "book");
}

// -- Processing instructions --------------------------------------------------

TEST_CASE("dtd parser: processing instructions are skipped", "[dtd_parser]") {
  dtd_parser p;
  auto doc = p.parse(R"(
    <?xml version="1.0"?>
    <!ELEMENT book EMPTY>
  )");
  REQUIRE(doc.elements.size() == 1);
}
