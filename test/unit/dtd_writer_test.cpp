#include <xb/dtd_writer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace dd = xb::dtd;

TEST_CASE("dtd_write: EMPTY element", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "br";
  ed.content.kind = dd::content_kind::empty;
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT br EMPTY>\n");
}

TEST_CASE("dtd_write: ANY element", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "doc";
  ed.content.kind = dd::content_kind::any;
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT doc ANY>\n");
}

TEST_CASE("dtd_write: PCDATA element", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "title";
  ed.content.kind = dd::content_kind::mixed;
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT title (#PCDATA)>\n");
}

TEST_CASE("dtd_write: mixed content", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "p";
  ed.content.kind = dd::content_kind::mixed;
  ed.content.mixed_names = {"em", "strong"};
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT p (#PCDATA | em | strong)*>\n");
}

TEST_CASE("dtd_write: sequence content", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "person";
  ed.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::sequence;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "name", dd::quantifier::one, {}});
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "email", dd::quantifier::optional, {}});
  ed.content.particle = std::move(cp);
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT person (name, email?)>\n");
}

TEST_CASE("dtd_write: choice content with quantifier", "[dtd_writer]") {
  dd::document doc;
  dd::element_decl ed;
  ed.name = "pet";
  ed.content.kind = dd::content_kind::children;
  dd::content_particle cp;
  cp.kind = dd::particle_kind::choice;
  cp.quant = dd::quantifier::one_or_more;
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "cat", dd::quantifier::one, {}});
  cp.children.push_back(dd::content_particle{
      dd::particle_kind::name, "dog", dd::quantifier::one, {}});
  ed.content.particle = std::move(cp);
  doc.elements.push_back(std::move(ed));

  auto result = dtd_write(doc);
  CHECK(result == "<!ELEMENT pet (cat | dog)+>\n");
}

TEST_CASE("dtd_write: ATTLIST with CDATA #REQUIRED", "[dtd_writer]") {
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

  auto result = dtd_write(doc);
  CHECK(result.find("<!ATTLIST img") != std::string::npos);
  CHECK(result.find("src CDATA #REQUIRED") != std::string::npos);
}

TEST_CASE("dtd_write: ATTLIST with #FIXED", "[dtd_writer]") {
  dd::document doc;

  dd::attlist_decl al;
  al.element_name = "html";
  al.attributes.push_back(dd::attribute_def{"version",
                                            dd::attribute_type::cdata,
                                            {},
                                            dd::default_kind::fixed,
                                            "1.0"});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_write(doc);
  CHECK(result.find("#FIXED \"1.0\"") != std::string::npos);
}

TEST_CASE("dtd_write: ATTLIST with enumeration", "[dtd_writer]") {
  dd::document doc;

  dd::attlist_decl al;
  al.element_name = "td";
  al.attributes.push_back(dd::attribute_def{"align",
                                            dd::attribute_type::enumeration,
                                            {"left", "right", "center"},
                                            dd::default_kind::implied,
                                            ""});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_write(doc);
  CHECK(result.find("(left | right | center)") != std::string::npos);
  CHECK(result.find("#IMPLIED") != std::string::npos);
}

TEST_CASE("dtd_write: ATTLIST with default value", "[dtd_writer]") {
  dd::document doc;

  dd::attlist_decl al;
  al.element_name = "p";
  al.attributes.push_back(dd::attribute_def{"class",
                                            dd::attribute_type::cdata,
                                            {},
                                            dd::default_kind::value,
                                            "normal"});
  doc.attlists.push_back(std::move(al));

  auto result = dtd_write(doc);
  CHECK(result.find("\"normal\"") != std::string::npos);
}
