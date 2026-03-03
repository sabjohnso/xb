#include <xb/wsa.hpp>

#include <catch2/catch_test_macros.hpp>

namespace wsa = xb::wsa;

// -- Namespace constants ------------------------------------------------------

TEST_CASE("wsa: namespace URI matches W3C spec", "[wsa]") {
  CHECK(std::string(wsa::wsa_ns) == "http://www.w3.org/2005/08/addressing");
}

TEST_CASE("wsa: anonymous URI matches W3C spec", "[wsa]") {
  CHECK(std::string(wsa::anonymous_uri) ==
        "http://www.w3.org/2005/08/addressing/anonymous");
}

TEST_CASE("wsa: none URI matches W3C spec", "[wsa]") {
  CHECK(std::string(wsa::none_uri) ==
        "http://www.w3.org/2005/08/addressing/none");
}

TEST_CASE("wsa: reply relationship matches W3C spec", "[wsa]") {
  CHECK(std::string(wsa::reply_relationship) ==
        "http://www.w3.org/2005/08/addressing/reply");
}

// -- endpoint_reference -------------------------------------------------------

TEST_CASE("wsa: endpoint_reference default construction", "[wsa]") {
  wsa::endpoint_reference epr;
  CHECK(epr.address.empty());
}

TEST_CASE("wsa: endpoint_reference equality", "[wsa]") {
  wsa::endpoint_reference a{"http://example.org/service"};
  wsa::endpoint_reference b{"http://example.org/service"};
  CHECK(a == b);

  b.address = "http://other.org";
  CHECK_FALSE(a == b);
}

// -- relates_to ---------------------------------------------------------------

TEST_CASE("wsa: relates_to default relationship is reply", "[wsa]") {
  wsa::relates_to rt;
  CHECK(rt.uri.empty());
  CHECK(rt.relationship_type == wsa::reply_relationship);
}

TEST_CASE("wsa: relates_to equality", "[wsa]") {
  wsa::relates_to a{"urn:uuid:123", std::string(wsa::reply_relationship)};
  wsa::relates_to b{"urn:uuid:123", std::string(wsa::reply_relationship)};
  CHECK(a == b);

  b.uri = "urn:uuid:456";
  CHECK_FALSE(a == b);
}

// -- addressing_headers -------------------------------------------------------

TEST_CASE("wsa: addressing_headers default construction", "[wsa]") {
  wsa::addressing_headers h;
  CHECK_FALSE(h.to.has_value());
  CHECK_FALSE(h.action.has_value());
  CHECK_FALSE(h.message_id.has_value());
  CHECK_FALSE(h.reply_to.has_value());
  CHECK_FALSE(h.fault_to.has_value());
  CHECK_FALSE(h.from.has_value());
  CHECK(h.relates_to_list.empty());
}

TEST_CASE("wsa: addressing_headers equality", "[wsa]") {
  wsa::addressing_headers a;
  a.to = "http://example.org/service";
  a.action = "http://example.org/DoSomething";
  a.message_id = "urn:uuid:abc-123";
  a.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};

  wsa::addressing_headers b = a;
  CHECK(a == b);

  b.action = "http://example.org/DoOther";
  CHECK_FALSE(a == b);
}

TEST_CASE("wsa: addressing_headers with relates_to", "[wsa]") {
  wsa::addressing_headers h;
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-2", "http://example.org/custom"});

  CHECK(h.relates_to_list.size() == 2);
  CHECK(h.relates_to_list[0].uri == "urn:uuid:req-1");
  CHECK(h.relates_to_list[1].relationship_type == "http://example.org/custom");
}
