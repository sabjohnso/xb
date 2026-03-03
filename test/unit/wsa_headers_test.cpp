#include <xb/wsa_headers.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>

namespace wsa = xb::wsa;
namespace soap = xb::soap;

// Helper: find a header block with a given local name in the WS-A namespace
static const soap::header_block*
find_wsa_header(const soap::envelope& env, const std::string& local) {
  for (const auto& hb : env.headers) {
    if (hb.content.name().namespace_uri() == wsa::wsa_ns &&
        hb.content.name().local_name() == local) {
      return &hb;
    }
  }
  return nullptr;
}

// Helper: get text content of an any_element
static std::string
text_content(const xb::any_element& elem) {
  std::string result;
  for (const auto& child : elem.children()) {
    if (auto* text = std::get_if<std::string>(&child)) { result += *text; }
  }
  return result;
}

// Helper: serialize envelope to XML and parse it back
static soap::envelope
round_trip_envelope(const soap::envelope& env) {
  std::ostringstream os;
  xb::ostream_writer writer(os);
  soap::write_envelope(writer, env);
  auto xml = os.str();

  xb::expat_reader reader(xml);
  reader.read();
  return soap::read_envelope(reader);
}

// -- add_addressing_headers ---------------------------------------------------

TEST_CASE("wsa headers: add To header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.to = "http://example.org/service";

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "To");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
  CHECK(text_content(hb->content) == "http://example.org/service");
}

TEST_CASE("wsa headers: add Action header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.action = "http://example.org/DoSomething";

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "Action");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
  CHECK(text_content(hb->content) == "http://example.org/DoSomething");
}

TEST_CASE("wsa headers: add MessageID header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.message_id = "urn:uuid:abc-123";

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "MessageID");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
  CHECK(text_content(hb->content) == "urn:uuid:abc-123");
}

TEST_CASE("wsa headers: add ReplyTo EPR header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "ReplyTo");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);

  // Should contain <wsa:Address>...</wsa:Address> child
  REQUIRE(hb->content.children().size() >= 1);
  auto* addr_elem = std::get_if<xb::any_element>(&hb->content.children()[0]);
  REQUIRE(addr_elem != nullptr);
  CHECK(addr_elem->name().local_name() == "Address");
  CHECK(addr_elem->name().namespace_uri() == wsa::wsa_ns);
  CHECK(text_content(*addr_elem) == wsa::anonymous_uri);
}

TEST_CASE("wsa headers: add FaultTo EPR header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.fault_to = wsa::endpoint_reference{"http://example.org/fault"};

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "FaultTo");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
}

TEST_CASE("wsa headers: add From EPR header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.from = wsa::endpoint_reference{"http://example.org/client"};

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "From");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
}

TEST_CASE("wsa headers: add RelatesTo header", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});

  wsa::add_addressing_headers(env, h);

  auto* hb = find_wsa_header(env, "RelatesTo");
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
  CHECK(text_content(hb->content) == "urn:uuid:req-1");
}

TEST_CASE("wsa headers: multiple RelatesTo headers", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-2", "http://example.org/custom"});

  wsa::add_addressing_headers(env, h);

  int count = 0;
  for (const auto& hb : env.headers) {
    if (hb.content.name().namespace_uri() == wsa::wsa_ns &&
        hb.content.name().local_name() == "RelatesTo") {
      count++;
    }
  }
  CHECK(count == 2);
}

TEST_CASE("wsa headers: empty headers produces no header blocks",
          "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;

  wsa::add_addressing_headers(env, h);

  CHECK(env.headers.empty());
}

// -- extract_addressing_headers -----------------------------------------------

TEST_CASE("wsa headers: extract To from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.to = "http://example.org/service";
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.to.has_value());
  CHECK(*extracted.to == "http://example.org/service");
}

TEST_CASE("wsa headers: extract Action from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.action = "http://example.org/DoSomething";
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.action.has_value());
  CHECK(*extracted.action == "http://example.org/DoSomething");
}

TEST_CASE("wsa headers: extract MessageID from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.message_id = "urn:uuid:abc-123";
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.message_id.has_value());
  CHECK(*extracted.message_id == "urn:uuid:abc-123");
}

TEST_CASE("wsa headers: extract ReplyTo EPR from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.reply_to.has_value());
  CHECK(extracted.reply_to->address == wsa::anonymous_uri);
}

