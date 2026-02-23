#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

using namespace xb;

#ifndef XB_SCHEMA_DIR
#error "XB_SCHEMA_DIR must be defined"
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

static schema
parse_xsd(const std::string& xml) {
  expat_reader reader(xml);
  schema_parser parser;
  return parser.parse(reader);
}

// 1. Empty schema
TEST_CASE("schema_parser: empty schema", "[schema_parser]") {
  auto s =
      parse_xsd(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"/>)");
  CHECK(s.target_namespace().empty());
  CHECK(s.simple_types().empty());
  CHECK(s.complex_types().empty());
  CHECK(s.elements().empty());
}

// 2. Schema with targetNamespace
TEST_CASE("schema_parser: schema with targetNamespace", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
    </xs:schema>
  )");
  CHECK(s.target_namespace() == "urn:test");
}

// 3. Global element declaration
TEST_CASE("schema_parser: global element declaration", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="foo" type="xs:string"/>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  CHECK(s.elements()[0].name() == qname("urn:test", "foo"));
  CHECK(s.elements()[0].type_name() == qname(xs_ns, "string"));
}

// 4. Global attribute declaration
TEST_CASE("schema_parser: global attribute declaration", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
      <xs:attribute name="version" type="xs:string"/>
    </xs:schema>
  )");
  REQUIRE(s.attributes().size() == 1);
  CHECK(s.attributes()[0].name() == qname("", "version"));
  CHECK(s.attributes()[0].type_name() == qname(xs_ns, "string"));
}

// 5. Simple type with enumeration restriction
TEST_CASE("schema_parser: simple type with enumeration", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:simpleType name="SideType">
        <xs:restriction base="xs:string">
          <xs:enumeration value="Buy"/>
          <xs:enumeration value="Sell"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& st = s.simple_types()[0];
  CHECK(st.name() == qname("urn:test", "SideType"));
  CHECK(st.variety() == simple_type_variety::atomic);
  CHECK(st.base_type_name() == qname(xs_ns, "string"));
  REQUIRE(st.facets().enumeration.size() == 2);
  CHECK(st.facets().enumeration[0] == "Buy");
  CHECK(st.facets().enumeration[1] == "Sell");
}

// 6. Simple type with numeric facets
TEST_CASE("schema_parser: simple type with numeric facets", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="PriceType">
        <xs:restriction base="xs:decimal">
          <xs:minInclusive value="0"/>
          <xs:maxInclusive value="999999.99"/>
          <xs:totalDigits value="8"/>
          <xs:fractionDigits value="2"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& f = s.simple_types()[0].facets();
  CHECK(f.min_inclusive.value() == "0");
  CHECK(f.max_inclusive.value() == "999999.99");
  CHECK(f.total_digits.value() == 8);
  CHECK(f.fraction_digits.value() == 2);
}

// 7. Simple type list
TEST_CASE("schema_parser: simple type list", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="IntList">
        <xs:list itemType="xs:integer"/>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& st = s.simple_types()[0];
  CHECK(st.variety() == simple_type_variety::list);
  REQUIRE(st.item_type_name().has_value());
  CHECK(st.item_type_name().value() == qname(xs_ns, "integer"));
}

// 8. Simple type union
TEST_CASE("schema_parser: simple type union", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="StringOrInt">
        <xs:union memberTypes="xs:string xs:int"/>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& st = s.simple_types()[0];
  CHECK(st.variety() == simple_type_variety::union_type);
  REQUIRE(st.member_type_names().size() == 2);
  CHECK(st.member_type_names()[0] == qname(xs_ns, "string"));
  CHECK(st.member_type_names()[1] == qname(xs_ns, "int"));
}

// 9. Complex type with sequence of elements
TEST_CASE("schema_parser: complex type with sequence", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="PersonType">
        <xs:sequence>
          <xs:element name="name" type="xs:string"/>
          <xs:element name="age" type="xs:int"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  CHECK(ct.name() == qname("urn:test", "PersonType"));
  CHECK(ct.content().kind == content_kind::element_only);

  const auto& cc = std::get<complex_content>(ct.content().detail);
  REQUIRE(cc.content_model.has_value());
  CHECK(cc.content_model->compositor() == compositor_kind::sequence);
  REQUIRE(cc.content_model->particles().size() == 2);
}

