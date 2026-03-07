#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/dtd_to_rng.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::rng;
namespace dd = xb::dtd;

static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

// Helper: build a document with a single element
static dd::document
make_doc(dd::element_decl ed) {
  dd::document doc;
  doc.elements.push_back(std::move(ed));
  return doc;
}

// Helper: find a define by name
static const define*
find_define(const grammar_pattern& g, const std::string& name) {
  for (const auto& d : g.defines) {
    if (d.name == name) return &d;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Step 1: Empty document → grammar with empty start
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: empty document", "[dtd_to_rng]") {
  dd::document doc;
  auto result = dtd_to_rng(doc);

  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.start);
  CHECK(g.start->holds<empty_pattern>());
  CHECK(g.defines.empty());
}

// ---------------------------------------------------------------------------
// Step 2: EMPTY element → element with empty_pattern
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: EMPTY element", "[dtd_to_rng]") {
  dd::element_decl ed;
  ed.name = "br";
  ed.content.kind = dd::content_kind::empty;

  auto result = dtd_to_rng(make_doc(std::move(ed)));
  auto& g = result.get<grammar_pattern>();

  REQUIRE(g.start->holds<ref_pattern>());
  CHECK(g.start->get<ref_pattern>().name == "br");

  auto* d = find_define(g, "br");
  REQUIRE(d);
  REQUIRE(d->body->holds<element_pattern>());
  auto& ep = d->body->get<element_pattern>();
  CHECK(ep.name.get<specific_name>().local_name == "br");
  REQUIRE(ep.content->holds<empty_pattern>());
}

// ---------------------------------------------------------------------------
// Step 3: ANY element → element with zeroOrMore(element { anyName } { text })
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: ANY element", "[dtd_to_rng]") {
  dd::element_decl ed;
  ed.name = "doc";
  ed.content.kind = dd::content_kind::any;

  auto result = dtd_to_rng(make_doc(std::move(ed)));
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "doc");
  REQUIRE(d);
  REQUIRE(d->body->holds<element_pattern>());
  auto& ep = d->body->get<element_pattern>();
  // ANY → mixed { zeroOrMore { element { anyName } { text } } }
  // or interleave(text, zeroOrMore(element anyName text))
  // We use: mixed { zeroOrMore { element { anyName } { text } } }
  REQUIRE(ep.content->holds<mixed_pattern>());
  auto& mx = ep.content->get<mixed_pattern>();
  REQUIRE(mx.content->holds<zero_or_more_pattern>());
  auto& zm = mx.content->get<zero_or_more_pattern>();
  REQUIRE(zm.content->holds<element_pattern>());
  CHECK(zm.content->get<element_pattern>().name.holds<any_name_nc>());
}

// ---------------------------------------------------------------------------
// Step 4: (#PCDATA) → element with text
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: PCDATA element → text", "[dtd_to_rng]") {
  dd::element_decl ed;
  ed.name = "title";
  ed.content.kind = dd::content_kind::mixed;

  auto result = dtd_to_rng(make_doc(std::move(ed)));
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "title");
  REQUIRE(d);
  REQUIRE(d->body->holds<element_pattern>());
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<text_pattern>());
}

// ---------------------------------------------------------------------------
// Step 5: (#PCDATA | a | b)* → mixed content with choice refs
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: mixed content with children", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl child_a;
  child_a.name = "em";
  child_a.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child_a));

  dd::element_decl child_b;
  child_b.name = "strong";
  child_b.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child_b));

  dd::element_decl parent;
  parent.name = "p";
  parent.content.kind = dd::content_kind::mixed;
  parent.content.mixed_names = {"em", "strong"};
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "p");
  REQUIRE(d);
  REQUIRE(d->body->holds<element_pattern>());
  auto& ep = d->body->get<element_pattern>();
  // mixed { zeroOrMore { choice { ref "em", ref "strong" } } }
  REQUIRE(ep.content->holds<mixed_pattern>());
  auto& mx = ep.content->get<mixed_pattern>();
  REQUIRE(mx.content->holds<zero_or_more_pattern>());
  auto& zm = mx.content->get<zero_or_more_pattern>();
  REQUIRE(zm.content->holds<choice_pattern>());
}