TEST_CASE("wsa headers: extract FaultTo EPR from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.fault_to = wsa::endpoint_reference{"http://example.org/fault"};
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.fault_to.has_value());
  CHECK(extracted.fault_to->address == "http://example.org/fault");
}

TEST_CASE("wsa headers: extract From EPR from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.from = wsa::endpoint_reference{"http://example.org/client"};
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.from.has_value());
  CHECK(extracted.from->address == "http://example.org/client");
}

TEST_CASE("wsa headers: extract RelatesTo from envelope", "[wsa_headers]") {
  soap::envelope env;
  wsa::addressing_headers h;
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.relates_to_list.size() == 1);
  CHECK(extracted.relates_to_list[0].uri == "urn:uuid:req-1");
}

// -- Round-trip ---------------------------------------------------------------

TEST_CASE("wsa headers: round-trip add then extract", "[wsa_headers]") {
  wsa::addressing_headers original;
  original.to = "http://example.org/service";
  original.action = "http://example.org/DoSomething";
  original.message_id = "urn:uuid:abc-123";
  original.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};
  original.fault_to = wsa::endpoint_reference{"http://example.org/fault"};
  original.from = wsa::endpoint_reference{"http://example.org/client"};
  original.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});

  soap::envelope env;
  wsa::add_addressing_headers(env, original);

  auto extracted = wsa::extract_addressing_headers(env);
  CHECK(extracted.to == original.to);
  CHECK(extracted.action == original.action);
  CHECK(extracted.message_id == original.message_id);
  REQUIRE(extracted.reply_to.has_value());
  CHECK(extracted.reply_to->address == original.reply_to->address);
  REQUIRE(extracted.fault_to.has_value());
  CHECK(extracted.fault_to->address == original.fault_to->address);
  REQUIRE(extracted.from.has_value());
  CHECK(extracted.from->address == original.from->address);
  REQUIRE(extracted.relates_to_list.size() == 1);
  CHECK(extracted.relates_to_list[0].uri == original.relates_to_list[0].uri);
}

TEST_CASE("wsa headers: full SOAP envelope XML round-trip", "[wsa_headers]") {
  wsa::addressing_headers original;
  original.to = "http://example.org/service";
  original.action = "http://example.org/DoSomething";
  original.message_id = "urn:uuid:abc-123";
  original.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wsa::add_addressing_headers(env, original);

  // Serialize to XML and parse back
  auto parsed = round_trip_envelope(env);

  auto extracted = wsa::extract_addressing_headers(parsed);
  CHECK(extracted.to == original.to);
  CHECK(extracted.action == original.action);
  CHECK(extracted.message_id == original.message_id);
  REQUIRE(extracted.reply_to.has_value());
  CHECK(extracted.reply_to->address == original.reply_to->address);
}

TEST_CASE("wsa headers: non-WS-A headers are ignored by extract",
          "[wsa_headers]") {
  soap::envelope env;

  // Add a non-WS-A header
  soap::header_block custom;
  custom.content = xb::any_element(xb::qname("urn:custom", "Token"), {},
                                   {std::string("secret")});
  custom.must_understand = false;
  env.headers.push_back(std::move(custom));

  // Add a WS-A header
  wsa::addressing_headers h;
  h.action = "urn:test";
  wsa::add_addressing_headers(env, h);

  auto extracted = wsa::extract_addressing_headers(env);
  REQUIRE(extracted.action.has_value());
  CHECK(*extracted.action == "urn:test");
  CHECK_FALSE(extracted.to.has_value());
}

TEST_CASE("wsa headers: all WS-A headers have must_understand true",
          "[wsa_headers]") {
  wsa::addressing_headers h;
  h.to = "http://example.org/service";
  h.action = "http://example.org/DoSomething";
  h.message_id = "urn:uuid:abc-123";
  h.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};
  h.fault_to = wsa::endpoint_reference{"http://example.org/fault"};
  h.from = wsa::endpoint_reference{"http://example.org/client"};
  h.relates_to_list.push_back(
      wsa::relates_to{"urn:uuid:req-1", std::string(wsa::reply_relationship)});

  soap::envelope env;
  wsa::add_addressing_headers(env, h);

  for (const auto& hb : env.headers) {
    INFO("Header: " << hb.content.name().local_name());
    CHECK(hb.must_understand == true);
  }
}