// 10. Complex type with choice
TEST_CASE("schema_parser: complex type with choice", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="PaymentType">
        <xs:choice>
          <xs:element name="cash" type="xs:decimal"/>
          <xs:element name="card" type="xs:string"/>
        </xs:choice>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  REQUIRE(cc.content_model.has_value());
  CHECK(cc.content_model->compositor() == compositor_kind::choice);
  CHECK(cc.content_model->particles().size() == 2);
}

// 11. Complex type with all
TEST_CASE("schema_parser: complex type with all", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="AllType">
        <xs:all>
          <xs:element name="x" type="xs:string"/>
          <xs:element name="y" type="xs:string"/>
        </xs:all>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  REQUIRE(cc.content_model.has_value());
  CHECK(cc.content_model->compositor() == compositor_kind::all);
}

// 12. Complex type with attributes
TEST_CASE("schema_parser: complex type with attributes", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="ItemType">
        <xs:sequence>
          <xs:element name="name" type="xs:string"/>
        </xs:sequence>
        <xs:attribute name="id" type="xs:string" use="required"/>
        <xs:attribute name="lang" type="xs:language"/>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.attributes().size() == 2);
  CHECK(ct.attributes()[0].name == qname("", "id"));
  CHECK(ct.attributes()[0].required);
  CHECK(ct.attributes()[1].name == qname("", "lang"));
  CHECK_FALSE(ct.attributes()[1].required);
}

// 13. Complex type with simpleContent/extension
TEST_CASE("schema_parser: complex type with simpleContent extension",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="PriceType">
        <xs:simpleContent>
          <xs:extension base="xs:decimal">
            <xs:attribute name="currency" type="xs:string" use="required"/>
          </xs:extension>
        </xs:simpleContent>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  CHECK(ct.content().kind == content_kind::simple);

  const auto& sc = std::get<simple_content>(ct.content().detail);
  CHECK(sc.base_type_name == qname(xs_ns, "decimal"));
  CHECK(sc.derivation == derivation_method::extension);

  REQUIRE(ct.attributes().size() == 1);
  CHECK(ct.attributes()[0].name == qname("", "currency"));
  CHECK(ct.attributes()[0].required);
}

// 14. Complex type with complexContent/extension
TEST_CASE("schema_parser: complex type with complexContent extension",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="ExtendedType">
        <xs:complexContent>
          <xs:extension base="tns:BaseType">
            <xs:sequence>
              <xs:element name="extra" type="xs:string"/>
            </xs:sequence>
          </xs:extension>
        </xs:complexContent>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  CHECK(ct.content().kind == content_kind::element_only);

  const auto& cc = std::get<complex_content>(ct.content().detail);
  CHECK(cc.base_type_name == qname("urn:test", "BaseType"));
  CHECK(cc.derivation == derivation_method::extension);
  REQUIRE(cc.content_model.has_value());
}

// 15. Complex type with complexContent/restriction
TEST_CASE("schema_parser: complex type with complexContent restriction",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="RestrictedType">
        <xs:complexContent>
          <xs:restriction base="tns:BaseType">
            <xs:sequence>
              <xs:element name="name" type="xs:string"/>
            </xs:sequence>
          </xs:restriction>
        </xs:complexContent>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  CHECK(cc.derivation == derivation_method::restriction);
}

// 16. Named model group (xs:group)
TEST_CASE("schema_parser: named model group", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:group name="PersonGroup">
        <xs:sequence>
          <xs:element name="first" type="xs:string"/>
          <xs:element name="last" type="xs:string"/>
        </xs:sequence>
      </xs:group>
    </xs:schema>
  )");
  REQUIRE(s.model_group_defs().size() == 1);
  const auto& g = s.model_group_defs()[0];
  CHECK(g.name() == qname("urn:test", "PersonGroup"));
  CHECK(g.group().compositor() == compositor_kind::sequence);
  CHECK(g.group().particles().size() == 2);
}

