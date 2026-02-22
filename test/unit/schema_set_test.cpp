#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

static schema
parse_xsd(const std::string& xml) {
  expat_reader reader(xml);
  schema_parser parser;
  return parser.parse(reader);
}

// 1. Single schema — add, resolve, find types/elements by QName
TEST_CASE("schema_set: single schema lookup", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:simpleType name="SideType">
        <xs:restriction base="xs:string">
          <xs:enumeration value="Buy"/>
          <xs:enumeration value="Sell"/>
        </xs:restriction>
      </xs:simpleType>
      <xs:element name="order" type="xs:string"/>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto* st = ss.find_simple_type(qname("urn:test", "SideType"));
  REQUIRE(st != nullptr);
  CHECK(st->name() == qname("urn:test", "SideType"));
  CHECK(st->facets().enumeration.size() == 2);

  auto* elem = ss.find_element(qname("urn:test", "order"));
  REQUIRE(elem != nullptr);
  CHECK(elem->name() == qname("urn:test", "order"));

  CHECK(ss.find_simple_type(qname("urn:test", "NonExistent")) == nullptr);
  CHECK(ss.find_element(qname("urn:test", "NonExistent")) == nullptr);
}

// 2. Built-in XSD type references are not flagged as unresolved
TEST_CASE("schema_set: built-in XSD types resolve", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:element name="name" type="xs:string"/>
      <xs:element name="count" type="xs:int"/>
      <xs:element name="price" type="xs:decimal"/>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  // Should not throw — xs:string, xs:int, xs:decimal are built-in
  CHECK_NOTHROW(ss.resolve());
}

// 3. Unresolved type reference — resolve() throws
TEST_CASE("schema_set: unresolved type reference throws", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:element name="foo" type="tns:MissingType"/>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  CHECK_THROWS_AS(ss.resolve(), std::runtime_error);
}

// 4. Unresolved element reference — resolve() throws
TEST_CASE("schema_set: unresolved element reference throws", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="RefType">
        <xs:sequence>
          <xs:element ref="tns:missingElement"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  CHECK_THROWS_AS(ss.resolve(), std::runtime_error);
}

// 5. Two schemas, cross-namespace reference
TEST_CASE("schema_set: cross-namespace reference", "[schema_set]") {
  auto s1 = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:types">
      <xs:simpleType name="NameType">
        <xs:restriction base="xs:string"/>
      </xs:simpleType>
    </xs:schema>
  )");

  auto s2 = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:main" xmlns:t="urn:types">
      <xs:import namespace="urn:types"/>
      <xs:element name="person" type="t:NameType"/>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  CHECK_NOTHROW(ss.resolve());

  auto* st = ss.find_simple_type(qname("urn:types", "NameType"));
  REQUIRE(st != nullptr);
}

// 6. Duplicate type name — resolve() throws
TEST_CASE("schema_set: duplicate type name throws", "[schema_set]") {
  auto s1 = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="Dup">
        <xs:restriction base="xs:string"/>
      </xs:simpleType>
    </xs:schema>
  )");

  auto s2 = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:simpleType name="Dup">
        <xs:restriction base="xs:int"/>
      </xs:simpleType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  CHECK_THROWS_AS(ss.resolve(), std::runtime_error);
}

// 7. End-to-end: parse two .xsd strings, add to schema_set, resolve, find
TEST_CASE("schema_set: end-to-end with parser", "[schema_set]") {
  auto types_schema = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:types" xmlns:t="urn:types">
      <xs:simpleType name="CurrencyType">
        <xs:restriction base="xs:string">
          <xs:enumeration value="USD"/>
          <xs:enumeration value="EUR"/>
          <xs:enumeration value="GBP"/>
        </xs:restriction>
      </xs:simpleType>
      <xs:complexType name="MoneyType">
        <xs:simpleContent>
          <xs:extension base="xs:decimal">
            <xs:attribute name="currency" type="t:CurrencyType" use="required"/>
          </xs:extension>
        </xs:simpleContent>
      </xs:complexType>
    </xs:schema>
  )");

  auto main_schema = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:orders" xmlns:t="urn:types">
      <xs:import namespace="urn:types"/>
      <xs:element name="price" type="t:MoneyType"/>
      <xs:element name="total" type="t:MoneyType"/>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(types_schema));
  ss.add(std::move(main_schema));
  CHECK_NOTHROW(ss.resolve());

  // Find types from the types schema
  auto* ct = ss.find_complex_type(qname("urn:types", "MoneyType"));
  REQUIRE(ct != nullptr);
  CHECK(ct->content().kind == content_kind::simple);

  // Find elements from the main schema
  auto* price = ss.find_element(qname("urn:orders", "price"));
  REQUIRE(price != nullptr);
  CHECK(price->type_name() == qname("urn:types", "MoneyType"));

  auto* total = ss.find_element(qname("urn:orders", "total"));
  REQUIRE(total != nullptr);
}

// Complex type lookup
TEST_CASE("schema_set: find complex type", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test">
      <xs:complexType name="PersonType">
        <xs:sequence>
          <xs:element name="name" type="xs:string"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto* ct = ss.find_complex_type(qname("urn:test", "PersonType"));
  REQUIRE(ct != nullptr);
  CHECK(ct->name() == qname("urn:test", "PersonType"));
}

// Model group def and attribute group def lookups
TEST_CASE("schema_set: find model group def", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:group name="PersonGroup">
        <xs:sequence>
          <xs:element name="first" type="xs:string"/>
        </xs:sequence>
      </xs:group>
      <xs:complexType name="UseGroup">
        <xs:sequence>
          <xs:group ref="tns:PersonGroup"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  CHECK_NOTHROW(ss.resolve());

  auto* g = ss.find_model_group_def(qname("urn:test", "PersonGroup"));
  REQUIRE(g != nullptr);
}

TEST_CASE("schema_set: find attribute group def", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:attributeGroup name="CommonAttrs">
        <xs:attribute name="id" type="xs:ID"/>
      </xs:attributeGroup>
      <xs:complexType name="UseAttrs">
        <xs:sequence>
          <xs:element name="data" type="xs:string"/>
        </xs:sequence>
        <xs:attributeGroup ref="tns:CommonAttrs"/>
      </xs:complexType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  CHECK_NOTHROW(ss.resolve());

  auto* ag = ss.find_attribute_group_def(qname("urn:test", "CommonAttrs"));
  REQUIRE(ag != nullptr);
}

// Unresolved group ref
TEST_CASE("schema_set: unresolved group ref throws", "[schema_set]") {
  auto s = parse_xsd(R"(
    <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
               targetNamespace="urn:test" xmlns:tns="urn:test">
      <xs:complexType name="BadType">
        <xs:sequence>
          <xs:group ref="tns:MissingGroup"/>
        </xs:sequence>
      </xs:complexType>
    </xs:schema>
  )");

  schema_set ss;
  ss.add(std::move(s));
  CHECK_THROWS_AS(ss.resolve(), std::runtime_error);
}
