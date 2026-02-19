#include <xb/qname.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

TEST_CASE("qname default construction", "[qname]") {
  xb::qname q;
  CHECK(q.namespace_uri.empty());
  CHECK(q.local_name.empty());
}

TEST_CASE("qname construction with values", "[qname]") {
  xb::qname q{"http://www.w3.org/2001/XMLSchema", "string"};
  CHECK(q.namespace_uri == "http://www.w3.org/2001/XMLSchema");
  CHECK(q.local_name == "string");
}

TEST_CASE("qname copy semantics", "[qname]") {
  xb::qname original{"urn:example", "element"};

  xb::qname copied = original;
  CHECK(copied == original);

  copied.local_name = "other";
  CHECK(copied.local_name == "other");
  CHECK(original.local_name == "element");
}

TEST_CASE("qname move semantics", "[qname]") {
  xb::qname source{"urn:example", "element"};
  std::string expected_ns = source.namespace_uri;
  std::string expected_ln = source.local_name;

  xb::qname moved = std::move(source);
  CHECK(moved.namespace_uri == expected_ns);
  CHECK(moved.local_name == expected_ln);
}

TEST_CASE("qname equality", "[qname]") {
  xb::qname a{"urn:ns", "name"};
  xb::qname b{"urn:ns", "name"};
  xb::qname c{"urn:ns", "other"};
  xb::qname d{"urn:other", "name"};

  CHECK(a == b);
  CHECK_FALSE(a == c);
  CHECK_FALSE(a == d);
  CHECK(a != c);
  CHECK(a != d);
}

TEST_CASE("qname ordering is lexicographic: namespace first, then local",
          "[qname]") {
  xb::qname a{"aaa", "zzz"};
  xb::qname b{"bbb", "aaa"};
  xb::qname c{"aaa", "aaa"};

  CHECK(a < b);
  CHECK(c < a);
  CHECK(c <= a);
  CHECK(b > a);
  CHECK(a >= c);
  CHECK(a >= a);
}

TEST_CASE("qname hashing: equal qnames hash equal", "[qname]") {
  xb::qname a{"urn:ns", "name"};
  xb::qname b{"urn:ns", "name"};

  std::hash<xb::qname> hasher;
  CHECK(hasher(a) == hasher(b));
}

TEST_CASE("qname usable as unordered_map key", "[qname]") {
  std::unordered_map<xb::qname, int> map;
  xb::qname key{"urn:ns", "elem"};
  map[key] = 42;

  CHECK(map.at(key) == 42);
  CHECK(map.count({"urn:ns", "elem"}) == 1);
  CHECK(map.count({"urn:ns", "other"}) == 0);
}

TEST_CASE("qname stream output", "[qname]") {
  xb::qname q{"http://example.org", "element"};
  std::ostringstream os;
  os << q;

  std::string output = os.str();
  CHECK(output.find("http://example.org") != std::string::npos);
  CHECK(output.find("element") != std::string::npos);
}

TEST_CASE("qname stream output with empty namespace", "[qname]") {
  xb::qname q{"", "local"};
  std::ostringstream os;
  os << q;

  std::string output = os.str();
  CHECK(output.find("local") != std::string::npos);
}