// 17. Named attribute group
TEST_CASE("schema_parser: named attribute group", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:attributeGroup name="CommonAttrs">
        <xs:attribute name="id" type="xs:ID"/>
        <xs:attribute name="lang" type="xs:language"/>
      </xs:attributeGroup>
    </xs:schema>
  )");
  REQUIRE(s.attribute_group_defs().size() == 1);
  const auto& ag = s.attribute_group_defs()[0];
  CHECK(ag.name() == qname("urn:test", "CommonAttrs"));
  REQUIRE(ag.attributes().size() == 2);
}

// 18. Nested compositors (sequence containing choice)
TEST_CASE("schema_parser: nested compositors", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="NestedType">
        <xs:sequence>
          <xs:element name="header" type="xs:string"/>
          <xs:choice>
            <xs:element name="optA" type="xs:string"/>
            <xs:element name="optB" type="xs:int"/>
          </xs:choice>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  REQUIRE(cc.content_model.has_value());
  CHECK(cc.content_model->compositor() == compositor_kind::sequence);
  REQUIRE(cc.content_model->particles().size() == 2);

  // Second particle should be a nested model_group (choice)
  const auto& second = cc.content_model->particles()[1];
  REQUIRE(std::holds_alternative<std::unique_ptr<model_group>>(second.term));
  const auto& nested = std::get<std::unique_ptr<model_group>>(second.term);
  CHECK(nested->compositor() == compositor_kind::choice);
  CHECK(nested->particles().size() == 2);
}

// 19. Occurrence constraints
TEST_CASE("schema_parser: occurrence constraints", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="ListType">
        <xs:sequence>
          <xs:element name="item" type="xs:string"
                      minOccurs="0" maxOccurs="unbounded"/>
          <xs:element name="footer" type="xs:string"
                      minOccurs="0" maxOccurs="1"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  REQUIRE(cc.content_model.has_value());
  const auto& particles = cc.content_model->particles();
  REQUIRE(particles.size() == 2);

  CHECK(particles[0].occurs.min_occurs == 0);
  CHECK(particles[0].occurs.is_unbounded());

  CHECK(particles[1].occurs.min_occurs == 0);
  CHECK(particles[1].occurs.max_occurs == 1);
}

// 20. xs:any wildcard
TEST_CASE("schema_parser: xs:any wildcard", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="OpenType">
        <xs:sequence>
          <xs:any namespace="##other" processContents="lax"
                  minOccurs="0" maxOccurs="unbounded"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  const auto& particles = cc.content_model->particles();
  REQUIRE(particles.size() == 1);

  REQUIRE(std::holds_alternative<wildcard>(particles[0].term));
  const auto& w = std::get<wildcard>(particles[0].term);
  CHECK(w.ns_constraint == wildcard_ns_constraint::other);
  CHECK(w.process == process_contents::lax);
  CHECK(particles[0].occurs.min_occurs == 0);
  CHECK(particles[0].occurs.is_unbounded());
}

// 21. xs:anyAttribute
TEST_CASE("schema_parser: xs:anyAttribute", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="FlexType">
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
        <xs:anyAttribute namespace="##any" processContents="skip"/>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.attribute_wildcard().has_value());
  CHECK(ct.attribute_wildcard()->ns_constraint == wildcard_ns_constraint::any);
  CHECK(ct.attribute_wildcard()->process == process_contents::skip);
}

// 22. xs:import declarations
TEST_CASE("schema_parser: xs:import", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:import namespace="urn:other" schemaLocation="other.xsd"/>
    </xs:schema>
  )");
  REQUIRE(s.imports().size() == 1);
  CHECK(s.imports()[0].namespace_uri == "urn:other");
  CHECK(s.imports()[0].schema_location == "other.xsd");
}

// 23. xs:include declarations
TEST_CASE("schema_parser: xs:include", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:include schemaLocation="types.xsd"/>
    </xs:schema>
  )");
  REQUIRE(s.includes().size() == 1);
  CHECK(s.includes()[0].schema_location == "types.xsd");
}

