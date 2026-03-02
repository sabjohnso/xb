#include <xb/soap_header.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace soap = xb::soap;

// -- Handler registration and dispatch ----------------------------------------

TEST_CASE("soap header: registered handler called for matching header",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {},
                               {std::string("secret")});
  hb.must_understand = false;
  env.headers.push_back(std::move(hb));

  bool called = false;
  soap::header_pipeline pipeline;
  pipeline.add_handler(xb::qname("urn:auth", "Token"),
                       [&](const soap::header_block& h) {
                         called = true;
                         CHECK(h.content.name().local_name() == "Token");
                         return true;
                       });

  pipeline.process(env);
  CHECK(called);
}

TEST_CASE("soap header: non-matching headers skipped without error",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:other", "Something"), {}, {});
  hb.must_understand = false;
  env.headers.push_back(std::move(hb));

  soap::header_pipeline pipeline;
  // No handlers registered — should be fine since not mustUnderstand
  CHECK_NOTHROW(pipeline.process(env));
}

TEST_CASE("soap header: mustUnderstand without handler throws",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {}, {});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  soap::header_pipeline pipeline;
  // No handler for Token — mustUnderstand should cause exception
  CHECK_THROWS_AS(pipeline.process(env), soap::soap_fault_exception);
}

TEST_CASE("soap header: soap_fault_exception carries appropriate fault 1.2",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {}, {});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  soap::header_pipeline pipeline;
  try {
    pipeline.process(env);
    FAIL("expected soap_fault_exception");
  } catch (const soap::soap_fault_exception& e) {
    CHECK(std::holds_alternative<soap::fault_1_2>(e.fault_value));
    auto& f = std::get<soap::fault_1_2>(e.fault_value);
    CHECK(f.code.value == "soap:MustUnderstand");
  }
}

TEST_CASE("soap header: soap_fault_exception carries appropriate fault 1.1",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {}, {});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  soap::header_pipeline pipeline;
  try {
    pipeline.process(env);
    FAIL("expected soap_fault_exception");
  } catch (const soap::soap_fault_exception& e) {
    CHECK(std::holds_alternative<soap::fault_1_1>(e.fault_value));
    auto& f = std::get<soap::fault_1_1>(e.fault_value);
    CHECK(f.fault_code == "soap:MustUnderstand");
  }
}

TEST_CASE("soap header: multiple handlers each called for their qname",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb1;
  hb1.content = xb::any_element(xb::qname("urn:auth", "Token"), {}, {});
  hb1.must_understand = true;
  env.headers.push_back(std::move(hb1));

  soap::header_block hb2;
  hb2.content = xb::any_element(xb::qname("urn:trace", "TraceId"), {}, {});
  hb2.must_understand = true;
  env.headers.push_back(std::move(hb2));

  bool token_called = false;
  bool trace_called = false;

  soap::header_pipeline pipeline;
  pipeline.add_handler(xb::qname("urn:auth", "Token"),
                       [&](const soap::header_block&) {
                         token_called = true;
                         return true;
                       });
  pipeline.add_handler(xb::qname("urn:trace", "TraceId"),
                       [&](const soap::header_block&) {
                         trace_called = true;
                         return true;
                       });

  pipeline.process(env);
  CHECK(token_called);
  CHECK(trace_called);
}

TEST_CASE("soap header: handler returning false leaves mustUnderstand "
          "unhandled",
          "[soap_header]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {}, {});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  soap::header_pipeline pipeline;
  pipeline.add_handler(xb::qname("urn:auth", "Token"),
                       [&](const soap::header_block&) {
                         return false; // Did NOT understand it
                       });

  CHECK_THROWS_AS(pipeline.process(env), soap::soap_fault_exception);
}
