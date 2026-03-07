#include <xb/xsd_writer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

TEST_CASE("xsd_write: empty schema", "[xsd_writer]") {
  schema s;
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:schema") != std::string::npos);
  CHECK(result.find("xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"") !=
        std::string::npos);
}

TEST_CASE("xsd_write: schema with targetNamespace", "[xsd_writer]") {
  schema s;
  s.set_target_namespace("http://example.com/ns");
  auto result = xsd_write_string(s);
  CHECK(result.find("targetNamespace=\"http://example.com/ns\"") !=
        std::string::npos);
}

TEST_CASE("xsd_write: import and include", "[xsd_writer]") {
  schema s;
  s.add_import(schema_import{"http://other.com/ns", "other.xsd"});
  s.add_include(schema_include{"common.xsd"});
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:import") != std::string::npos);
  CHECK(result.find("namespace=\"http://other.com/ns\"") != std::string::npos);
  CHECK(result.find("schemaLocation=\"other.xsd\"") != std::string::npos);
  CHECK(result.find("<xs:include") != std::string::npos);
  CHECK(result.find("schemaLocation=\"common.xsd\"") != std::string::npos);
}

TEST_CASE("xsd_write: global element with built-in type", "[xsd_writer]") {
  schema s;
  s.add_element(element_decl(qname("", "greeting"), qname(xs_ns, "string")));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:element") != std::string::npos);
  CHECK(result.find("name=\"greeting\"") != std::string::npos);
  CHECK(result.find("type=\"xs:string\"") != std::string::npos);
}

TEST_CASE("xsd_write: simple type with restriction", "[xsd_writer]") {
  schema s;
  facet_set facets;
  facets.min_inclusive = "0";
  facets.max_inclusive = "100";
  s.add_simple_type(simple_type(qname("", "percentage"),
                                simple_type_variety::atomic,
                                qname(xs_ns, "integer"), facets));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:simpleType") != std::string::npos);
  CHECK(result.find("name=\"percentage\"") != std::string::npos);
  CHECK(result.find("<xs:restriction") != std::string::npos);
  CHECK(result.find("base=\"xs:integer\"") != std::string::npos);
  CHECK(result.find("<xs:minInclusive") != std::string::npos);
  CHECK(result.find("value=\"0\"") != std::string::npos);
  CHECK(result.find("<xs:maxInclusive") != std::string::npos);
  CHECK(result.find("value=\"100\"") != std::string::npos);
}

TEST_CASE("xsd_write: simple type with enumeration", "[xsd_writer]") {
  schema s;
  facet_set facets;
  facets.enumeration = {"red", "green", "blue"};
  s.add_simple_type(simple_type(qname("", "color"), simple_type_variety::atomic,
                                qname(xs_ns, "string"), facets));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:enumeration value=\"red\"") != std::string::npos);
  CHECK(result.find("<xs:enumeration value=\"green\"") != std::string::npos);
  CHECK(result.find("<xs:enumeration value=\"blue\"") != std::string::npos);
}

TEST_CASE("xsd_write: simple type list", "[xsd_writer]") {
  schema s;
  s.add_simple_type(simple_type(qname("", "intList"), simple_type_variety::list,
                                qname(xs_ns, "anySimpleType"), {},
                                qname(xs_ns, "integer")));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:list") != std::string::npos);
  CHECK(result.find("itemType=\"xs:integer\"") != std::string::npos);
}

TEST_CASE("xsd_write: simple type union", "[xsd_writer]") {
  schema s;
  s.add_simple_type(
      simple_type(qname("", "stringOrInt"), simple_type_variety::union_type,
                  qname(xs_ns, "anySimpleType"), {}, std::nullopt,
                  {qname(xs_ns, "string"), qname(xs_ns, "integer")}));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:union") != std::string::npos);
  CHECK(result.find("memberTypes=\"xs:string xs:integer\"") !=
        std::string::npos);
}