// 24. Element with ref attribute
TEST_CASE("schema_parser: element with ref in compositor", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="RefType">
        <xs:sequence>
          <xs:element ref="tns:someElement"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  const auto& particles = cc.content_model->particles();
  REQUIRE(particles.size() == 1);
  REQUIRE(std::holds_alternative<element_ref>(particles[0].term));
  CHECK(std::get<element_ref>(particles[0].term).ref ==
        qname("urn:test", "someElement"));
}

// 25. Group ref inside a compositor
TEST_CASE("schema_parser: group ref in compositor", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="GroupRefType">
        <xs:sequence>
          <xs:group ref="tns:PersonGroup"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  const auto& particles = cc.content_model->particles();
  REQUIRE(particles.size() == 1);
  REQUIRE(std::holds_alternative<group_ref>(particles[0].term));
  CHECK(std::get<group_ref>(particles[0].term).ref ==
        qname("urn:test", "PersonGroup"));
}

// 26. Attribute group ref
TEST_CASE("schema_parser: attribute group ref", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="WithGroupRef">
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
        <xs:attributeGroup ref="tns:CommonAttrs"/>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.attribute_group_refs().size() == 1);
  CHECK(ct.attribute_group_refs()[0].ref == qname("urn:test", "CommonAttrs"));
}

// 27. Anonymous types
TEST_CASE("schema_parser: anonymous complex type on element",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="order">
        <xs:complexType>
          <xs:sequence>
            <xs:element name="item" type="xs:string"/>
          </xs:sequence>
        </xs:complexType>
      </xs:element>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  // Anonymous type gets synthetic name: element_name + "_type"
  CHECK(s.elements()[0].type_name() == qname("urn:test", "order_type"));

  REQUIRE(s.complex_types().size() == 1);
  CHECK(s.complex_types()[0].name() == qname("urn:test", "order_type"));
}

TEST_CASE("schema_parser: anonymous simple type on element",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:element name="status">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:enumeration value="active"/>
            <xs:enumeration value="inactive"/>
          </xs:restriction>
        </xs:simpleType>
      </xs:element>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  CHECK(s.elements()[0].type_name() == qname("urn:test", "status_type"));

  REQUIRE(s.simple_types().size() == 1);
  CHECK(s.simple_types()[0].name() == qname("urn:test", "status_type"));
}

// 28. Nillable and abstract elements
TEST_CASE("schema_parser: nillable and abstract elements", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:element name="nilElem" type="xs:string" nillable="true"/>
      <xs:element name="absElem" type="xs:string" abstract="true"/>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 2);
  CHECK(s.elements()[0].nillable());
  CHECK_FALSE(s.elements()[0].abstract());
  CHECK_FALSE(s.elements()[1].nillable());
  CHECK(s.elements()[1].abstract());
}

// 29. Substitution group
TEST_CASE("schema_parser: substitution group", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="special" type="xs:string"
                  substitutionGroup="tns:base"/>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  REQUIRE(s.elements()[0].substitution_group().has_value());
  CHECK(s.elements()[0].substitution_group().value() ==
        qname("urn:test", "base"));
}

// 30. Default and fixed values
TEST_CASE("schema_parser: default and fixed values", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:element name="status" type="xs:string" default="active"/>
      <xs:element name="version" type="xs:string" fixed="1.0"/>
      <xs:attribute name="lang" type="xs:language" default="en"/>
      <xs:attribute name="encoding" type="xs:string" fixed="UTF-8"/>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 2);
  CHECK(s.elements()[0].default_value().value() == "active");
  CHECK_FALSE(s.elements()[0].fixed_value().has_value());
  CHECK(s.elements()[1].fixed_value().value() == "1.0");
  CHECK_FALSE(s.elements()[1].default_value().has_value());

  REQUIRE(s.attributes().size() == 2);
  CHECK(s.attributes()[0].default_value().value() == "en");
  CHECK(s.attributes()[1].fixed_value().value() == "UTF-8");
}

