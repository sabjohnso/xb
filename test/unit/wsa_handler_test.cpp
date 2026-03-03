#include <xb/wsa_handler.hpp>
#include <xb/wsa_headers.hpp>

#include <catch2/catch_test_macros.hpp>

namespace wsa = xb::wsa;
namespace soap = xb::soap;

// -- register_wsa_handlers ----------------------------------------------------

TEST_CASE("wsa handler: WS-A mustUnderstand headers don't fault when "
          "handlers registered",
          "[wsa_handler]") {
  wsa::addressing_headers original;
  original.to = "http://example.org/service";
  original.action = "http://example.org/DoSomething";
  original.message_id = "urn:uuid:abc-123";

  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  wsa::add_addressing_headers(env, original);

  wsa::addressing_headers target;
  soap::header_pipeline pipeline;
  wsa::register_wsa_handlers(pipeline, target);

  CHECK_NOTHROW(pipeline.process(env));
}

TEST_CASE("wsa handler: extracted values match expectations", "[wsa_handler]") {
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
  env.version = soap::soap_version::v1_2;
  wsa::add_addressing_headers(env, original);

  wsa::addressing_headers target;
  soap::header_pipeline pipeline;
  wsa::register_wsa_handlers(pipeline, target);
  pipeline.process(env);

  REQUIRE(target.to.has_value());
  CHECK(*target.to == "http://example.org/service");

  REQUIRE(target.action.has_value());
  CHECK(*target.action == "http://example.org/DoSomething");

  REQUIRE(target.message_id.has_value());
  CHECK(*target.message_id == "urn:uuid:abc-123");

  REQUIRE(target.reply_to.has_value());
  CHECK(target.reply_to->address == wsa::anonymous_uri);

  REQUIRE(target.fault_to.has_value());
  CHECK(target.fault_to->address == "http://example.org/fault");

  REQUIRE(target.from.has_value());
  CHECK(target.from->address == "http://example.org/client");

  REQUIRE(target.relates_to_list.size() == 1);
  CHECK(target.relates_to_list[0].uri == "urn:uuid:req-1");
}

TEST_CASE("wsa handler: non-WS-A mustUnderstand headers still fault",
          "[wsa_handler]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  // Add a non-WS-A mustUnderstand header
  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:custom", "Token"), {},
                               {std::string("secret")});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  wsa::addressing_headers target;
  soap::header_pipeline pipeline;
  wsa::register_wsa_handlers(pipeline, target);

  CHECK_THROWS_AS(pipeline.process(env), soap::soap_fault_exception);
}

TEST_CASE("wsa handler: multiple WS-A headers all processed", "[wsa_handler]") {
  wsa::addressing_headers original;
  original.to = "http://example.org/service";
  original.action = "http://example.org/DoSomething";
  original.message_id = "urn:uuid:abc-123";
  original.reply_to = wsa::endpoint_reference{std::string(wsa::anonymous_uri)};

  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  wsa::add_addressing_headers(env, original);

  wsa::addressing_headers target;
  soap::header_pipeline pipeline;
  wsa::register_wsa_handlers(pipeline, target);
  pipeline.process(env);

  // All four headers should be extracted
  CHECK(target.to.has_value());
  CHECK(target.action.has_value());
  CHECK(target.message_id.has_value());
  CHECK(target.reply_to.has_value());
}

TEST_CASE("wsa handler: mixed WS-A and non-WS-A headers, non-WS-A "
          "not mustUnderstand",
          "[wsa_handler]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  // non-WS-A header, not mustUnderstand
  soap::header_block custom;
  custom.content = xb::any_element(xb::qname("urn:custom", "Trace"), {},
                                   {std::string("trace-id")});
  custom.must_understand = false;
  env.headers.push_back(std::move(custom));

  // WS-A headers
  wsa::addressing_headers h;
  h.action = "urn:test";
  wsa::add_addressing_headers(env, h);

  wsa::addressing_headers target;
  soap::header_pipeline pipeline;
  wsa::register_wsa_handlers(pipeline, target);

  CHECK_NOTHROW(pipeline.process(env));
  REQUIRE(target.action.has_value());
  CHECK(*target.action == "urn:test");
}