TEST_CASE("xsd_write: complex type with sequence", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(
      particle(element_decl(qname("", "name"), qname(xs_ns, "string"))));
  mg.add_particle(
      particle(element_decl(qname("", "age"), qname(xs_ns, "int"))));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "personType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:complexType") != std::string::npos);
  CHECK(result.find("name=\"personType\"") != std::string::npos);
  CHECK(result.find("<xs:sequence") != std::string::npos);
  CHECK(result.find("name=\"name\"") != std::string::npos);
  CHECK(result.find("type=\"xs:string\"") != std::string::npos);
  CHECK(result.find("name=\"age\"") != std::string::npos);
  CHECK(result.find("type=\"xs:int\"") != std::string::npos);
}

TEST_CASE("xsd_write: complex type with choice", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::choice);
  mg.add_particle(
      particle(element_decl(qname("", "cat"), qname(xs_ns, "string"))));
  mg.add_particle(
      particle(element_decl(qname("", "dog"), qname(xs_ns, "string"))));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "petType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:choice") != std::string::npos);
}

TEST_CASE("xsd_write: complex type with all", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::all);
  mg.add_particle(particle(element_decl(qname("", "x"), qname(xs_ns, "int"))));
  mg.add_particle(particle(element_decl(qname("", "y"), qname(xs_ns, "int"))));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "pointType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:all") != std::string::npos);
}

TEST_CASE("xsd_write: occurrence attributes", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(
      particle(element_decl(qname("", "item"), qname(xs_ns, "string")),
               occurrence{0, unbounded}));
  mg.add_particle(
      particle(element_decl(qname("", "note"), qname(xs_ns, "string")),
               occurrence{0, 1}));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "listType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("minOccurs=\"0\"") != std::string::npos);
  CHECK(result.find("maxOccurs=\"unbounded\"") != std::string::npos);
}

TEST_CASE("xsd_write: attributes", "[xsd_writer]") {
  schema s;
  content_type ct(content_kind::empty, std::monostate{});
  std::vector<attribute_use> attrs = {
      attribute_use{qname("", "id"), qname(xs_ns, "ID"), true, std::nullopt,
                    std::nullopt},
      attribute_use{qname("", "class"), qname(xs_ns, "string"), false,
                    std::string("default-class"), std::nullopt},
      attribute_use{qname("", "version"), qname(xs_ns, "string"), false,
                    std::nullopt, std::string("1.0")},
  };
  s.add_complex_type(complex_type(qname("", "elemType"), false, false,
                                  std::move(ct), std::move(attrs)));
  auto result = xsd_write_string(s);
  CHECK(result.find("name=\"id\"") != std::string::npos);
  CHECK(result.find("type=\"xs:ID\"") != std::string::npos);
  CHECK(result.find("use=\"required\"") != std::string::npos);
  CHECK(result.find("default=\"default-class\"") != std::string::npos);
  CHECK(result.find("fixed=\"1.0\"") != std::string::npos);
}

TEST_CASE("xsd_write: mixed content", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(
      particle(element_decl(qname("", "b"), qname(xs_ns, "string"))));
  content_type ct(
      content_kind::mixed,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "mixedType"), false, true, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("mixed=\"true\"") != std::string::npos);
}

TEST_CASE("xsd_write: simple content", "[xsd_writer]") {
  schema s;
  content_type ct(
      content_kind::simple,
      simple_content{qname(xs_ns, "string"), derivation_method::extension, {}});
  std::vector<attribute_use> attrs = {
      attribute_use{qname("", "lang"), qname(xs_ns, "string"), false,
                    std::nullopt, std::nullopt}};
  s.add_complex_type(complex_type(qname("", "textType"), false, false,
                                  std::move(ct), std::move(attrs)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:simpleContent") != std::string::npos);
  CHECK(result.find("<xs:extension") != std::string::npos);
  CHECK(result.find("base=\"xs:string\"") != std::string::npos);
}

TEST_CASE("xsd_write: complex content extension", "[xsd_writer]") {
  schema s;
  s.set_target_namespace("http://example.com/ns");
  model_group mg(compositor_kind::sequence);
  mg.add_particle(
      particle(element_decl(qname("", "email"), qname(xs_ns, "string"))));
  content_type ct(content_kind::element_only,
                  complex_content(qname("http://example.com/ns", "personType"),
                                  derivation_method::extension, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("http://example.com/ns", "employeeType"), false, false,
                   std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:complexContent") != std::string::npos);
  CHECK(result.find("<xs:extension") != std::string::npos);
  CHECK(result.find("base=\"tns:personType\"") != std::string::npos);
}

TEST_CASE("xsd_write: element ref", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(particle(element_ref{qname("", "name")}));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "refType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("ref=\"name\"") != std::string::npos);
}

TEST_CASE("xsd_write: group ref", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(particle(group_ref{qname("", "commonFields")}));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "grpType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:group") != std::string::npos);
  CHECK(result.find("ref=\"commonFields\"") != std::string::npos);
}