// 31. Smoke test: parse xb-typemap.xsd from disk
TEST_CASE("schema_parser: parse xb-typemap.xsd from disk", "[schema_parser]") {
  // Read the file into a string
  std::string path = STRINGIFY(XB_SCHEMA_DIR) "/xb-typemap.xsd";
  std::FILE* f = std::fopen(path.c_str(), "r");
  REQUIRE(f != nullptr);
  std::fseek(f, 0, SEEK_END);
  auto size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::string content(static_cast<std::size_t>(size), '\0');
  auto bytes_read = std::fread(content.data(), 1, content.size(), f);
  content.resize(bytes_read);
  std::fclose(f);

  auto s = parse_xsd(content);

  // xb-typemap.xsd defines a typemap schema - verify basic structure
  CHECK_FALSE(s.target_namespace().empty());
  // Should have at least one element or complex type
  CHECK((!s.elements().empty() || !s.complex_types().empty()));
}

// Additional facets
TEST_CASE("schema_parser: simple type with pattern facet", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="PhoneType">
        <xs:restriction base="xs:string">
          <xs:pattern value="\d{3}-\d{4}"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  CHECK(s.simple_types()[0].facets().pattern.value() == "\\d{3}-\\d{4}");
}

TEST_CASE("schema_parser: simple type with length facets", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="Code">
        <xs:restriction base="xs:string">
          <xs:minLength value="1"/>
          <xs:maxLength value="10"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& f = s.simple_types()[0].facets();
  CHECK(f.min_length.value() == 1);
  CHECK(f.max_length.value() == 10);
}

TEST_CASE("schema_parser: simple type with exclusive bounds",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="RangeType">
        <xs:restriction base="xs:int">
          <xs:minExclusive value="0"/>
          <xs:maxExclusive value="100"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:schema>
  )");
  REQUIRE(s.simple_types().size() == 1);
  const auto& f = s.simple_types()[0].facets();
  CHECK(f.min_exclusive.value() == "0");
  CHECK(f.max_exclusive.value() == "100");
}

// Complex type with attribute default/fixed
TEST_CASE("schema_parser: complex type attribute with default/fixed",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="ConfigType">
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
        <xs:attribute name="version" type="xs:string" default="1.0"/>
        <xs:attribute name="encoding" type="xs:string" fixed="UTF-8"/>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  REQUIRE(s.complex_types()[0].attributes().size() == 2);
  CHECK(s.complex_types()[0].attributes()[0].default_value.value() == "1.0");
  CHECK(s.complex_types()[0].attributes()[1].fixed_value.value() == "UTF-8");
}

// ===== XSD 1.1: Open Content =====

TEST_CASE("schema_parser: openContent interleave mode", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="FlexType">
        <xs:openContent mode="interleave">
          <xs:any namespace="##other" processContents="lax"/>
        </xs:openContent>
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.open_content_value().has_value());
  CHECK(ct.open_content_value()->mode == open_content_mode::interleave);
  CHECK(ct.open_content_value()->wc.ns_constraint ==
        wildcard_ns_constraint::other);
  CHECK(ct.open_content_value()->wc.process == process_contents::lax);
}

TEST_CASE("schema_parser: openContent suffix mode", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="SuffixType">
        <xs:openContent mode="suffix">
          <xs:any processContents="skip"/>
        </xs:openContent>
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.open_content_value().has_value());
  CHECK(ct.open_content_value()->mode == open_content_mode::suffix);
  CHECK(ct.open_content_value()->wc.process == process_contents::skip);
}

TEST_CASE("schema_parser: openContent none mode", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="ClosedType">
        <xs:openContent mode="none"/>
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.open_content_value().has_value());
  CHECK(ct.open_content_value()->mode == open_content_mode::none);
}

TEST_CASE("schema_parser: openContent default mode is interleave",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="DefaultMode">
        <xs:openContent>
          <xs:any/>
        </xs:openContent>
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& ct = s.complex_types()[0];
  REQUIRE(ct.open_content_value().has_value());
  CHECK(ct.open_content_value()->mode == open_content_mode::interleave);
}

TEST_CASE("schema_parser: defaultOpenContent at schema level",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:defaultOpenContent>
        <xs:any namespace="##other" processContents="lax"/>
      </xs:defaultOpenContent>
      <xs:complexType name="MyType">
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.default_open_content().has_value());
  CHECK(s.default_open_content()->mode == open_content_mode::interleave);
  CHECK(s.default_open_content()->wc.ns_constraint ==
        wildcard_ns_constraint::other);
  CHECK(s.default_open_content()->wc.process == process_contents::lax);
  CHECK_FALSE(s.default_open_content_applies_to_empty());
}

