#include <xb/expat_reader.hpp>
#include <xb/xml_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

using namespace xb;

TEST_CASE("reader: empty element", "[xml_reader]") {
  expat_reader reader("<root/>");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"", "root"});
  CHECK(reader.depth() == 1);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name() == qname{"", "root"});
  CHECK(reader.depth() == 1);

  CHECK_FALSE(reader.read());
}

TEST_CASE("reader: element with text content", "[xml_reader]") {
  expat_reader reader("<msg>hello</msg>");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"", "msg"});

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::characters);
  CHECK(reader.text() == "hello");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name() == qname{"", "msg"});

  CHECK_FALSE(reader.read());
}

TEST_CASE("reader: nested elements", "[xml_reader]") {
  expat_reader reader("<a><b><c/></b></a>");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name().local_name == "a");
  CHECK(reader.depth() == 1);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name().local_name == "b");
  CHECK(reader.depth() == 2);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name().local_name == "c");
  CHECK(reader.depth() == 3);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name().local_name == "c");
  CHECK(reader.depth() == 3);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name().local_name == "b");
  CHECK(reader.depth() == 2);

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::end_element);
  CHECK(reader.name().local_name == "a");
  CHECK(reader.depth() == 1);

  CHECK_FALSE(reader.read());
}

TEST_CASE("reader: attributes by index", "[xml_reader]") {
  expat_reader reader(R"(<e x="1" y="2"/>)");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.attribute_count() == 2);

  // Expat preserves document order for attributes
  bool found_x = false;
  bool found_y = false;
  for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
    if (reader.attribute_name(i).local_name == "x") {
      CHECK(reader.attribute_value(i) == "1");
      found_x = true;
    }
    if (reader.attribute_name(i).local_name == "y") {
      CHECK(reader.attribute_value(i) == "2");
      found_y = true;
    }
  }
  CHECK(found_x);
  CHECK(found_y);
}

TEST_CASE("reader: attribute by qname lookup", "[xml_reader]") {
  expat_reader reader(R"(<e color="red" size="large"/>)");

  REQUIRE(reader.read());
  CHECK(reader.attribute_value(qname{"", "color"}) == "red");
  CHECK(reader.attribute_value(qname{"", "size"}) == "large");
}

TEST_CASE("reader: missing attribute returns empty string_view", "[xml_reader]") {
  expat_reader reader(R"(<e x="1"/>)");

  REQUIRE(reader.read());
  CHECK(reader.attribute_value(qname{"", "missing"}).empty());
}

TEST_CASE("reader: namespaced elements with prefix", "[xml_reader]") {
  expat_reader reader(
      R"(<ns:root xmlns:ns="http://example.org"><ns:child/></ns:root>)");

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"http://example.org", "root"});

  REQUIRE(reader.read());
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name() == qname{"http://example.org", "child"});
}

TEST_CASE("reader: default namespace", "[xml_reader]") {
  expat_reader reader(
      R"(<root xmlns="http://example.org"><child/></root>)");

  REQUIRE(reader.read());
  CHECK(reader.name() == qname{"http://example.org", "root"});

  REQUIRE(reader.read());
  CHECK(reader.name() == qname{"http://example.org", "child"});
}

TEST_CASE("reader: namespaced attributes", "[xml_reader]") {
  expat_reader reader(
      R"(<root xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" )"
      R"(xsi:type="myType"/>)");

  REQUIRE(reader.read());

  qname expected_attr{"http://www.w3.org/2001/XMLSchema-instance", "type"};
  CHECK(reader.attribute_value(expected_attr) == "myType");
}

TEST_CASE("reader: coalesce adjacent character data", "[xml_reader]") {
  // Entity references cause expat to split character data callbacks.
  // The reader must coalesce them into a single characters event.
  expat_reader reader("<e>a&amp;b</e>");

  REQUIRE(reader.read()); // start_element
  REQUIRE(reader.read()); // characters
  CHECK(reader.node_type() == xml_node_type::characters);
  CHECK(reader.text() == "a&b");

  REQUIRE(reader.read()); // end_element
  CHECK(reader.node_type() == xml_node_type::end_element);
}

TEST_CASE("reader: depth tracking", "[xml_reader]") {
  expat_reader reader("<a><b>text</b></a>");

  REQUIRE(reader.read()); // start a
  CHECK(reader.depth() == 1);

  REQUIRE(reader.read()); // start b
  CHECK(reader.depth() == 2);

  REQUIRE(reader.read()); // characters "text"
  CHECK(reader.depth() == 2);

  REQUIRE(reader.read()); // end b
  CHECK(reader.depth() == 2);

  REQUIRE(reader.read()); // end a
  CHECK(reader.depth() == 1);
}

TEST_CASE("reader: throws on malformed XML", "[xml_reader]") {
  CHECK_THROWS_AS(expat_reader("<unclosed>"), std::runtime_error);
}

TEST_CASE("reader: throws on empty input", "[xml_reader]") {
  CHECK_THROWS_AS(expat_reader(""), std::runtime_error);
}

TEST_CASE("reader: multiple children with mixed content", "[xml_reader]") {
  expat_reader reader("<p>Hello <b>world</b>!</p>");

  REQUIRE(reader.read()); // start p
  CHECK(reader.node_type() == xml_node_type::start_element);

  REQUIRE(reader.read()); // characters "Hello "
  CHECK(reader.node_type() == xml_node_type::characters);
  CHECK(reader.text() == "Hello ");

  REQUIRE(reader.read()); // start b
  CHECK(reader.node_type() == xml_node_type::start_element);
  CHECK(reader.name().local_name == "b");

  REQUIRE(reader.read()); // characters "world"
  CHECK(reader.node_type() == xml_node_type::characters);
  CHECK(reader.text() == "world");

  REQUIRE(reader.read()); // end b
  CHECK(reader.node_type() == xml_node_type::end_element);

  REQUIRE(reader.read()); // characters "!"
  CHECK(reader.node_type() == xml_node_type::characters);
  CHECK(reader.text() == "!");

  REQUIRE(reader.read()); // end p
  CHECK(reader.node_type() == xml_node_type::end_element);

  CHECK_FALSE(reader.read());
}
