#include <xb/xop.hpp>

#include <xb/xml_value.hpp>

#include <catch2/catch_test_macros.hpp>

namespace xop = xb::xop;
namespace soap = xb::soap;
namespace mime = xb::mime;

// Helper: create a SOAP envelope with a body element containing text
static soap::envelope
make_envelope_with_body(const std::string& elem_name, const std::string& text) {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  env.body.push_back(xb::any_element(xb::qname("urn:test", elem_name), {},
                                     {std::string(text)}));
  return env;
}

// Helper: generate a large base64 string from known binary data
static std::string
make_large_base64(std::size_t byte_count) {
  std::vector<std::byte> data;
  for (std::size_t i = 0; i < byte_count; ++i) {
    data.push_back(static_cast<std::byte>(i % 256));
  }
  return xb::format_base64_binary(data);
}

// -- Namespace constant -------------------------------------------------------

TEST_CASE("xop: namespace URI matches W3C spec", "[xop]") {
  CHECK(std::string(xop::xop_ns) == "http://www.w3.org/2004/08/xop/include");
}

// -- Type defaults and equality -----------------------------------------------

TEST_CASE("xop: attachment default construction", "[xop]") {
  xop::attachment att;
  CHECK(att.content_id.empty());
  CHECK(att.content_type.empty());
  CHECK(att.data.empty());
}

TEST_CASE("xop: attachment equality", "[xop]") {
  xop::attachment a;
  a.content_id = "part1";
  a.content_type = "application/octet-stream";
  a.data = {std::byte{1}, std::byte{2}};

  xop::attachment b = a;
  CHECK(a == b);

  b.content_id = "part2";
  CHECK_FALSE(a == b);
}

TEST_CASE("xop: mtom_message default construction", "[xop]") {
  xop::mtom_message msg;
  CHECK(msg.attachments.empty());
}

// -- optimize: no base64 -> no attachments ------------------------------------

TEST_CASE("xop: optimize with no base64 produces no attachments", "[xop]") {
  auto env = make_envelope_with_body("Data", "plain text content");
  auto result = xop::optimize(env);

  CHECK(result.attachments.empty());
  CHECK(result.envelope.body.size() == 1);
}

// -- optimize: small base64 preserved -----------------------------------------

TEST_CASE("xop: optimize preserves small base64 below threshold", "[xop]") {
  auto small_b64 = make_large_base64(100); // well below 1024 threshold
  auto env = make_envelope_with_body("Data", small_b64);

  auto result = xop::optimize(env, 1024);

  CHECK(result.attachments.empty());
}

// -- optimize: large base64 replaced with xop:Include -------------------------

TEST_CASE("xop: optimize replaces large base64 with xop:Include", "[xop]") {
  auto large_b64 = make_large_base64(2048); // above default threshold
  auto env = make_envelope_with_body("Data", large_b64);

  auto result = xop::optimize(env);

  REQUIRE(result.attachments.size() == 1);
  CHECK_FALSE(result.attachments[0].content_id.empty());
  CHECK(result.attachments[0].data.size() == 2048);

  // Body element should contain xop:Include child
  REQUIRE(result.envelope.body.size() == 1);
  const auto& body = result.envelope.body[0];
  REQUIRE(body.children().size() == 1);
  auto* include = std::get_if<xb::any_element>(&body.children()[0]);
  REQUIRE(include != nullptr);
  CHECK(include->name().namespace_uri() == xop::xop_ns);
  CHECK(include->name().local_name() == "Include");
}

// -- optimize: multiple base64 elements ---------------------------------------

TEST_CASE("xop: optimize handles multiple base64 elements", "[xop]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  auto b64_1 = make_large_base64(2048);
  auto b64_2 = make_large_base64(4096);
  env.body.push_back(
      xb::any_element(xb::qname("urn:test", "Att1"), {}, {std::string(b64_1)}));
  env.body.push_back(
      xb::any_element(xb::qname("urn:test", "Att2"), {}, {std::string(b64_2)}));

  auto result = xop::optimize(env);

  CHECK(result.attachments.size() == 2);
  CHECK(result.attachments[0].data.size() == 2048);
  CHECK(result.attachments[1].data.size() == 4096);
}

