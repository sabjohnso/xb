#include <xb/expat_reader.hpp>
#include <xb/xml_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace xb;

#ifndef XB_SCHEMA_DIR
#error "XB_SCHEMA_DIR must be defined"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static std::string
read_file(const std::string& path) {
  std::ifstream in(path);
  REQUIRE(in.good());
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

static bool
is_whitespace_only(std::string_view sv) {
  return !sv.empty() && std::all_of(sv.begin(), sv.end(), [](char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
  });
}

static bool
read_skip_ws(xml_reader& reader) {
  while (reader.read()) {
    if (reader.node_type() == xml_node_type::characters &&
        is_whitespace_only(reader.text()))
      continue;
    return true;
  }
  return false;
}

static const std::string schema_dir = TOSTRING(XB_SCHEMA_DIR);

TEST_CASE("typemap schema: xb-typemap.xsd is well-formed XML",
          "[typemap_schema]") {
  auto xml = read_file(schema_dir + "/xb-typemap.xsd");
  REQUIRE_FALSE(xml.empty());

  expat_reader reader(xml);
  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
}

TEST_CASE("typemap schema: valid document has correct structure",
          "[typemap_schema]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="decimal"
                  cpp-type="double"
                  cpp-header="&lt;cmath&gt;"/>
    </xb:typemap>
  )";

  expat_reader reader(doc);

  REQUIRE(read_skip_ws(reader));
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname("http://xb.dev/typemap", "typemap"));

  REQUIRE(read_skip_ws(reader));
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname("http://xb.dev/typemap", "mapping"));

  CHECK(reader.attribute_count() == 3);
  CHECK(reader.attribute_value(qname("", "xsd-type")) == "decimal");
  CHECK(reader.attribute_value(qname("", "cpp-type")) == "double");
  CHECK(reader.attribute_value(qname("", "cpp-header")) == "<cmath>");
}

TEST_CASE("typemap schema: empty typemap is valid", "[typemap_schema]") {
  std::string doc = R"(<xb:typemap xmlns:xb="http://xb.dev/typemap"/>)";

  expat_reader reader(doc);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname("http://xb.dev/typemap", "typemap"));

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name() == qname("http://xb.dev/typemap", "typemap"));

  CHECK_FALSE(reader.read());
}

TEST_CASE("typemap schema: multiple mappings parse correctly",
          "[typemap_schema]") {
  std::string doc = R"(
    <xb:typemap xmlns:xb="http://xb.dev/typemap">
      <xb:mapping xsd-type="decimal"
                  cpp-type="double"
                  cpp-header="&lt;cmath&gt;"/>
      <xb:mapping xsd-type="integer"
                  cpp-type="int64_t"
                  cpp-header="&lt;cstdint&gt;"/>
      <xb:mapping xsd-type="dateTime"
                  cpp-type="my::timestamp"
                  cpp-header="&quot;my/timestamp.hpp&quot;"/>
    </xb:typemap>
  )";

  expat_reader reader(doc);

  // Root element
  REQUIRE(read_skip_ws(reader));
  CHECK(reader.name() == qname("http://xb.dev/typemap", "typemap"));

  // First mapping: decimal -> double
  REQUIRE(read_skip_ws(reader));
  CHECK(reader.name() == qname("http://xb.dev/typemap", "mapping"));
  CHECK(reader.attribute_value(qname("", "xsd-type")) == "decimal");
  CHECK(reader.attribute_value(qname("", "cpp-type")) == "double");
  CHECK(reader.attribute_value(qname("", "cpp-header")) == "<cmath>");

  REQUIRE(read_skip_ws(reader)); // end mapping
  CHECK(reader.node_type() == xml_node_type::end_element);

  // Second mapping: integer -> int64_t
  REQUIRE(read_skip_ws(reader));
  CHECK(reader.name() == qname("http://xb.dev/typemap", "mapping"));
  CHECK(reader.attribute_value(qname("", "xsd-type")) == "integer");
  CHECK(reader.attribute_value(qname("", "cpp-type")) == "int64_t");
  CHECK(reader.attribute_value(qname("", "cpp-header")) == "<cstdint>");

  REQUIRE(read_skip_ws(reader)); // end mapping
  CHECK(reader.node_type() == xml_node_type::end_element);

  // Third mapping: dateTime -> my::timestamp
  REQUIRE(read_skip_ws(reader));
  CHECK(reader.name() == qname("http://xb.dev/typemap", "mapping"));
  CHECK(reader.attribute_value(qname("", "xsd-type")) == "dateTime");
  CHECK(reader.attribute_value(qname("", "cpp-type")) == "my::timestamp");
  CHECK(reader.attribute_value(qname("", "cpp-header")) ==
        "\"my/timestamp.hpp\"");

  REQUIRE(read_skip_ws(reader)); // end mapping
  CHECK(reader.node_type() == xml_node_type::end_element);

  // End of root
  REQUIRE(read_skip_ws(reader));
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name() == qname("http://xb.dev/typemap", "typemap"));

  CHECK_FALSE(read_skip_ws(reader));
}

TEST_CASE("typemap schema: xsd-type restricted to known XSD built-in types",
          "[typemap_schema]") {
  const std::set<std::string> expected = {
      "string",
      "normalizedString",
      "token",
      "boolean",
      "float",
      "double",
      "decimal",
      "integer",
      "nonPositiveInteger",
      "negativeInteger",
      "nonNegativeInteger",
      "positiveInteger",
      "long",
      "int",
      "short",
      "byte",
      "unsignedLong",
      "unsignedInt",
      "unsignedShort",
      "unsignedByte",
      "dateTime",
      "date",
      "time",
      "duration",
      "hexBinary",
      "base64Binary",
      "anyURI",
      "QName",
      "ID",
      "IDREF",
      "NMTOKEN",
      "language",
  };

  auto xml = read_file(schema_dir + "/xb-typemap.xsd");
  expat_reader reader(xml);

  // Walk the schema and collect enumeration values that are children of
  // the xsdBuiltinType simpleType (identified by its xs:restriction parent
  // inside xs:simpleType[@name='xsdBuiltinType']).
  const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
  std::set<std::string> found;
  bool in_builtin_type = false;

  while (reader.read()) {
    if (reader.node_type() == xml_node_type::start_element) {
      if (reader.name() == qname(xs_ns, "simpleType") &&
          reader.attribute_value(qname("", "name")) == "xsdBuiltinType") {
        in_builtin_type = true;
      } else if (in_builtin_type &&
                 reader.name() == qname(xs_ns, "enumeration")) {
        found.insert(std::string(reader.attribute_value(qname("", "value"))));
      }
    } else if (reader.node_type() == xml_node_type::end_element) {
      if (reader.name() == qname(xs_ns, "simpleType")) {
        in_builtin_type = false;
      }
    }
  }

  CHECK(found == expected);
}
