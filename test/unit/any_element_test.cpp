#include <xb/any_element.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace xb;

TEST_CASE("any_element default construction", "[any_element]") {
  any_element e;
  CHECK(e.name.namespace_uri.empty());
  CHECK(e.name.local_name.empty());
  CHECK(e.attributes.empty());
  CHECK(e.children.empty());
}

TEST_CASE("any_element construction with text child", "[any_element]") {
  any_element e{
      {"", "p"},
      {},
      {std::string("hello world")},
  };
  CHECK(e.name.local_name == "p");
  REQUIRE(e.children.size() == 1);
  CHECK(std::holds_alternative<std::string>(e.children[0]));
  CHECK(std::get<std::string>(e.children[0]) == "hello world");
}

TEST_CASE("any_element construction with element child", "[any_element]") {
  any_element child{{"", "span"}, {}, {std::string("inner")}};
  any_element parent{
      {"", "div"},
      {},
      {child},
  };
  CHECK(parent.name.local_name == "div");
  REQUIRE(parent.children.size() == 1);
  CHECK(std::holds_alternative<any_element>(parent.children[0]));

  const auto& got = std::get<any_element>(parent.children[0]);
  CHECK(got.name.local_name == "span");
  REQUIRE(got.children.size() == 1);
  CHECK(std::get<std::string>(got.children[0]) == "inner");
}

TEST_CASE("any_element with attributes", "[any_element]") {
  any_element e{
      {"", "img"},
      {any_attribute{{"", "src"}, "/pic.png"},
       any_attribute{{"", "alt"}, "photo"}},
      {},
  };
  CHECK(e.attributes.size() == 2);
  CHECK(e.attributes[0].name.local_name == "src");
  CHECK(e.attributes[0].value == "/pic.png");
  CHECK(e.attributes[1].name.local_name == "alt");
  CHECK(e.attributes[1].value == "photo");
}

TEST_CASE("any_element mixed content", "[any_element]") {
  // <p>Hello <b>world</b>!</p>
  any_element bold{{"", "b"}, {}, {std::string("world")}};
  any_element p{
      {"", "p"},
      {},
      {std::string("Hello "), bold, std::string("!")},
  };
  REQUIRE(p.children.size() == 3);
  CHECK(std::holds_alternative<std::string>(p.children[0]));
  CHECK(std::holds_alternative<any_element>(p.children[1]));
  CHECK(std::holds_alternative<std::string>(p.children[2]));
}

TEST_CASE("any_element deep copy semantics", "[any_element]") {
  any_element inner{{"", "inner"}, {}, {std::string("text")}};
  any_element middle{{"", "middle"}, {}, {inner}};
  any_element outer{{"", "outer"}, {}, {middle}};

  any_element copy = outer;
  CHECK(copy == outer);

  // Mutate the copy's deep child
  auto& copy_middle = std::get<any_element>(copy.children[0]);
  auto& copy_inner = std::get<any_element>(copy_middle.children[0]);
  copy_inner.name.local_name = "modified";

  // Original unchanged
  const auto& orig_middle = std::get<any_element>(outer.children[0]);
  const auto& orig_inner = std::get<any_element>(orig_middle.children[0]);
  CHECK(orig_inner.name.local_name == "inner");
  CHECK(copy_inner.name.local_name == "modified");
  CHECK_FALSE(copy == outer);
}

TEST_CASE("any_element move semantics", "[any_element]") {
  any_element source{
      {"urn:ns", "elem"},
      {any_attribute{{"", "a"}, "1"}},
      {std::string("text")},
  };
  qname expected_name = source.name;
  std::size_t expected_attrs = source.attributes.size();
  std::size_t expected_children = source.children.size();

  any_element moved = std::move(source);
  CHECK(moved.name == expected_name);
  CHECK(moved.attributes.size() == expected_attrs);
  CHECK(moved.children.size() == expected_children);
}

TEST_CASE("any_element equality: deep comparison", "[any_element]") {
  any_element a{
      {"", "root"},
      {any_attribute{{"", "id"}, "1"}},
      {std::string("hello"), any_element{{"", "child"}, {}, {}}},
  };
  any_element b{
      {"", "root"},
      {any_attribute{{"", "id"}, "1"}},
      {std::string("hello"), any_element{{"", "child"}, {}, {}}},
  };
  any_element c{
      {"", "root"},
      {any_attribute{{"", "id"}, "2"}},
      {std::string("hello"), any_element{{"", "child"}, {}, {}}},
  };

  CHECK(a == b);
  CHECK_FALSE(a == c);
  CHECK(a != c);
}

TEST_CASE("any_element nested 3+ levels deep", "[any_element]") {
  any_element level3{{"", "c"}, {}, {std::string("leaf")}};
  any_element level2{{"", "b"}, {}, {level3}};
  any_element level1{{"", "a"}, {}, {level2}};

  // Navigate three levels
  const auto& l2 = std::get<any_element>(level1.children[0]);
  CHECK(l2.name.local_name == "b");
  const auto& l3 = std::get<any_element>(l2.children[0]);
  CHECK(l3.name.local_name == "c");
  CHECK(std::get<std::string>(l3.children[0]) == "leaf");
}

TEST_CASE("any_element stream output", "[any_element]") {
  any_element e{
      {"http://example.org", "root"},
      {any_attribute{{"", "id"}, "1"}},
      {std::string("text"), any_element{{"", "child"}, {}, {}}},
  };
  std::ostringstream os;
  os << e;

  std::string output = os.str();
  CHECK(output.find("root") != std::string::npos);
  CHECK(output.find("id") != std::string::npos);
  CHECK(output.find("text") != std::string::npos);
  CHECK(output.find("child") != std::string::npos);
}
