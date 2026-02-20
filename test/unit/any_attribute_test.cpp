#include <xb/any_attribute.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

TEST_CASE("any_attribute default construction", "[any_attribute]") {
  xb::any_attribute a;
  CHECK(a.name().namespace_uri().empty());
  CHECK(a.name().local_name().empty());
  CHECK(a.value().empty());
}

TEST_CASE("any_attribute construction with values", "[any_attribute]") {
  xb::any_attribute a(xb::qname("http://example.org", "attr"), "hello");
  CHECK(a.name().namespace_uri() == "http://example.org");
  CHECK(a.name().local_name() == "attr");
  CHECK(a.value() == "hello");
}

TEST_CASE("any_attribute copy semantics", "[any_attribute]") {
  xb::any_attribute original(xb::qname("urn:ns", "x"), "value");

  xb::any_attribute copied = original;
  CHECK(copied == original);

  // After encapsulation, any_attribute is immutable — copy independence is
  // guaranteed by value semantics.
  CHECK(copied.value() == "value");
  CHECK(original.value() == "value");
}

TEST_CASE("any_attribute move semantics", "[any_attribute]") {
  xb::any_attribute source(xb::qname("urn:ns", "x"), "value");
  std::string expected_value = source.value();
  xb::qname expected_name = source.name();

  xb::any_attribute moved = std::move(source);
  CHECK(moved.name() == expected_name);
  CHECK(moved.value() == expected_value);
}

TEST_CASE("any_attribute equality", "[any_attribute]") {
  xb::any_attribute a(xb::qname("urn:ns", "x"), "val");
  xb::any_attribute b(xb::qname("urn:ns", "x"), "val");
  xb::any_attribute c(xb::qname("urn:ns", "x"), "other");
  xb::any_attribute d(xb::qname("urn:ns", "y"), "val");

  CHECK(a == b);
  CHECK_FALSE(a == c);
  CHECK_FALSE(a == d);
  CHECK(a != c);
  CHECK(a != d);
}

TEST_CASE("any_attribute ordering", "[any_attribute]") {
  xb::any_attribute a(xb::qname("aaa", "x"), "zzz");
  xb::any_attribute b(xb::qname("bbb", "x"), "aaa");
  xb::any_attribute c(xb::qname("aaa", "x"), "aaa");

  CHECK(a < b);
  CHECK(c < a);
  CHECK(c <= a);
  CHECK(b > a);
  CHECK(a >= c);
  CHECK(a >= a);
}

TEST_CASE("any_attribute hashing: equal values hash equal", "[any_attribute]") {
  xb::any_attribute a(xb::qname("urn:ns", "x"), "val");
  xb::any_attribute b(xb::qname("urn:ns", "x"), "val");

  std::hash<xb::any_attribute> hasher;
  CHECK(hasher(a) == hasher(b));
}

TEST_CASE("any_attribute usable as unordered_map key", "[any_attribute]") {
  std::unordered_map<xb::any_attribute, int> map;
  xb::any_attribute key(xb::qname("urn:ns", "x"), "val");
  map[key] = 42;

  CHECK(map.at(key) == 42);
  CHECK(map.count(xb::any_attribute(xb::qname("urn:ns", "x"), "val")) == 1);
  CHECK(map.count(xb::any_attribute(xb::qname("urn:ns", "x"), "other")) == 0);
}

TEST_CASE("any_attribute stream output", "[any_attribute]") {
  xb::any_attribute a(xb::qname("http://example.org", "attr"), "hello");
  std::ostringstream os;
  os << a;

  std::string output = os.str();
  CHECK(output.find("http://example.org") != std::string::npos);
  CHECK(output.find("attr") != std::string::npos);
  CHECK(output.find("hello") != std::string::npos);
}

TEST_CASE("any_attribute stream output escapes special characters",
          "[any_attribute]") {
  xb::any_attribute a(xb::qname("", "x"), R"(a"b&c<d)");
  std::ostringstream os;
  os << a;
  std::string output = os.str();
  CHECK(output.find("&quot;") != std::string::npos);
  CHECK(output.find("&amp;") != std::string::npos);
  CHECK(output.find("&lt;") != std::string::npos);
}

TEST_CASE("any_attribute stream output with empty namespace",
          "[any_attribute]") {
  xb::any_attribute a(xb::qname("", "local"), "value");
  std::ostringstream os;
  os << a;

  std::string output = os.str();
  CHECK(output.find("local") != std::string::npos);
  CHECK(output.find("value") != std::string::npos);
}
