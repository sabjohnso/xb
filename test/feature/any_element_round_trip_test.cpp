#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/xml_reader.hpp>
#include <xb/xml_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <variant>

using namespace xb;

namespace {

// Parse the current element subtree (reader must be positioned on a
// start_element) into an any_element. After return, the reader has consumed
// the matching end_element.
any_element
parse_any_element(xml_reader& reader) {
  any_element result;
  result.name = reader.name();

  for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
    result.attributes.push_back(
        any_attribute{reader.attribute_name(i),
                      std::string(reader.attribute_value(i))});
  }

  std::size_t start_depth = reader.depth();
  while (reader.read()) {
    switch (reader.node_type()) {
    case xml_node_type::start_element:
      result.children.push_back(parse_any_element(reader));
      break;
    case xml_node_type::characters:
      result.children.push_back(std::string(reader.text()));
      break;
    case xml_node_type::end_element:
      if (reader.depth() == start_depth) {
        return result;
      }
      break;
    }
  }
  return result;
}

// Write an any_element tree to a writer.
void
write_any_element(xml_writer& writer, const any_element& elem) {
  writer.start_element(elem.name);
  for (const auto& attr : elem.attributes) {
    writer.attribute(attr.name, attr.value);
  }
  for (const auto& child : elem.children) {
    std::visit(
        [&writer](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, std::string>) {
            writer.characters(v);
          } else {
            write_any_element(writer, v);
          }
        },
        child);
  }
  writer.end_element();
}

// Helper: parse XML string into any_element (skips to first start_element).
any_element
parse_xml(const std::string& xml) {
  expat_reader reader(xml);
  while (reader.read()) {
    if (reader.node_type() == xml_node_type::start_element) {
      return parse_any_element(reader);
    }
  }
  throw std::runtime_error("no root element found");
}

// Helper: serialize any_element to XML string.
std::string
serialize_xml(const any_element& elem) {
  std::ostringstream os;
  ostream_writer writer(os);
  write_any_element(writer, elem);
  return os.str();
}

} // namespace

TEST_CASE("round-trip: simple element with attribute", "[any_element_round_trip]") {
  std::string input = R"(<item sku="A1">Widget</item>)";

  any_element parsed = parse_xml(input);
  CHECK(parsed.name.local_name == "item");
  REQUIRE(parsed.attributes.size() == 1);
  CHECK(parsed.attributes[0].name.local_name == "sku");
  CHECK(parsed.attributes[0].value == "A1");
  REQUIRE(parsed.children.size() == 1);
  CHECK(std::get<std::string>(parsed.children[0]) == "Widget");

  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);
}

TEST_CASE("round-trip: nested elements with attributes and mixed text",
          "[any_element_round_trip]") {
  std::string input =
      R"(<order id="123"><item sku="A1">Widget</item><item sku="B2">Gadget</item></order>)";

  any_element parsed = parse_xml(input);
  CHECK(parsed.name.local_name == "order");
  REQUIRE(parsed.attributes.size() == 1);
  CHECK(parsed.attributes[0].value == "123");
  REQUIRE(parsed.children.size() == 2);

  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);
}

TEST_CASE("round-trip: deeply nested structure", "[any_element_round_trip]") {
  std::string input = R"(<a x="1"><b y="2"><c z="3">leaf</c></b></a>)";

  any_element parsed = parse_xml(input);
  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);

  // Verify structure depth
  const auto& b = std::get<any_element>(parsed.children[0]);
  CHECK(b.name.local_name == "b");
  const auto& c = std::get<any_element>(b.children[0]);
  CHECK(c.name.local_name == "c");
  CHECK(std::get<std::string>(c.children[0]) == "leaf");
}

TEST_CASE("round-trip: mixed content", "[any_element_round_trip]") {
  std::string input = R"(<p>Hello <b>world</b>!</p>)";

  any_element parsed = parse_xml(input);
  REQUIRE(parsed.children.size() == 3);
  CHECK(std::get<std::string>(parsed.children[0]) == "Hello ");
  CHECK(std::get<any_element>(parsed.children[1]).name.local_name == "b");
  CHECK(std::get<std::string>(parsed.children[2]) == "!");

  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);
}

TEST_CASE("round-trip: element with multiple attributes", "[any_element_round_trip]") {
  std::string input = R"(<img src="/pic.png" alt="photo" width="100"/>)";

  any_element parsed = parse_xml(input);
  CHECK(parsed.attributes.size() == 3);
  CHECK(parsed.children.empty());

  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);
}

TEST_CASE("round-trip: empty root element", "[any_element_round_trip]") {
  std::string input = R"(<empty/>)";

  any_element parsed = parse_xml(input);
  CHECK(parsed.name.local_name == "empty");
  CHECK(parsed.attributes.empty());
  CHECK(parsed.children.empty());

  std::string output = serialize_xml(parsed);
  any_element reparsed = parse_xml(output);
  CHECK(parsed == reparsed);
}
