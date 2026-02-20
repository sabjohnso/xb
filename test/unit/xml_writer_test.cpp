#include <xb/ostream_writer.hpp>
#include <xb/xml_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

using namespace xb;

TEST_CASE("writer: empty element (self-closing)", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "root"});
  writer.end_element();

  CHECK(os.str() == "<root/>");
}

TEST_CASE("writer: element with text content", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "msg"});
  writer.characters("hello");
  writer.end_element();

  CHECK(os.str() == "<msg>hello</msg>");
}

TEST_CASE("writer: nested elements", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "a"});
  writer.start_element({"", "b"});
  writer.end_element();
  writer.end_element();

  CHECK(os.str() == "<a><b/></a>");
}

TEST_CASE("writer: attributes", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "e"});
  writer.attribute({"", "x"}, "1");
  writer.attribute({"", "y"}, "2");
  writer.end_element();

  CHECK(os.str() == R"(<e x="1" y="2"/>)");
}

TEST_CASE("writer: escape special characters in text", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "e"});
  writer.characters("a<b>c&d");
  writer.end_element();

  CHECK(os.str() == "<e>a&lt;b&gt;c&amp;d</e>");
}

TEST_CASE("writer: escape special characters in attribute values", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "e"});
  writer.attribute({"", "v"}, R"(a"b<c&d)");
  writer.end_element();

  CHECK(os.str() == R"(<e v="a&quot;b&lt;c&amp;d"/>)");
}

TEST_CASE("writer: default namespace declaration", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"http://example.org", "root"});
  writer.namespace_declaration("", "http://example.org");
  writer.end_element();

  CHECK(os.str() == R"(<root xmlns="http://example.org"/>)");
}

TEST_CASE("writer: prefixed namespace declaration", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"http://example.org", "root"});
  writer.namespace_declaration("ns", "http://example.org");
  writer.end_element();

  CHECK(os.str() == R"(<ns:root xmlns:ns="http://example.org"/>)");
}

TEST_CASE("writer: prefixed child elements", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"http://example.org", "root"});
  writer.namespace_declaration("ns", "http://example.org");
  writer.start_element({"http://example.org", "child"});
  writer.end_element();
  writer.end_element();

  CHECK(os.str() == R"(<ns:root xmlns:ns="http://example.org"><ns:child/></ns:root>)");
}

TEST_CASE("writer: element with children is not self-closing", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "parent"});
  writer.start_element({"", "child"});
  writer.characters("text");
  writer.end_element();
  writer.end_element();

  CHECK(os.str() == "<parent><child>text</child></parent>");
}

TEST_CASE("writer: attributes with element content", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "e"});
  writer.attribute({"", "a"}, "1");
  writer.characters("text");
  writer.end_element();

  CHECK(os.str() == R"(<e a="1">text</e>)");
}

TEST_CASE("writer: namespaced attributes", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"", "root"});
  writer.namespace_declaration("xsi", "http://www.w3.org/2001/XMLSchema-instance");
  writer.attribute({"http://www.w3.org/2001/XMLSchema-instance", "type"}, "myType");
  writer.end_element();

  CHECK(os.str() ==
        R"(<root xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="myType"/>)");
}

TEST_CASE("writer: multiple namespace declarations", "[xml_writer]") {
  std::ostringstream os;
  ostream_writer writer(os);

  writer.start_element({"http://a.example", "root"});
  writer.namespace_declaration("a", "http://a.example");
  writer.namespace_declaration("b", "http://b.example");
  writer.start_element({"http://b.example", "child"});
  writer.end_element();
  writer.end_element();

  CHECK(os.str() ==
        R"(<a:root xmlns:a="http://a.example" xmlns:b="http://b.example"><b:child/></a:root>)");
}
