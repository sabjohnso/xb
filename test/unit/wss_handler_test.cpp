#include <xb/wss_handler.hpp>
#include <xb/wss_headers.hpp>

#include <xb/wsa.hpp>
#include <xb/wsa_handler.hpp>
#include <xb/wsa_headers.hpp>

#include <catch2/catch_test_macros.hpp>

namespace wss = xb::wss;
namespace wsa = xb::wsa;
namespace soap = xb::soap;

// -- register_wss_handlers ----------------------------------------------------

TEST_CASE("wss handler: WSS mustUnderstand header doesn't fault when "
          "handler registered",
          "[wss_handler]") {
  wss::security_header original;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  original.ts = ts;

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wss::add_security_header(env, original);

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);

  CHECK_NOTHROW(pipeline.process(env));
}

TEST_CASE("wss handler: extracted values match expectations", "[wss_handler]") {
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

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wss::add_security_header(env, original);

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);
  pipeline.process(env);

  REQUIRE(target.username.has_value());
  CHECK(target.username->username == "alice");
  CHECK(target.username->password == "secret");
  REQUIRE(target.username->nonce.has_value());
  CHECK(*target.username->nonce == "bm9uY2U=");

  REQUIRE(target.ts.has_value());
  CHECK(target.ts->created == "2026-03-01T00:00:00Z");
  REQUIRE(target.ts->expires.has_value());
  CHECK(*target.ts->expires == "2026-03-01T00:05:00Z");
}

TEST_CASE("wss handler: coexists with WSA handlers", "[wss_handler]") {
  // Build an envelope with both WSS and WSA headers
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  wss::security_header wss_original;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  wss_original.ts = ts;
  wss::add_security_header(env, wss_original);

  wsa::addressing_headers wsa_original;
  wsa_original.to = "http://example.org/service";
  wsa_original.action = "http://example.org/DoSomething";
  wsa::add_addressing_headers(env, wsa_original);

  // Register both handlers
  wss::security_header wss_target;
  wsa::addressing_headers wsa_target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, wss_target);
  wsa::register_wsa_handlers(pipeline, wsa_target);

  CHECK_NOTHROW(pipeline.process(env));

  REQUIRE(wss_target.ts.has_value());
  CHECK(wss_target.ts->created == "2026-03-01T00:00:00Z");

  REQUIRE(wsa_target.to.has_value());
  CHECK(*wsa_target.to == "http://example.org/service");
  REQUIRE(wsa_target.action.has_value());
  CHECK(*wsa_target.action == "http://example.org/DoSomething");
}

TEST_CASE("wss handler: non-WSS mustUnderstand headers still fault",
          "[wss_handler]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:custom", "Token"), {},
                               {std::string("secret")});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);

  CHECK_THROWS_AS(pipeline.process(env), soap::soap_fault_exception);
}

TEST_CASE("wss handler: BinarySecurityToken extracted through pipeline",
          "[wss_handler]") {
  wss::security_header original;
  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...cert...";
  bst.wsu_id = "X509Token";
  original.binary_token = bst;

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wss::add_security_header(env, original);

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);
  pipeline.process(env);

  REQUIRE(target.binary_token.has_value());
  CHECK(target.binary_token->value_type == wss::x509_token_type);
  CHECK(target.binary_token->value == "MIIB...cert...");
  REQUIRE(target.binary_token->wsu_id.has_value());
  CHECK(*target.binary_token->wsu_id == "X509Token");
}

TEST_CASE("wss handler: SOAP 1.1 processing", "[wss_handler]") {
  wss::security_header original;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  original.username = ut;

  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  wss::add_security_header(env, original);

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);

  CHECK_NOTHROW(pipeline.process(env));

  REQUIRE(target.username.has_value());
  CHECK(target.username->username == "alice");
}

TEST_CASE("wss handler: mixed WSS and non-WSS headers, non-WSS "
          "not mustUnderstand",
          "[wss_handler]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  // non-WSS header, not mustUnderstand
  soap::header_block custom;
  custom.content = xb::any_element(xb::qname("urn:custom", "Trace"), {},
                                   {std::string("trace-id")});
  custom.must_understand = false;
  env.headers.push_back(std::move(custom));

  // WSS header
  wss::security_header original;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  original.ts = ts;
  wss::add_security_header(env, original);

  wss::security_header target;
  soap::header_pipeline pipeline;
  wss::register_wss_handlers(pipeline, target);

  CHECK_NOTHROW(pipeline.process(env));
  REQUIRE(target.ts.has_value());
  CHECK(target.ts->created == "2026-03-01T00:00:00Z");
}
