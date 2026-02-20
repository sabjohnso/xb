#include <xb/any_element.hpp>
#include <xb/xml_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace xb;

namespace {

  // A mock reader that delivers a single start_element event and then EOF,
  // simulating truncated XML input.
  class truncated_reader : public xml_reader {
    qname name_;
    bool delivered_ = false;

  public:
    explicit truncated_reader(qname name) : name_(std::move(name)) {}

    bool
    read() override {
      if (!delivered_) {
        delivered_ = true;
        return true;
      }
      return false;
    }

    xml_node_type
    node_type() const override {
      return xml_node_type::start_element;
    }

    const qname&
    name() const override {
      return name_;
    }

    std::size_t
    attribute_count() const override {
      return 0;
    }

    const qname&
    attribute_name(std::size_t /*index*/) const override {
      return name_;
    }

    std::string_view
    attribute_value(std::size_t /*index*/) const override {
      return {};
    }

    std::string_view
    attribute_value(const qname& /*name*/) const override {
      return {};
    }

    std::string_view
    text() const override {
      return {};
    }

    std::size_t
    depth() const override {
      return 1;
    }
  };

} // namespace

TEST_CASE("any_element default construction", "[any_element]") {
  any_element e;
  CHECK(e.name().namespace_uri().empty());
  CHECK(e.name().local_name().empty());
  CHECK(e.attributes().empty());
  CHECK(e.children().empty());
}

TEST_CASE("any_element construction with text child", "[any_element]") {
  any_element e(qname("", "p"), {}, {std::string("hello world")});
  CHECK(e.name().local_name() == "p");
  REQUIRE(e.children().size() == 1);
  CHECK(std::holds_alternative<std::string>(e.children()[0]));
  CHECK(std::get<std::string>(e.children()[0]) == "hello world");
}

TEST_CASE("any_element construction with element child", "[any_element]") {
  any_element child(qname("", "span"), {}, {std::string("inner")});
  any_element parent(qname("", "div"), {}, {child});
  CHECK(parent.name().local_name() == "div");
  REQUIRE(parent.children().size() == 1);
  CHECK(std::holds_alternative<any_element>(parent.children()[0]));

  const auto& got = std::get<any_element>(parent.children()[0]);
  CHECK(got.name().local_name() == "span");
  REQUIRE(got.children().size() == 1);
  CHECK(std::get<std::string>(got.children()[0]) == "inner");
}

TEST_CASE("any_element with attributes", "[any_element]") {
  any_element e(qname("", "img"),
                {any_attribute(qname("", "src"), "/pic.png"),
                 any_attribute(qname("", "alt"), "photo")},
                {});
  CHECK(e.attributes().size() == 2);
  CHECK(e.attributes()[0].name().local_name() == "src");
  CHECK(e.attributes()[0].value() == "/pic.png");
  CHECK(e.attributes()[1].name().local_name() == "alt");
  CHECK(e.attributes()[1].value() == "photo");
}

TEST_CASE("any_element mixed content", "[any_element]") {
  // <p>Hello <b>world</b>!</p>
  any_element bold(qname("", "b"), {}, {std::string("world")});
  any_element p(qname("", "p"), {},
                {std::string("Hello "), bold, std::string("!")});
  REQUIRE(p.children().size() == 3);
  CHECK(std::holds_alternative<std::string>(p.children()[0]));
  CHECK(std::holds_alternative<any_element>(p.children()[1]));
  CHECK(std::holds_alternative<std::string>(p.children()[2]));
}

TEST_CASE("any_element deep copy semantics", "[any_element]") {
  any_element inner(qname("", "inner"), {}, {std::string("text")});
  any_element middle(qname("", "middle"), {}, {inner});
  any_element outer(qname("", "outer"), {}, {middle});

  any_element copy = outer;
  CHECK(copy == outer);
}

TEST_CASE("any_element move semantics", "[any_element]") {
  any_element source(qname("urn:ns", "elem"),
                     {any_attribute(qname("", "a"), "1")},
                     {std::string("text")});
  qname expected_name = source.name();
  std::size_t expected_attrs = source.attributes().size();
  std::size_t expected_children = source.children().size();

  any_element moved = std::move(source);
  CHECK(moved.name() == expected_name);
  CHECK(moved.attributes().size() == expected_attrs);
  CHECK(moved.children().size() == expected_children);
}

TEST_CASE("any_element equality: deep comparison", "[any_element]") {
  any_element a(
      qname("", "root"), {any_attribute(qname("", "id"), "1")},
      {std::string("hello"), any_element(qname("", "child"), {}, {})});
  any_element b(
      qname("", "root"), {any_attribute(qname("", "id"), "1")},
      {std::string("hello"), any_element(qname("", "child"), {}, {})});
  any_element c(
      qname("", "root"), {any_attribute(qname("", "id"), "2")},
      {std::string("hello"), any_element(qname("", "child"), {}, {})});

  CHECK(a == b);
  CHECK_FALSE(a == c);
  CHECK(a != c);
}

TEST_CASE("any_element nested 3+ levels deep", "[any_element]") {
  any_element level3(qname("", "c"), {}, {std::string("leaf")});
  any_element level2(qname("", "b"), {}, {level3});
  any_element level1(qname("", "a"), {}, {level2});

  // Navigate three levels
  const auto& l2 = std::get<any_element>(level1.children()[0]);
  CHECK(l2.name().local_name() == "b");
  const auto& l3 = std::get<any_element>(l2.children()[0]);
  CHECK(l3.name().local_name() == "c");
  CHECK(std::get<std::string>(l3.children()[0]) == "leaf");
}

TEST_CASE("any_element stream output escapes text children", "[any_element]") {
  any_element e(qname("", "p"), {}, {std::string("a<b&c>d")});
  std::ostringstream os;
  os << e;
  std::string output = os.str();
  CHECK(output.find("&lt;") != std::string::npos);
  CHECK(output.find("&amp;") != std::string::npos);
  CHECK(output.find("&gt;") != std::string::npos);
}

TEST_CASE("any_element stream output", "[any_element]") {
  any_element e(qname("http://example.org", "root"),
                {any_attribute(qname("", "id"), "1")},
                {std::string("text"), any_element(qname("", "child"), {}, {})});
  std::ostringstream os;
  os << e;

  std::string output = os.str();
  CHECK(output.find("root") != std::string::npos);
  CHECK(output.find("id") != std::string::npos);
  CHECK(output.find("text") != std::string::npos);
  CHECK(output.find("child") != std::string::npos);
}

TEST_CASE("any_element constructor throws on truncated input",
          "[any_element]") {
  truncated_reader reader(qname("", "root"));
  // Advance to the start_element event so any_element sees it as its own
  REQUIRE(reader.read());
  CHECK_THROWS_AS(any_element(reader), std::runtime_error);
}
