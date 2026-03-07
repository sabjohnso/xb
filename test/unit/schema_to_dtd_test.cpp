#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/schema_to_dtd.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace dd = xb::dtd;

static const std::string xs = "http://www.w3.org/2001/XMLSchema";
static const std::string tns = "urn:test";

static schema_set
make_ss(schema s) {
  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();
  return ss;
}

// Helper: find element by name in DTD document
static const dd::element_decl*
find_elem(const dd::document& doc, const std::string& name) {
  for (const auto& e : doc.elements) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

// Helper: find attlist by element name
static const dd::attlist_decl*
find_attlist(const dd::document& doc, const std::string& elem_name) {
  for (const auto& al : doc.attlists) {
    if (al.element_name == elem_name) return &al;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Simple element with xs:string → PCDATA
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: simple string element → PCDATA", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);
  s.add_element(element_decl(qname(tns, "greeting"), qname(xs, "string")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "greeting");
  REQUIRE(ed);
  CHECK(ed->content.kind == dd::content_kind::mixed);
  CHECK(ed->content.mixed_names.empty());
}

// ---------------------------------------------------------------------------
// Empty complex type → EMPTY
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: empty complex type → EMPTY", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);
  content_type ct;
  s.add_complex_type(
      complex_type(qname(tns, "brType"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "br"), qname(tns, "brType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "br");
  REQUIRE(ed);
  CHECK(ed->content.kind == dd::content_kind::empty);
}

// ---------------------------------------------------------------------------
// Sequence content model → children
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: sequence → children content", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);

  s.add_element(element_decl(qname(tns, "name"), qname(xs, "string")));
  s.add_element(element_decl(qname(tns, "email"), qname(xs, "string")));

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "name"), qname(xs, "string")));
  parts.emplace_back(element_decl(qname(tns, "email"), qname(xs, "string")));
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "personType"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "person"), qname(tns, "personType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "person");
  REQUIRE(ed);
  CHECK(ed->content.kind == dd::content_kind::children);
  REQUIRE(ed->content.particle.has_value());
  CHECK(ed->content.particle->kind == dd::particle_kind::sequence);
  CHECK(ed->content.particle->children.size() == 2);
}

// ---------------------------------------------------------------------------
// Choice content model
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: choice → choice content", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "cat"), qname(xs, "string")));
  parts.emplace_back(element_decl(qname(tns, "dog"), qname(xs, "string")));
  model_group mg(compositor_kind::choice, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "petType"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "pet"), qname(tns, "petType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "pet");
  REQUIRE(ed);
  REQUIRE(ed->content.particle.has_value());
  CHECK(ed->content.particle->kind == dd::particle_kind::choice);
}

// ---------------------------------------------------------------------------
// Occurrence mapping
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: occurrence mapping", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "a"), qname(xs, "string")),
                     occurrence{0, 1});
  parts.emplace_back(element_decl(qname(tns, "b"), qname(xs, "string")),
                     occurrence{0, unbounded});
  parts.emplace_back(element_decl(qname(tns, "c"), qname(xs, "string")),
                     occurrence{1, unbounded});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "occType"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "occ"), qname(tns, "occType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "occ");
  REQUIRE(ed);
  REQUIRE(ed->content.particle.has_value());
  auto& children = ed->content.particle->children;
  REQUIRE(children.size() == 3);
  CHECK(children[0].quant == dd::quantifier::optional);
  CHECK(children[1].quant == dd::quantifier::zero_or_more);
  CHECK(children[2].quant == dd::quantifier::one_or_more);
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: attributes", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);

  content_type ct;
  std::vector<attribute_use> attrs;
  attrs.push_back(
      attribute_use{qname("", "id"), qname(xs, "ID"), true, {}, {}});
  attrs.push_back(
      attribute_use{qname("", "class"), qname(xs, "string"), false, {}, {}});
  s.add_complex_type(complex_type(qname(tns, "divType"), false, false,
                                  std::move(ct), std::move(attrs)));
  s.add_element(element_decl(qname(tns, "div"), qname(tns, "divType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* al = find_attlist(doc, "div");
  REQUIRE(al);
  REQUIRE(al->attributes.size() == 2);
  CHECK(al->attributes[0].name == "id");
  CHECK(al->attributes[0].type == dd::attribute_type::id);
  CHECK(al->attributes[0].dflt == dd::default_kind::required);
  CHECK(al->attributes[1].name == "class");
  CHECK(al->attributes[1].dflt == dd::default_kind::implied);
}

// ---------------------------------------------------------------------------
// Mixed content → mixed
// ---------------------------------------------------------------------------

TEST_CASE("schema_to_dtd: mixed content", "[schema_to_dtd]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "b"), qname(xs, "string")));
  model_group mg(compositor_kind::choice, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::mixed, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "pType"), false, true, std::move(ct)));
  s.add_element(element_decl(qname(tns, "p"), qname(tns, "pType")));
  auto ss = make_ss(std::move(s));
  auto doc = schema_to_dtd(ss);

  auto* ed = find_elem(doc, "p");
  REQUIRE(ed);
  CHECK(ed->content.kind == dd::content_kind::mixed);
  CHECK(ed->content.mixed_names.size() == 1);
  CHECK(ed->content.mixed_names[0] == "b");
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
