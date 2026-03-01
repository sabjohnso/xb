// GCC 12-13 emit a false -Wmaybe-uninitialized when constructing
// particle objects (std::variant containing std::unique_ptr) at -O3.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/dtd_translator.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace dd = xb::dtd;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

// Helper: build a DTD document with a single element
static dd::document
make_doc(dd::element_decl ed) {
  dd::document doc;
  doc.elements.push_back(std::move(ed));
  return doc;
}

// -- EMPTY element -> complex_type with empty content -------------------------

TEST_CASE("dtd translate: EMPTY element", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "br";
  ed.content.kind = dd::content_kind::empty;

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  CHECK(s.target_namespace().empty());
  REQUIRE(s.elements().size() >= 1);
  CHECK(s.elements()[0].name().local_name() == "br");

  REQUIRE(!s.complex_types().empty());
  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "brType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  CHECK(ct->content().kind == xb::content_kind::empty);
}

// -- ANY element -> complex_type (empty content, best approx) -----------------

TEST_CASE("dtd translate: ANY element", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "doc";
  ed.content.kind = dd::content_kind::any;

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];
  REQUIRE(s.elements().size() >= 1);
  CHECK(s.elements()[0].name().local_name() == "doc");
}

// -- (#PCDATA) element -> simple content xs:string ----------------------------

TEST_CASE("dtd translate: PCDATA element -> simple content",
          "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "title";
  ed.content.kind = dd::content_kind::mixed;

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];
  REQUIRE(s.elements().size() >= 1);
  CHECK(s.elements()[0].type_name() == qname(xs_ns, "string"));
}

// -- Children content (a, b) -> complex type with sequence --------------------

TEST_CASE("dtd translate: sequence (a, b)", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "root";
  ed.content.kind = dd::content_kind::children;

  dd::content_particle a;
  a.kind = dd::particle_kind::name;
  a.name = "a";
  dd::content_particle b;
  b.kind = dd::particle_kind::name;
  b.name = "b";

  dd::content_particle seq;
  seq.kind = dd::particle_kind::sequence;
  seq.children.push_back(std::move(a));
  seq.children.push_back(std::move(b));
  ed.content.particle = std::move(seq);

  dd::document doc;
  doc.elements.push_back(std::move(ed));
  {
    dd::element_decl ea;
    ea.name = "a";
    ea.content.kind = dd::content_kind::mixed;
    doc.elements.push_back(std::move(ea));
  }
  {
    dd::element_decl eb;
    eb.name = "b";
    eb.content.kind = dd::content_kind::mixed;
    doc.elements.push_back(std::move(eb));
  }

  auto ss = dtd_translate(std::move(doc));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "rootType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  auto* cc = std::get_if<complex_content>(&ct->content().detail);
  REQUIRE(cc != nullptr);
  REQUIRE(cc->content_model.has_value());
  CHECK(cc->content_model->compositor() == compositor_kind::sequence);
  CHECK(cc->content_model->particles().size() == 2);
}

// -- Choice (a | b) -> complex type with choice compositor --------------------

TEST_CASE("dtd translate: choice (a | b)", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "root";
  ed.content.kind = dd::content_kind::children;

  dd::content_particle a;
  a.kind = dd::particle_kind::name;
  a.name = "a";
  dd::content_particle b;
  b.kind = dd::particle_kind::name;
  b.name = "b";

  dd::content_particle ch;
  ch.kind = dd::particle_kind::choice;
  ch.children.push_back(std::move(a));
  ch.children.push_back(std::move(b));
  ed.content.particle = std::move(ch);

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "rootType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  auto* cc = std::get_if<complex_content>(&ct->content().detail);
  REQUIRE(cc != nullptr);
  REQUIRE(cc->content_model.has_value());
  CHECK(cc->content_model->compositor() == compositor_kind::choice);
}

// -- Quantifiers -> occurrence ------------------------------------------------

TEST_CASE("dtd translate: quantifier * -> {0, unbounded}", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "root";
  ed.content.kind = dd::content_kind::children;

  dd::content_particle child;
  child.kind = dd::particle_kind::name;
  child.name = "item";
  child.quantifier = dd::quantifier::zero_or_more;

  dd::content_particle seq;
  seq.kind = dd::particle_kind::sequence;
  seq.children.push_back(std::move(child));
  ed.content.particle = std::move(seq);

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "rootType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  auto* cc = std::get_if<complex_content>(&ct->content().detail);
  REQUIRE(cc != nullptr);
  REQUIRE(cc->content_model.has_value());
  REQUIRE(!cc->content_model->particles().empty());
  auto& p = cc->content_model->particles()[0];
  CHECK(p.occurs.min_occurs == 0);
  CHECK(p.occurs.is_unbounded());
}

TEST_CASE("dtd translate: quantifier + -> {1, unbounded}", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "root";
  ed.content.kind = dd::content_kind::children;

  dd::content_particle child;
  child.kind = dd::particle_kind::name;
  child.name = "item";
  child.quantifier = dd::quantifier::one_or_more;

  dd::content_particle seq;
  seq.kind = dd::particle_kind::sequence;
  seq.children.push_back(std::move(child));
  ed.content.particle = std::move(seq);

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "rootType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  auto* cc = std::get_if<complex_content>(&ct->content().detail);
  REQUIRE(cc != nullptr);
  REQUIRE(!cc->content_model->particles().empty());
  CHECK(cc->content_model->particles()[0].occurs.min_occurs == 1);
  CHECK(cc->content_model->particles()[0].occurs.is_unbounded());
}