TEST_CASE("xsd_write: wildcard particle", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(particle(
      wildcard{wildcard_ns_constraint::any, {}, process_contents::lax, {}, {}},
      occurrence{0, unbounded}));
  content_type ct(
      content_kind::element_only,
      complex_content(qname(), derivation_method::restriction, std::move(mg)));
  s.add_complex_type(
      complex_type(qname("", "openType"), false, false, std::move(ct)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:any") != std::string::npos);
  CHECK(result.find("processContents=\"lax\"") != std::string::npos);
}

TEST_CASE("xsd_write: model group def", "[xsd_writer]") {
  schema s;
  model_group mg(compositor_kind::sequence);
  mg.add_particle(
      particle(element_decl(qname("", "a"), qname(xs_ns, "string"))));
  s.add_model_group_def(model_group_def(qname("", "myGroup"), std::move(mg)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:group") != std::string::npos);
  CHECK(result.find("name=\"myGroup\"") != std::string::npos);
  CHECK(result.find("<xs:sequence") != std::string::npos);
}

TEST_CASE("xsd_write: attribute group def", "[xsd_writer]") {
  schema s;
  std::vector<attribute_use> attrs = {attribute_use{
      qname("", "x"), qname(xs_ns, "int"), false, std::nullopt, std::nullopt}};
  s.add_attribute_group_def(
      attribute_group_def(qname("", "posAttrs"), std::move(attrs)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:attributeGroup") != std::string::npos);
  CHECK(result.find("name=\"posAttrs\"") != std::string::npos);
  CHECK(result.find("name=\"x\"") != std::string::npos);
}

TEST_CASE("xsd_write: abstract and nillable element", "[xsd_writer]") {
  schema s;
  s.add_element(element_decl(qname("", "item"), qname(xs_ns, "string"),
                             /*nillable=*/true, /*abstract=*/true));
  auto result = xsd_write_string(s);
  CHECK(result.find("nillable=\"true\"") != std::string::npos);
  CHECK(result.find("abstract=\"true\"") != std::string::npos);
}

TEST_CASE("xsd_write: attribute group ref on complex type", "[xsd_writer]") {
  schema s;
  content_type ct(content_kind::empty, std::monostate{});
  s.add_complex_type(
      complex_type(qname("", "agType"), false, false, std::move(ct), {},
                   {attribute_group_ref{qname("", "commonAttrs")}}));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:attributeGroup") != std::string::npos);
  CHECK(result.find("ref=\"commonAttrs\"") != std::string::npos);
}

TEST_CASE("xsd_write: attribute wildcard", "[xsd_writer]") {
  schema s;
  content_type ct(content_kind::empty, std::monostate{});
  wildcard aw{
      wildcard_ns_constraint::other, {}, process_contents::skip, {}, {}};
  s.add_complex_type(complex_type(qname("", "awType"), false, false,
                                  std::move(ct), {}, {}, std::move(aw)));
  auto result = xsd_write_string(s);
  CHECK(result.find("<xs:anyAttribute") != std::string::npos);
  CHECK(result.find("namespace=\"##other\"") != std::string::npos);
  CHECK(result.find("processContents=\"skip\"") != std::string::npos);
}
