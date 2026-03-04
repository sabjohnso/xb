#include <xb/wss_headers.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>

namespace wss = xb::wss;
namespace soap = xb::soap;

// Helper: find the Security header block
static const soap::header_block*
find_security_header(const soap::envelope& env) {
  for (const auto& hb : env.headers) {
    if (hb.content.name().namespace_uri() == wss::wsse_ns &&
        hb.content.name().local_name() == "Security") {
      return &hb;
    }
  }
  return nullptr;
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

// -- add UsernameToken --------------------------------------------------------

TEST_CASE("wss headers: add UsernameToken with text password",
          "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  sh.username = ut;

  wss::add_security_header(env, sh);

  auto* hb = find_security_header(env);
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "alice");
  CHECK(extracted.username->password == "secret");
  CHECK(extracted.username->password_type == wss::password_text_type);
}

TEST_CASE("wss headers: add UsernameToken with digest password",
          "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "digest-value";
  ut.password_type = std::string(wss::password_digest_type);
  ut.nonce = "dGVzdG5vbmNl";
  ut.created = "2026-03-01T00:00:00Z";
  sh.username = ut;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->password_type == wss::password_digest_type);
  REQUIRE(extracted.username->nonce.has_value());
  CHECK(*extracted.username->nonce == "dGVzdG5vbmNl");
  REQUIRE(extracted.username->created.has_value());
  CHECK(*extracted.username->created == "2026-03-01T00:00:00Z");
}

// -- add Timestamp ------------------------------------------------------------

TEST_CASE("wss headers: add Timestamp created only", "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  sh.ts = ts;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-03-01T00:00:00Z");
  CHECK_FALSE(extracted.ts->expires.has_value());
}

TEST_CASE("wss headers: add Timestamp created and expires", "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  ts.expires = "2026-03-01T00:05:00Z";
  sh.ts = ts;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-03-01T00:00:00Z");
  REQUIRE(extracted.ts->expires.has_value());
  CHECK(*extracted.ts->expires == "2026-03-01T00:05:00Z");
}

// -- add BinarySecurityToken --------------------------------------------------

TEST_CASE("wss headers: add BinarySecurityToken", "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...base64...";
  bst.wsu_id = "X509Token";
  sh.binary_token = bst;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.binary_token.has_value());
  CHECK(extracted.binary_token->value_type == wss::x509_token_type);
  CHECK(extracted.binary_token->encoding_type == wss::base64_encoding_type);
  CHECK(extracted.binary_token->value == "MIIB...base64...");
  REQUIRE(extracted.binary_token->wsu_id.has_value());
  CHECK(*extracted.binary_token->wsu_id == "X509Token");
}

// -- Full round-trip ----------------------------------------------------------

TEST_CASE("wss headers: full security header round-trip", "[wss_headers]") {
  wss::security_header original;

  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  ut.nonce = "bm9uY2U=";
  ut.created = "2026-03-01T00:00:00Z";
  original.username = ut;

  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  ts.expires = "2026-03-01T00:05:00Z";
  original.ts = ts;

  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...";
  bst.wsu_id = "CertToken";
  original.binary_token = bst;

  soap::envelope env;
  wss::add_security_header(env, original);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "alice");
  CHECK(extracted.username->password == "secret");
  REQUIRE(extracted.username->nonce.has_value());
  CHECK(*extracted.username->nonce == "bm9uY2U=");
  REQUIRE(extracted.username->created.has_value());
  CHECK(*extracted.username->created == "2026-03-01T00:00:00Z");

  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-03-01T00:00:00Z");
  REQUIRE(extracted.ts->expires.has_value());

  REQUIRE(extracted.binary_token.has_value());
  CHECK(extracted.binary_token->value == "MIIB...");
}

// -- SOAP envelope XML round-trip ---------------------------------------------

TEST_CASE("wss headers: SOAP envelope XML round-trip", "[wss_headers]") {
  wss::security_header original;

  wss::username_token ut;
  ut.username = "bob";
  ut.password = "pass123";
  original.username = ut;

  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  original.ts = ts;

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wss::add_security_header(env, original);

  auto parsed = round_trip_envelope(env);

  auto extracted = wss::extract_security_header(parsed);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "bob");
  CHECK(extracted.username->password == "pass123");

  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-03-01T00:00:00Z");
}

// -- Empty and edge cases -----------------------------------------------------

TEST_CASE("wss headers: empty security header produces no header blocks",
          "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;

  wss::add_security_header(env, sh);

  CHECK(env.headers.empty());
}

TEST_CASE("wss headers: security header has must_understand true",
          "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  sh.ts = ts;

  wss::add_security_header(env, sh);

  auto* hb = find_security_header(env);
  REQUIRE(hb != nullptr);
  CHECK(hb->must_understand == true);
}

TEST_CASE("wss headers: non-WSS headers ignored by extract", "[wss_headers]") {
  soap::envelope env;

  // Add a non-WSS header
  soap::header_block custom;
  custom.content = xb::any_element(xb::qname("urn:custom", "Token"), {},
                                   {std::string("secret")});
  custom.must_understand = false;
  env.headers.push_back(std::move(custom));

  auto extracted = wss::extract_security_header(env);
  CHECK_FALSE(extracted.username.has_value());
  CHECK_FALSE(extracted.ts.has_value());
  CHECK_FALSE(extracted.binary_token.has_value());
}

// -- Extract-only tests (mirroring WSA pattern) -------------------------------

