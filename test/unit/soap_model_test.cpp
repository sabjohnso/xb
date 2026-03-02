#include <xb/soap_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace soap = xb::soap;

// -- soap_version -------------------------------------------------------------

TEST_CASE("soap model: version enum values", "[soap_model]") {
  CHECK(soap::soap_version::v1_1 != soap::soap_version::v1_2);
}

// -- header_block -------------------------------------------------------------

TEST_CASE("soap model: header_block default construction", "[soap_model]") {
  soap::header_block hb;
  CHECK(hb.must_understand == false);
  CHECK(hb.role.empty());
}

TEST_CASE("soap model: header_block with values", "[soap_model]") {
  xb::any_element content(xb::qname("urn:example", "Auth"), {}, {});
  soap::header_block hb;
  hb.content = content;
  hb.must_understand = true;
  hb.role = "http://www.w3.org/2003/05/soap-envelope/role/next";
  CHECK(hb.must_understand == true);
  CHECK(hb.content.name().local_name() == "Auth");
}

// -- envelope -----------------------------------------------------------------

TEST_CASE("soap model: envelope default construction", "[soap_model]") {
  soap::envelope env;
  CHECK(env.headers.empty());
  CHECK(env.body.empty());
}

TEST_CASE("soap model: envelope value semantics (copy)", "[soap_model]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  env.body.emplace_back(xb::qname("urn:ex", "Payload"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  soap::envelope copy = env;
  CHECK(copy.version == soap::soap_version::v1_2);
  CHECK(copy.body.size() == 1);
  CHECK(copy.body[0].name().local_name() == "Payload");
  CHECK(copy == env);
}

TEST_CASE("soap model: envelope value semantics (move)", "[soap_model]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  env.body.emplace_back(xb::qname("urn:ex", "Data"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  soap::envelope moved = std::move(env);
  CHECK(moved.version == soap::soap_version::v1_1);
  CHECK(moved.body.size() == 1);
}

TEST_CASE("soap model: envelope equality", "[soap_model]") {
  soap::envelope a;
  a.version = soap::soap_version::v1_2;

  soap::envelope b;
  b.version = soap::soap_version::v1_1;

  CHECK_FALSE(a == b);

  b.version = soap::soap_version::v1_2;
  CHECK(a == b);
}

// -- fault_1_1 ----------------------------------------------------------------

TEST_CASE("soap model: fault_1_1 default construction", "[soap_model]") {
  soap::fault_1_1 f;
  CHECK(f.fault_code.empty());
  CHECK(f.fault_string.empty());
  CHECK(f.fault_actor.empty());
  CHECK_FALSE(f.detail.has_value());
}

TEST_CASE("soap model: fault_1_1 with values", "[soap_model]") {
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "Internal error";
  f.fault_actor = "http://example.org/actor";
  f.detail = xb::any_element(xb::qname("", "detail"), {}, {});
  CHECK(f.fault_code == "soap:Server");
  CHECK(f.detail.has_value());
}

// -- fault_subcode (recursive unique_ptr) -------------------------------------

TEST_CASE("soap model: fault_subcode default construction", "[soap_model]") {
  soap::fault_subcode sc;
  CHECK(sc.value.empty());
  CHECK(sc.subcode == nullptr);
}

TEST_CASE("soap model: fault_subcode deep copy", "[soap_model]") {
  soap::fault_subcode inner;
  inner.value = "app:InnerError";

  soap::fault_subcode outer;
  outer.value = "app:OuterError";
  outer.subcode = std::make_unique<soap::fault_subcode>(std::move(inner));

  // Copy
  soap::fault_subcode copy = outer;
  CHECK(copy.value == "app:OuterError");
  REQUIRE(copy.subcode != nullptr);
  CHECK(copy.subcode->value == "app:InnerError");
  CHECK(copy.subcode->subcode == nullptr);

  // Verify independence
  copy.subcode->value = "modified";
  CHECK(outer.subcode->value == "app:InnerError");
}