// ---------------------------------------------------------------------------
// Step 6: (a, b) → sequence (group_pattern)
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: sequence content model", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl a;
  a.name = "name";
  a.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(a));

  dd::element_decl b;
  b.name = "email";
  b.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(b));

  dd::element_decl parent;
  parent.name = "person";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "name", dd::quantifier::one, {}});
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "email", dd::quantifier::one, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "person");
  REQUIRE(d);
  REQUIRE(d->body->holds<element_pattern>());
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<group_pattern>());
  // group(ref "name", ref "email")
  auto& gp = ep.content->get<group_pattern>();
  CHECK(gp.left->holds<ref_pattern>());
  CHECK(gp.right->holds<ref_pattern>());
}

// ---------------------------------------------------------------------------
// Step 7: (a | b) → choice_pattern
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: choice content model", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl a;
  a.name = "cat";
  a.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(a));

  dd::element_decl b;
  b.name = "dog";
  b.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(b));

  dd::element_decl parent;
  parent.name = "pet";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::choice;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "cat", dd::quantifier::one, {}});
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "dog", dd::quantifier::one, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "pet");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<choice_pattern>());
}

// ---------------------------------------------------------------------------
// Step 8: Quantifiers: ?, *, +
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: quantifier ? → optional", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl child;
  child.name = "sub";
  child.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child));

  dd::element_decl parent;
  parent.name = "top";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "sub", dd::quantifier::optional, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "top");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<optional_pattern>());
}

TEST_CASE("dtd_to_rng: quantifier * → zeroOrMore", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl child;
  child.name = "item";
  child.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child));

  dd::element_decl parent;
  parent.name = "list";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "item", dd::quantifier::zero_or_more, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "list");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<zero_or_more_pattern>());
}

TEST_CASE("dtd_to_rng: quantifier + → oneOrMore", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl child;
  child.name = "item";
  child.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child));

  dd::element_decl parent;
  parent.name = "list";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "item", dd::quantifier::one_or_more, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "list");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<one_or_more_pattern>());
}

// ---------------------------------------------------------------------------
// Step 9: CDATA attribute (#REQUIRED) → attribute with text
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: required CDATA attribute", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "img";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "img";
  al.attributes.push_back(dd::attribute_def{
      "src", dd::attribute_type::cdata, {}, dd::default_kind::required, ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "img");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  // empty + required attribute → attribute_pattern (not wrapped in optional)
  REQUIRE(ep.content->holds<attribute_pattern>());
  auto& ap = ep.content->get<attribute_pattern>();
  CHECK(ap.name.get<specific_name>().local_name == "src");
  CHECK(ap.content->holds<text_pattern>());
}

// ---------------------------------------------------------------------------
// Step 10: #IMPLIED attribute → optional(attribute)
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: implied attribute → optional", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "div";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "div";
  al.attributes.push_back(dd::attribute_def{
      "class", dd::attribute_type::cdata, {}, dd::default_kind::implied, ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "div");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<optional_pattern>());
  auto& opt = ep.content->get<optional_pattern>();
  REQUIRE(opt.content->holds<attribute_pattern>());
}

// ---------------------------------------------------------------------------
// Step 11: #FIXED attribute → attribute with value_pattern
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: fixed attribute → value_pattern", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "html";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "html";
  al.attributes.push_back(dd::attribute_def{"version",
                                            dd::attribute_type::cdata,
                                            {},
                                            dd::default_kind::fixed,
                                            "1.0"});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "html");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  // Fixed attribute is still optional in the instance
  REQUIRE(ep.content->holds<optional_pattern>());
  auto& opt = ep.content->get<optional_pattern>();
  REQUIRE(opt.content->holds<attribute_pattern>());
  auto& ap = opt.content->get<attribute_pattern>();
  REQUIRE(ap.content->holds<value_pattern>());
  CHECK(ap.content->get<value_pattern>().value == "1.0");
}

// ---------------------------------------------------------------------------
// Step 12: ID/IDREF attribute types → data_pattern
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: ID attribute type → data_pattern", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "item";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "item";
  al.attributes.push_back(dd::attribute_def{
      "id", dd::attribute_type::id, {}, dd::default_kind::required, ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "item");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<attribute_pattern>());
  auto& ap = ep.content->get<attribute_pattern>();
  REQUIRE(ap.content->holds<data_pattern>());
  CHECK(ap.content->get<data_pattern>().type == "ID");
}

