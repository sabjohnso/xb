#include <xb/dtd_model.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb::dtd;

// -- content_particle ---------------------------------------------------------

TEST_CASE("dtd model: name particle with no quantifier", "[dtd_model]") {
  content_particle cp;
  cp.kind = particle_kind::name;
  cp.name = "chapter";
  cp.quantifier = quantifier::one;
  CHECK(cp.name == "chapter");
  CHECK(cp.quantifier == quantifier::one);
  CHECK(cp.children.empty());
}

TEST_CASE("dtd model: group particle with children", "[dtd_model]") {
  content_particle child1;
  child1.kind = particle_kind::name;
  child1.name = "a";
  child1.quantifier = quantifier::one;

  content_particle child2;
  child2.kind = particle_kind::name;
  child2.name = "b";
  child2.quantifier = quantifier::one;

  content_particle group;
  group.kind = particle_kind::sequence;
  group.quantifier = quantifier::one;
  group.children.push_back(std::move(child1));
  group.children.push_back(std::move(child2));

  CHECK(group.children.size() == 2);
  CHECK(group.children[0].name == "a");
  CHECK(group.children[1].name == "b");
}

TEST_CASE("dtd model: quantifier variants", "[dtd_model]") {
  content_particle cp;
  cp.kind = particle_kind::name;
  cp.name = "item";

  cp.quantifier = quantifier::zero_or_more;
  CHECK(cp.quantifier == quantifier::zero_or_more);

  cp.quantifier = quantifier::one_or_more;
  CHECK(cp.quantifier == quantifier::one_or_more);

  cp.quantifier = quantifier::optional;
  CHECK(cp.quantifier == quantifier::optional);
}

// -- content_spec -------------------------------------------------------------

TEST_CASE("dtd model: EMPTY content spec", "[dtd_model]") {
  content_spec cs;
  cs.kind = content_kind::empty;
  CHECK(cs.kind == content_kind::empty);
}

TEST_CASE("dtd model: ANY content spec", "[dtd_model]") {
  content_spec cs;
  cs.kind = content_kind::any;
  CHECK(cs.kind == content_kind::any);
}

TEST_CASE("dtd model: children content spec", "[dtd_model]") {
  content_particle cp;
  cp.kind = particle_kind::name;
  cp.name = "title";
  cp.quantifier = quantifier::one;

  content_spec cs;
  cs.kind = content_kind::children;
  cs.particle = std::move(cp);
  CHECK(cs.kind == content_kind::children);
  CHECK(cs.particle.has_value());
  CHECK(cs.particle->name == "title");
}

TEST_CASE("dtd model: mixed content spec", "[dtd_model]") {
  content_spec cs;
  cs.kind = content_kind::mixed;
  cs.mixed_names = {"em", "strong"};
  CHECK(cs.kind == content_kind::mixed);
  CHECK(cs.mixed_names.size() == 2);
}

// -- attribute_def ------------------------------------------------------------

TEST_CASE("dtd model: CDATA attribute", "[dtd_model]") {
  attribute_def ad;
  ad.name = "id";
  ad.type = attribute_type::cdata;
  ad.default_kind = default_kind::implied;
  CHECK(ad.name == "id");
  CHECK(ad.type == attribute_type::cdata);
  CHECK(ad.default_kind == default_kind::implied);
}

TEST_CASE("dtd model: enumeration attribute", "[dtd_model]") {
  attribute_def ad;
  ad.name = "type";
  ad.type = attribute_type::enumeration;
  ad.enum_values = {"fiction", "nonfiction"};
  ad.default_kind = default_kind::value;
  ad.default_value = "fiction";
  CHECK(ad.enum_values.size() == 2);
  CHECK(ad.default_value == "fiction");
}

TEST_CASE("dtd model: ID attribute", "[dtd_model]") {
  attribute_def ad;
  ad.name = "xml-id";
  ad.type = attribute_type::id;
  ad.default_kind = default_kind::required;
  CHECK(ad.type == attribute_type::id);
  CHECK(ad.default_kind == default_kind::required);
}

TEST_CASE("dtd model: FIXED attribute", "[dtd_model]") {
  attribute_def ad;
  ad.name = "version";
  ad.type = attribute_type::cdata;
  ad.default_kind = default_kind::fixed;
  ad.default_value = "1.0";
  CHECK(ad.default_kind == default_kind::fixed);
  CHECK(ad.default_value == "1.0");
}

// -- element_decl -------------------------------------------------------------

TEST_CASE("dtd model: element declaration", "[dtd_model]") {
  element_decl ed;
  ed.name = "book";
  ed.content.kind = content_kind::empty;
  CHECK(ed.name == "book");
  CHECK(ed.content.kind == content_kind::empty);
}

// -- attlist_decl -------------------------------------------------------------

TEST_CASE("dtd model: attlist declaration", "[dtd_model]") {
  attribute_def ad;
  ad.name = "id";
  ad.type = attribute_type::id;
  ad.default_kind = default_kind::implied;

  attlist_decl al;
  al.element_name = "book";
  al.attributes.push_back(std::move(ad));
  CHECK(al.element_name == "book");
  CHECK(al.attributes.size() == 1);
}

// -- entity_decl --------------------------------------------------------------

TEST_CASE("dtd model: parameter entity", "[dtd_model]") {
  entity_decl ent;
  ent.name = "inline";
  ent.is_parameter = true;
  ent.value = "em | strong";
  CHECK(ent.is_parameter == true);
  CHECK(ent.value == "em | strong");
}

TEST_CASE("dtd model: general entity", "[dtd_model]") {
  entity_decl ent;
  ent.name = "copyright";
  ent.is_parameter = false;
  ent.value = "&copy; 2026";
  CHECK(ent.is_parameter == false);
}

// -- document -----------------------------------------------------------------

TEST_CASE("dtd model: document collects declarations", "[dtd_model]") {
  document doc;

  element_decl ed;
  ed.name = "book";
  ed.content.kind = content_kind::empty;
  doc.elements.push_back(std::move(ed));

  attlist_decl al;
  al.element_name = "book";
  doc.attlists.push_back(std::move(al));

  entity_decl ent;
  ent.name = "inline";
  ent.is_parameter = true;
  ent.value = "em | strong";
  doc.entities.push_back(std::move(ent));

  CHECK(doc.elements.size() == 1);
  CHECK(doc.attlists.size() == 1);
  CHECK(doc.entities.size() == 1);
}
