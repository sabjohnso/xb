#include <xb/schema.hpp>

#include <catch2/catch_test_macros.hpp>

static const std::string xs = "http://www.w3.org/2001/XMLSchema";
static const std::string tns = "urn:test";

TEST_CASE("schema default is empty", "[schema]") {
  xb::schema s;
  CHECK(s.target_namespace().empty());
  CHECK(s.simple_types().empty());
  CHECK(s.complex_types().empty());
  CHECK(s.elements().empty());
  CHECK(s.attributes().empty());
  CHECK(s.model_group_defs().empty());
  CHECK(s.attribute_group_defs().empty());
  CHECK(s.imports().empty());
  CHECK(s.includes().empty());
}

TEST_CASE("schema set_target_namespace", "[schema]") {
  xb::schema s;
  s.set_target_namespace(tns);
  CHECK(s.target_namespace() == tns);
}

TEST_CASE("schema add_simple_type", "[schema]") {
  xb::schema s;
  s.add_simple_type(xb::simple_type(xb::qname(tns, "SideType"),
                                    xb::simple_type_variety::atomic,
                                    xb::qname(xs, "string")));

  CHECK(s.simple_types().size() == 1);
  CHECK(s.simple_types()[0].name() == xb::qname(tns, "SideType"));
}

TEST_CASE("schema add_complex_type", "[schema]") {
  xb::schema s;
  xb::content_type ct;
  s.add_complex_type(xb::complex_type(xb::qname(tns, "PersonType"), false,
                                      false, std::move(ct)));

  CHECK(s.complex_types().size() == 1);
  CHECK(s.complex_types()[0].name() == xb::qname(tns, "PersonType"));
}

TEST_CASE("schema add_element", "[schema]") {
  xb::schema s;
  s.add_element(
      xb::element_decl(xb::qname(tns, "order"), xb::qname(tns, "OrderType")));

  CHECK(s.elements().size() == 1);
  CHECK(s.elements()[0].name() == xb::qname(tns, "order"));
}

TEST_CASE("schema add_attribute", "[schema]") {
  xb::schema s;
  s.add_attribute(
      xb::attribute_decl(xb::qname("", "version"), xb::qname(xs, "string")));

  CHECK(s.attributes().size() == 1);
  CHECK(s.attributes()[0].name() == xb::qname("", "version"));
}

TEST_CASE("schema add_model_group_def", "[schema]") {
  xb::schema s;
  xb::model_group mg(xb::compositor_kind::sequence);
  s.add_model_group_def(
      xb::model_group_def(xb::qname(tns, "myGroup"), std::move(mg)));

  CHECK(s.model_group_defs().size() == 1);
  CHECK(s.model_group_defs()[0].name() == xb::qname(tns, "myGroup"));
}

TEST_CASE("schema add_attribute_group_def", "[schema]") {
  xb::schema s;
  s.add_attribute_group_def(xb::attribute_group_def(xb::qname(tns, "attrs")));

  CHECK(s.attribute_group_defs().size() == 1);
  CHECK(s.attribute_group_defs()[0].name() == xb::qname(tns, "attrs"));
}

TEST_CASE("schema add_import", "[schema]") {
  xb::schema s;
  s.add_import(xb::schema_import{"urn:other", "other.xsd"});

  CHECK(s.imports().size() == 1);
  CHECK(s.imports()[0].namespace_uri == "urn:other");
  CHECK(s.imports()[0].schema_location == "other.xsd");
}

TEST_CASE("schema add_include", "[schema]") {
  xb::schema s;
  s.add_include(xb::schema_include{"types.xsd"});

  CHECK(s.includes().size() == 1);
  CHECK(s.includes()[0].schema_location == "types.xsd");
}

TEST_CASE("schema multiple components", "[schema]") {
  xb::schema s;
  s.set_target_namespace(tns);

  s.add_simple_type(xb::simple_type(xb::qname(tns, "A"),
                                    xb::simple_type_variety::atomic,
                                    xb::qname(xs, "string")));
  s.add_simple_type(xb::simple_type(xb::qname(tns, "B"),
                                    xb::simple_type_variety::atomic,
                                    xb::qname(xs, "int")));

  s.add_element(xb::element_decl(xb::qname(tns, "foo"), xb::qname(tns, "A")));
  s.add_element(xb::element_decl(xb::qname(tns, "bar"), xb::qname(tns, "B")));

  CHECK(s.simple_types().size() == 2);
  CHECK(s.elements().size() == 2);
}