// -- deoptimize: restores base64 from attachments -----------------------------

TEST_CASE("xop: deoptimize restores base64 from attachments", "[xop]") {
  auto large_b64 = make_large_base64(2048);
  auto env = make_envelope_with_body("Data", large_b64);

  auto optimized = xop::optimize(env);
  auto restored = xop::deoptimize(optimized);

  REQUIRE(restored.body.size() == 1);
  REQUIRE(restored.body[0].children().size() == 1);
  auto* text = std::get_if<std::string>(&restored.body[0].children()[0]);
  REQUIRE(text != nullptr);

  // The restored base64 should decode to the same binary data
  auto original_bytes = xb::parse_base64_binary(large_b64);
  auto restored_bytes = xb::parse_base64_binary(*text);
  CHECK(original_bytes == restored_bytes);
}

// -- Round-trip: optimize -> deoptimize preserves content
// ----------------------

TEST_CASE("xop: optimize then deoptimize preserves content", "[xop]") {
  auto large_b64 = make_large_base64(2048);
  auto env = make_envelope_with_body("Data", large_b64);

  auto optimized = xop::optimize(env);
  auto restored = xop::deoptimize(optimized);

  // Body element name preserved
  CHECK(restored.body[0].name() == env.body[0].name());
}

// -- to_multipart / from_multipart --------------------------------------------

TEST_CASE("xop: to_multipart has correct part count", "[xop]") {
  auto large_b64 = make_large_base64(2048);
  auto env = make_envelope_with_body("Data", large_b64);

  auto optimized = xop::optimize(env);
  auto mp = xop::to_multipart(optimized);

  // 1 root XML part + 1 attachment
  CHECK(mp.parts.size() == 2);
  CHECK(mp.parts[0].content_type.find("application/xop+xml") !=
        std::string::npos);
}

TEST_CASE("xop: to_multipart first part is application/xop+xml", "[xop]") {
  auto env = make_envelope_with_body("Data", "small text");
  auto optimized = xop::optimize(env);
  auto mp = xop::to_multipart(optimized);

  CHECK(mp.parts.size() == 1);
  CHECK(mp.parts[0].content_type.find("application/xop+xml") !=
        std::string::npos);
}

TEST_CASE("xop: from_multipart parses envelope and collects attachments",
          "[xop]") {
  auto large_b64 = make_large_base64(2048);
  auto env = make_envelope_with_body("Data", large_b64);

  auto optimized = xop::optimize(env);
  auto mp = xop::to_multipart(optimized);
  auto parsed = xop::from_multipart(mp);

  CHECK(parsed.attachments.size() == 1);
  CHECK(parsed.envelope.body.size() == 1);
}

// -- Full round-trip: optimize -> multipart -> parse -> deoptimize ------------

TEST_CASE("xop: full round-trip through multipart", "[xop]") {
  auto large_b64 = make_large_base64(2048);
  auto env = make_envelope_with_body("Data", large_b64);

  // optimize -> to_multipart -> serialize -> parse -> from_multipart ->
  // deoptimize
  auto optimized = xop::optimize(env);
  auto mp = xop::to_multipart(optimized);

  auto serialized = mime::serialize_multipart(mp);
  auto parsed_mp = mime::parse_multipart(serialized, mp.boundary);
  auto parsed = xop::from_multipart(parsed_mp);
  auto restored = xop::deoptimize(parsed);

  // The body element name should be preserved
  REQUIRE(restored.body.size() == 1);
  CHECK(restored.body[0].name() == env.body[0].name());

  // The binary content should be preserved
  auto* text = std::get_if<std::string>(&restored.body[0].children()[0]);
  REQUIRE(text != nullptr);
  auto original_bytes = xb::parse_base64_binary(large_b64);
  auto restored_bytes = xb::parse_base64_binary(*text);
  CHECK(original_bytes == restored_bytes);
}