// ---------------------------------------------------------------------------
// Step 13: Enumeration attribute → choice of value_patterns
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: enumeration attribute", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "align";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "align";
  al.attributes.push_back(dd::attribute_def{"dir",
                                            dd::attribute_type::enumeration,
                                            {"left", "right", "center"},
                                            dd::default_kind::required,
                                            ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "align");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  REQUIRE(ep.content->holds<attribute_pattern>());
  auto& ap = ep.content->get<attribute_pattern>();
  REQUIRE(ap.content->holds<choice_pattern>());
  // choice(value "left", choice(value "right", value "center"))
  auto& ch = ap.content->get<choice_pattern>();
  CHECK(ch.left->holds<value_pattern>());
  CHECK(ch.left->get<value_pattern>().value == "left");
}

// ---------------------------------------------------------------------------
// Step 14: Multiple elements → choice start
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: multiple elements → choice start", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl a;
  a.name = "a";
  a.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(a));

  dd::element_decl b;
  b.name = "b";
  b.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(b));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  REQUIRE(g.start->holds<choice_pattern>());
  CHECK(g.defines.size() == 2);
}

// ---------------------------------------------------------------------------
// Step 15: Multiple attributes on one element
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: multiple attributes combined", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl ed;
  ed.name = "input";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  dd::attlist_decl al;
  al.element_name = "input";
  al.attributes.push_back(dd::attribute_def{
      "name", dd::attribute_type::cdata, {}, dd::default_kind::required, ""});
  al.attributes.push_back(dd::attribute_def{
      "value", dd::attribute_type::cdata, {}, dd::default_kind::implied, ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "input");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  // group(attribute "name", optional(attribute "value"))
  REQUIRE(ep.content->holds<group_pattern>());
  auto& gp = ep.content->get<group_pattern>();
  CHECK(gp.left->holds<attribute_pattern>());
  CHECK(gp.right->holds<optional_pattern>());
}

// ---------------------------------------------------------------------------
// Step 16: Content + attributes combined
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: content model with attributes", "[dtd_to_rng]") {
  dd::document doc;

  dd::element_decl child;
  child.name = "item";
  child.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(child));

  dd::element_decl parent;
  parent.name = "list";
  parent.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "item", dd::quantifier::one_or_more, {}});
  parent.content.particle = std::move(cp);
  doc.elements.push_back(std::move(parent));

  dd::attlist_decl al;
  al.element_name = "list";
  al.attributes.push_back(dd::attribute_def{
      "id", dd::attribute_type::id, {}, dd::default_kind::required, ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "list");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  // group(oneOrMore(ref "item"), attribute "id")
  REQUIRE(ep.content->holds<group_pattern>());
  auto& gp = ep.content->get<group_pattern>();
  CHECK(gp.left->holds<one_or_more_pattern>());
  CHECK(gp.right->holds<attribute_pattern>());
}

// ---------------------------------------------------------------------------
// Step 17: Nested content model groups
// ---------------------------------------------------------------------------

TEST_CASE("dtd_to_rng: nested groups ((a, b) | c)", "[dtd_to_rng]") {
  dd::document doc;

  for (const auto& n : {"a", "b", "c"}) {
    dd::element_decl ed;
    ed.name = n;
    ed.content.kind = dd::content_kind::mixed;
    doc.elements.push_back(std::move(ed));
  }

  dd::element_decl parent;
  parent.name = "top";
  parent.content.kind = dd::content_kind::children;

  dd::content_particle seq;
  seq.kind = dd::particle_kind::sequence;
  seq.children.push_back(dd::content_particle{
      dd::particle_kind::name, "a", dd::quantifier::one, {}});
  seq.children.push_back(dd::content_particle{
      dd::particle_kind::name, "b", dd::quantifier::one, {}});

  dd::content_particle root;
  root.kind = dd::particle_kind::choice;
  root.children.push_back(std::move(seq));
  root.children.push_back(dd::content_particle{
      dd::particle_kind::name, "c", dd::quantifier::one, {}});
  parent.content.particle = std::move(root);
  doc.elements.push_back(std::move(parent));

  auto result = dtd_to_rng(doc);
  auto& g = result.get<grammar_pattern>();

  auto* d = find_define(g, "top");
  REQUIRE(d);
  auto& ep = d->body->get<element_pattern>();
  // choice(group(ref "a", ref "b"), ref "c")
  REQUIRE(ep.content->holds<choice_pattern>());
  auto& ch = ep.content->get<choice_pattern>();
  CHECK(ch.left->holds<group_pattern>());
  CHECK(ch.right->holds<ref_pattern>());
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