TEST_CASE("schema_parser: defaultOpenContent appliesToEmpty=true",
          "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:defaultOpenContent appliesToEmpty="true">
        <xs:any/>
      </xs:defaultOpenContent>
    </xs:schema>
  )");
  REQUIRE(s.default_open_content().has_value());
  CHECK(s.default_open_content_applies_to_empty());
}

TEST_CASE("schema_parser: defaultOpenContent suffix mode", "[schema_parser]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:defaultOpenContent mode="suffix">
        <xs:any/>
      </xs:defaultOpenContent>
    </xs:schema>
  )");
  REQUIRE(s.default_open_content().has_value());
  CHECK(s.default_open_content()->mode == open_content_mode::suffix);
}

// ===== XSD 1.1: Conditional Type Assignment =====

TEST_CASE("schema_parser: global element with xs:alternative children",
          "[schema_parser][cta]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="vehicle" type="tns:vehicleType">
        <xs:alternative test="@kind = 'car'" type="tns:carType"/>
        <xs:alternative test="@kind = 'truck'" type="tns:truckType"/>
        <xs:alternative type="tns:vehicleType"/>
      </xs:element>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  const auto& elem = s.elements()[0];
  CHECK(elem.name() == qname("urn:test", "vehicle"));
  CHECK(elem.type_name() == qname("urn:test", "vehicleType"));

  REQUIRE(elem.type_alternatives().size() == 3);
  CHECK(elem.type_alternatives()[0].test.value() == "@kind = 'car'");
  CHECK(elem.type_alternatives()[0].type_name == qname("urn:test", "carType"));
  CHECK(elem.type_alternatives()[1].test.value() == "@kind = 'truck'");
  CHECK(elem.type_alternatives()[1].type_name ==
        qname("urn:test", "truckType"));
  CHECK_FALSE(elem.type_alternatives()[2].test.has_value());
  CHECK(elem.type_alternatives()[2].type_name ==
        qname("urn:test", "vehicleType"));
}

TEST_CASE("schema_parser: inline element with xs:alternative children",
          "[schema_parser][cta]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="ContainerType">
        <xs:sequence>
          <xs:element name="item" type="tns:baseType">
            <xs:alternative test="@kind = 'a'" type="tns:aType"/>
            <xs:alternative test="@kind = 'b'" type="tns:bType"/>
          </xs:element>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");
  REQUIRE(s.complex_types().size() == 1);
  const auto& cc =
      std::get<complex_content>(s.complex_types()[0].content().detail);
  REQUIRE(cc.content_model.has_value());
  REQUIRE(cc.content_model->particles().size() == 1);

  const auto& ed =
      std::get<element_decl>(cc.content_model->particles()[0].term);
  REQUIRE(ed.type_alternatives().size() == 2);
  CHECK(ed.type_alternatives()[0].test.value() == "@kind = 'a'");
  CHECK(ed.type_alternatives()[0].type_name == qname("urn:test", "aType"));
  CHECK(ed.type_alternatives()[1].test.value() == "@kind = 'b'");
  CHECK(ed.type_alternatives()[1].type_name == qname("urn:test", "bType"));
}

TEST_CASE("schema_parser: element without alternatives has empty vector",
          "[schema_parser][cta]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:element name="simple" type="xs:string"/>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  CHECK(s.elements()[0].type_alternatives().empty());
}

TEST_CASE("schema_parser: default alternative has nullopt test",
          "[schema_parser][cta]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="thing" type="tns:baseType">
        <xs:alternative test="@x = '1'" type="tns:xType"/>
        <xs:alternative type="tns:baseType"/>
      </xs:element>
    </xs:schema>
  )");
  REQUIRE(s.elements().size() == 1);
  REQUIRE(s.elements()[0].type_alternatives().size() == 2);
  CHECK(s.elements()[0].type_alternatives()[0].test.has_value());
  CHECK_FALSE(s.elements()[0].type_alternatives()[1].test.has_value());
}