TEST_CASE("wss headers: extract UsernameToken from manually built envelope",
          "[wss_headers]") {
  // Build a Security header manually using any_element
  xb::any_element username(xb::qname(wss::wsse_ns, "Username"), {},
                           {std::string("carol")});
  std::vector<xb::any_attribute> pw_attrs;
  pw_attrs.emplace_back(xb::qname("", "Type"),
                        std::string(wss::password_text_type));
  xb::any_element password(xb::qname(wss::wsse_ns, "Password"),
                           std::move(pw_attrs), {std::string("pw123")});
  xb::any_element ut_elem(xb::qname(wss::wsse_ns, "UsernameToken"), {},
                          {std::move(username), std::move(password)});
  xb::any_element security(xb::qname(wss::wsse_ns, "Security"), {},
                           {std::move(ut_elem)});

  soap::envelope env;
  soap::header_block hb;
  hb.must_understand = true;
  hb.content = std::move(security);
  env.headers.push_back(std::move(hb));

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "carol");
  CHECK(extracted.username->password == "pw123");
  CHECK(extracted.username->password_type == wss::password_text_type);
}

TEST_CASE("wss headers: extract Timestamp from manually built envelope",
          "[wss_headers]") {
  xb::any_element created(xb::qname(wss::wsu_ns, "Created"), {},
                          {std::string("2026-01-01T00:00:00Z")});
  xb::any_element expires(xb::qname(wss::wsu_ns, "Expires"), {},
                          {std::string("2026-01-01T01:00:00Z")});
  xb::any_element ts_elem(xb::qname(wss::wsu_ns, "Timestamp"), {},
                          {std::move(created), std::move(expires)});
  xb::any_element security(xb::qname(wss::wsse_ns, "Security"), {},
                           {std::move(ts_elem)});

  soap::envelope env;
  soap::header_block hb;
  hb.must_understand = true;
  hb.content = std::move(security);
  env.headers.push_back(std::move(hb));

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-01-01T00:00:00Z");
  REQUIRE(extracted.ts->expires.has_value());
  CHECK(*extracted.ts->expires == "2026-01-01T01:00:00Z");
}

TEST_CASE("wss headers: extract BinarySecurityToken from manually built "
          "envelope",
          "[wss_headers]") {
  std::vector<xb::any_attribute> attrs;
  attrs.emplace_back(xb::qname("", "ValueType"),
                     std::string(wss::x509_token_type));
  attrs.emplace_back(xb::qname("", "EncodingType"),
                     std::string(wss::base64_encoding_type));
  xb::any_element bst_elem(xb::qname(wss::wsse_ns, "BinarySecurityToken"),
                           std::move(attrs), {std::string("MIIC...")});
  xb::any_element security(xb::qname(wss::wsse_ns, "Security"), {},
                           {std::move(bst_elem)});

  soap::envelope env;
  soap::header_block hb;
  hb.must_understand = true;
  hb.content = std::move(security);
  env.headers.push_back(std::move(hb));

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.binary_token.has_value());
  CHECK(extracted.binary_token->value_type == wss::x509_token_type);
  CHECK(extracted.binary_token->encoding_type == wss::base64_encoding_type);
  CHECK(extracted.binary_token->value == "MIIC...");
  CHECK_FALSE(extracted.binary_token->wsu_id.has_value());
}

// -- password_type round-trip -------------------------------------------------

TEST_CASE("wss headers: password_type round-trips correctly", "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "digest-value";
  ut.password_type = std::string(wss::password_digest_type);
  sh.username = ut;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->password_type == wss::password_digest_type);
}

// -- BinarySecurityToken without wsu_id ---------------------------------------

TEST_CASE("wss headers: BinarySecurityToken without wsu_id round-trips",
          "[wss_headers]") {
  soap::envelope env;
  wss::security_header sh;
  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...";
  // No wsu_id set
  sh.binary_token = bst;

  wss::add_security_header(env, sh);

  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.binary_token.has_value());
  CHECK(extracted.binary_token->value == "MIIB...");
  CHECK_FALSE(extracted.binary_token->wsu_id.has_value());
}

// -- Duplicate add_security_header calls --------------------------------------

TEST_CASE("wss headers: duplicate add_security_header creates two blocks",
          "[wss_headers]") {
  soap::envelope env;

  wss::security_header sh1;
  wss::timestamp ts1;
  ts1.created = "2026-01-01T00:00:00Z";
  sh1.ts = ts1;
  wss::add_security_header(env, sh1);

  wss::security_header sh2;
  wss::username_token ut;
  ut.username = "bob";
  ut.password = "pass";
  sh2.username = ut;
  wss::add_security_header(env, sh2);

  // Two Security header blocks should exist
  int count = 0;
  for (const auto& hb : env.headers) {
    if (hb.content.name().namespace_uri() == wss::wsse_ns &&
        hb.content.name().local_name() == "Security") {
      ++count;
    }
  }
  CHECK(count == 2);

  // extract_security_header sees the last one's username (second block)
  auto extracted = wss::extract_security_header(env);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "bob");
  // But also sees the first block's timestamp
  REQUIRE(extracted.ts.has_value());
  CHECK(extracted.ts->created == "2026-01-01T00:00:00Z");
}

// -- SOAP 1.1 XML round-trip --------------------------------------------------

TEST_CASE("wss headers: SOAP 1.1 envelope XML round-trip", "[wss_headers]") {
  wss::security_header original;

  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  original.username = ut;

  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  wss::add_security_header(env, original);

  auto parsed = round_trip_envelope(env);

  auto extracted = wss::extract_security_header(parsed);
  REQUIRE(extracted.username.has_value());
  CHECK(extracted.username->username == "alice");
  CHECK(extracted.username->password == "secret");
}