TEST_CASE("soap model: fault_subcode deep copy 3 levels", "[soap_model]") {
  soap::fault_subcode level3;
  level3.value = "L3";

  soap::fault_subcode level2;
  level2.value = "L2";
  level2.subcode = std::make_unique<soap::fault_subcode>(std::move(level3));

  soap::fault_subcode level1;
  level1.value = "L1";
  level1.subcode = std::make_unique<soap::fault_subcode>(std::move(level2));

  soap::fault_subcode copy = level1;
  CHECK(copy.value == "L1");
  REQUIRE(copy.subcode != nullptr);
  CHECK(copy.subcode->value == "L2");
  REQUIRE(copy.subcode->subcode != nullptr);
  CHECK(copy.subcode->subcode->value == "L3");
  CHECK(copy.subcode->subcode->subcode == nullptr);
}

TEST_CASE("soap model: fault_subcode equality", "[soap_model]") {
  soap::fault_subcode a;
  a.value = "X";

  soap::fault_subcode b;
  b.value = "X";

  CHECK(a == b);

  b.subcode = std::make_unique<soap::fault_subcode>();
  b.subcode->value = "Y";
  CHECK_FALSE(a == b);

  a.subcode = std::make_unique<soap::fault_subcode>();
  a.subcode->value = "Y";
  CHECK(a == b);
}

TEST_CASE("soap model: fault_subcode copy assignment", "[soap_model]") {
  soap::fault_subcode src;
  src.value = "A";
  src.subcode = std::make_unique<soap::fault_subcode>();
  src.subcode->value = "B";

  soap::fault_subcode dst;
  dst = src;
  CHECK(dst.value == "A");
  REQUIRE(dst.subcode != nullptr);
  CHECK(dst.subcode->value == "B");
}

// -- fault_code ---------------------------------------------------------------

TEST_CASE("soap model: fault_code default construction", "[soap_model]") {
  soap::fault_code fc;
  CHECK(fc.value.empty());
  CHECK_FALSE(fc.subcode.has_value());
}

// -- fault_reason_text --------------------------------------------------------

TEST_CASE("soap model: fault_reason_text", "[soap_model]") {
  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "An error occurred";
  CHECK(rt.lang == "en");
  CHECK(rt.text == "An error occurred");
}

// -- fault_1_2 ----------------------------------------------------------------

TEST_CASE("soap model: fault_1_2 default construction", "[soap_model]") {
  soap::fault_1_2 f;
  CHECK(f.code.value.empty());
  CHECK(f.reason.empty());
  CHECK(f.node.empty());
  CHECK(f.role.empty());
  CHECK_FALSE(f.detail.has_value());
}

TEST_CASE("soap model: fault_1_2 with subcode and reason", "[soap_model]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Sender";
  f.code.subcode.emplace();
  f.code.subcode->value = "app:ValidationError";

  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Validation failed";
  f.reason.push_back(std::move(rt));

  CHECK(f.code.value == "soap:Sender");
  REQUIRE(f.code.subcode.has_value());
  CHECK(f.code.subcode->value == "app:ValidationError");
  CHECK(f.reason.size() == 1);
}

// -- fault variant ------------------------------------------------------------

TEST_CASE("soap model: fault variant holds 1.1", "[soap_model]") {
  soap::fault_1_1 f11;
  f11.fault_code = "soap:Client";
  f11.fault_string = "Bad request";

  soap::fault flt = f11;
  CHECK(std::holds_alternative<soap::fault_1_1>(flt));
  CHECK(std::get<soap::fault_1_1>(flt).fault_code == "soap:Client");
}

TEST_CASE("soap model: fault variant holds 1.2", "[soap_model]") {
  soap::fault_1_2 f12;
  f12.code.value = "soap:Receiver";

  soap::fault flt = f12;
  CHECK(std::holds_alternative<soap::fault_1_2>(flt));
  CHECK(std::get<soap::fault_1_2>(flt).code.value == "soap:Receiver");
}
