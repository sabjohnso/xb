#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/xml_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <string>

using namespace xb;

// Helper: create a reader positioned at the first start_element
static expat_reader
make_reader(const std::string& xml) {
  expat_reader reader(xml);
  reader.read(); // advance to first start_element
  return reader;
}

// ===== read_text =====

// TDD step 17: read_text reads text between start/end element
TEST_CASE("read_text reads element text content", "[xml_io]") {
  auto reader = make_reader("<value>hello world</value>");
  auto text = read_text(reader);
  CHECK(text == "hello world");
}

// TDD step 18: read_text empty element returns ""
TEST_CASE("read_text empty element returns empty string", "[xml_io]") {
  auto reader = make_reader("<value></value>");
  auto text = read_text(reader);
  CHECK(text == "");
}

TEST_CASE("read_text self-closing element returns empty string", "[xml_io]") {
  auto reader = make_reader("<value/>");
  auto text = read_text(reader);
  CHECK(text == "");
}

// ===== read_simple =====

// TDD step 19: read_simple<int32_t> reads and parses element text
TEST_CASE("read_simple int32 reads and parses", "[xml_io]") {
  auto reader = make_reader("<count>42</count>");
  auto value = read_simple<int32_t>(reader);
  CHECK(value == 42);
}

TEST_CASE("read_simple string reads element", "[xml_io]") {
  auto reader = make_reader("<name>Alice</name>");
  auto value = read_simple<std::string>(reader);
  CHECK(value == "Alice");
}

TEST_CASE("read_simple bool reads element", "[xml_io]") {
  auto reader = make_reader("<flag>true</flag>");
  auto value = read_simple<bool>(reader);
  CHECK(value == true);
}

// ===== write_simple =====

// TDD step 20: write_simple writes element with formatted text content
TEST_CASE("write_simple writes element with text", "[xml_io]") {
  std::ostringstream os;
  ostream_writer writer(os);
  writer.start_element(qname{"", "root"});
  write_simple(writer, qname{"", "count"}, int32_t{42});
  writer.end_element();
  auto xml = os.str();
  CHECK(xml.find("<count>42</count>") != std::string::npos);
}

TEST_CASE("write_simple writes string element", "[xml_io]") {
  std::ostringstream os;
  ostream_writer writer(os);
  writer.start_element(qname{"", "root"});
  write_simple(writer, qname{"", "name"}, std::string("Alice"));
  writer.end_element();
  auto xml = os.str();
  CHECK(xml.find("<name>Alice</name>") != std::string::npos);
}

TEST_CASE("write_simple writes bool element", "[xml_io]") {
  std::ostringstream os;
  ostream_writer writer(os);
  writer.start_element(qname{"", "root"});
  write_simple(writer, qname{"", "flag"}, true);
  writer.end_element();
  auto xml = os.str();
  CHECK(xml.find("<flag>true</flag>") != std::string::npos);
}

// ===== skip_element =====

// TDD step 21: skip_element skips simple element
TEST_CASE("skip_element skips simple element", "[xml_io]") {
  auto reader =
      make_reader("<root><skip>ignore</skip><keep>value</keep></root>");
  // Reader is at <root>, advance to first child
  reader.read(); // <skip>
  CHECK(reader.name() == qname{"", "skip"});
  skip_element(reader);
  // Should now be able to read <keep>
  reader.read(); // <keep>
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"", "keep"});
}

// TDD step 22: skip_element skips element with nested children
TEST_CASE("skip_element skips element with nested children", "[xml_io]") {
  auto reader = make_reader(
      "<root><skip><a><b>text</b></a></skip><keep>value</keep></root>");
  reader.read(); // <skip>
  CHECK(reader.name() == qname{"", "skip"});
  skip_element(reader);
  reader.read(); // <keep>
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"", "keep"});
}