TEST_CASE("dtd translate: quantifier ? -> {0, 1}", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "root";
  ed.content.kind = dd::content_kind::children;

  dd::content_particle child;
  child.kind = dd::particle_kind::name;
  child.name = "item";
  child.quantifier = dd::quantifier::optional;

  dd::content_particle seq;
  seq.kind = dd::particle_kind::sequence;
  seq.children.push_back(std::move(child));
  ed.content.particle = std::move(seq);

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "rootType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  auto* cc = std::get_if<complex_content>(&ct->content().detail);
  REQUIRE(cc != nullptr);
  REQUIRE(!cc->content_model->particles().empty());
  CHECK(cc->content_model->particles()[0].occurs.min_occurs == 0);
  CHECK(cc->content_model->particles()[0].occurs.max_occurs == 1);
}

// -- Mixed content -> mixed flag on complex_type ------------------------------

TEST_CASE("dtd translate: mixed content (#PCDATA | em)*", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "p";
  ed.content.kind = dd::content_kind::mixed;
  ed.content.mixed_names = {"em", "strong"};

  auto ss = dtd_translate(make_doc(std::move(ed)));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "pType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  CHECK(ct->mixed() == true);
}

// -- Attributes ---------------------------------------------------------------

TEST_CASE("dtd translate: CDATA attribute -> xs:string attribute_use",
          "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "img";
  ed.content.kind = dd::content_kind::empty;

  dd::attlist_decl al;
  al.element_name = "img";
  dd::attribute_def ad;
  ad.name = "src";
  ad.type = dd::attribute_type::cdata;
  ad.default_kind = dd::default_kind::required;
  al.attributes.push_back(std::move(ad));

  dd::document doc;
  doc.elements.push_back(std::move(ed));
  doc.attlists.push_back(std::move(al));

  auto ss = dtd_translate(std::move(doc));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "imgType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  REQUIRE(ct->attributes().size() >= 1);
  CHECK(ct->attributes()[0].name.local_name() == "src");
  CHECK(ct->attributes()[0].type_name == qname(xs_ns, "string"));
  CHECK(ct->attributes()[0].required == true);
}

TEST_CASE("dtd translate: ID attribute -> xs:ID", "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "div";
  ed.content.kind = dd::content_kind::empty;

  dd::attlist_decl al;
  al.element_name = "div";
  dd::attribute_def ad;
  ad.name = "id";
  ad.type = dd::attribute_type::id;
  ad.default_kind = dd::default_kind::implied;
  al.attributes.push_back(std::move(ad));

  dd::document doc;
  doc.elements.push_back(std::move(ed));
  doc.attlists.push_back(std::move(al));

  auto ss = dtd_translate(std::move(doc));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "divType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  REQUIRE(!ct->attributes().empty());
  CHECK(ct->attributes()[0].type_name == qname(xs_ns, "ID"));
  CHECK(ct->attributes()[0].required == false);
}

TEST_CASE("dtd translate: enumeration attribute -> simple type with enum facet",
          "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "book";
  ed.content.kind = dd::content_kind::empty;

  dd::attlist_decl al;
  al.element_name = "book";
  dd::attribute_def ad;
  ad.name = "genre";
  ad.type = dd::attribute_type::enumeration;
  ad.enum_values = {"fiction", "nonfiction", "poetry"};
  ad.default_kind = dd::default_kind::value;
  ad.default_value = "fiction";
  al.attributes.push_back(std::move(ad));

  dd::document doc;
  doc.elements.push_back(std::move(ed));
  doc.attlists.push_back(std::move(al));

  auto ss = dtd_translate(std::move(doc));
  const auto& s = ss.schemas()[0];

  REQUIRE(!s.simple_types().empty());
  const simple_type* st = nullptr;
  for (const auto& t : s.simple_types()) {
    if (!t.facets().enumeration.empty()) {
      st = &t;
      break;
    }
  }
  REQUIRE(st != nullptr);
  CHECK(st->facets().enumeration.size() == 3);
}

TEST_CASE("dtd translate: FIXED attribute has fixed_value",
          "[dtd_translator]") {
  dd::element_decl ed;
  ed.name = "doc";
  ed.content.kind = dd::content_kind::empty;

  dd::attlist_decl al;
  al.element_name = "doc";
  dd::attribute_def ad;
  ad.name = "version";
  ad.type = dd::attribute_type::cdata;
  ad.default_kind = dd::default_kind::fixed;
  ad.default_value = "1.0";
  al.attributes.push_back(std::move(ad));

  dd::document doc;
  doc.elements.push_back(std::move(ed));
  doc.attlists.push_back(std::move(al));

  auto ss = dtd_translate(std::move(doc));
  const auto& s = ss.schemas()[0];

  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "docType") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  REQUIRE(!ct->attributes().empty());
  CHECK(ct->attributes()[0].fixed_value.has_value());
  CHECK(ct->attributes()[0].fixed_value.value() == "1.0");
}
