#include <xb/soap_fault.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace soap = xb::soap;

namespace {

  // Helper: write a fault inside a SOAP Body wrapper
  std::string
  serialize_fault(const soap::fault& f, soap::soap_version ver) {
    std::ostringstream os;
    xb::ostream_writer writer(os);
    soap::write_fault(writer, f, ver);
    return os.str();
  }

  // Helper: parse a fault from XML (reader must be positioned at Fault start)
  soap::fault
  parse_fault(const std::string& xml, soap::soap_version ver) {
    xb::expat_reader reader(xml);
    while (reader.read()) {
      if (reader.node_type() == xb::xml_node_type::start_element) {
        return soap::read_fault(reader, ver);
      }
    }
    throw std::runtime_error("no root element");
  }

} // namespace

// -- SOAP 1.1 Fault -----------------------------------------------------------

TEST_CASE("soap fault: 1.1 round-trip with all fields", "[soap_fault]") {
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "Internal error";
  f.fault_actor = "http://example.org/actor";
  f.detail = xb::any_element(xb::qname("urn:app", "ErrorInfo"), {},
                             {std::string("details here")});

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_1);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_1);

  REQUIRE(std::holds_alternative<soap::fault_1_1>(parsed));
  auto& p = std::get<soap::fault_1_1>(parsed);
  CHECK(p.fault_code == "soap:Server");
  CHECK(p.fault_string == "Internal error");
  CHECK(p.fault_actor == "http://example.org/actor");
  REQUIRE(p.detail.has_value());
  CHECK(p.detail->name().local_name() == "ErrorInfo");
}

TEST_CASE("soap fault: 1.1 without optional fields", "[soap_fault]") {
  soap::fault_1_1 f;
  f.fault_code = "soap:Client";
  f.fault_string = "Bad request";

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_1);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_1);

  REQUIRE(std::holds_alternative<soap::fault_1_1>(parsed));
  auto& p = std::get<soap::fault_1_1>(parsed);
  CHECK(p.fault_code == "soap:Client");
  CHECK(p.fault_string == "Bad request");
  CHECK(p.fault_actor.empty());
  CHECK_FALSE(p.detail.has_value());
}

// -- SOAP 1.2 Fault -----------------------------------------------------------

TEST_CASE("soap fault: 1.2 round-trip with code and reason", "[soap_fault]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Sender";

  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Validation failed";
  f.reason.push_back(std::move(rt));

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_2);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_2);

  REQUIRE(std::holds_alternative<soap::fault_1_2>(parsed));
  auto& p = std::get<soap::fault_1_2>(parsed);
  CHECK(p.code.value == "soap:Sender");
  REQUIRE(p.reason.size() == 1);
  CHECK(p.reason[0].lang == "en");
  CHECK(p.reason[0].text == "Validation failed");
}

TEST_CASE("soap fault: 1.2 with subcode", "[soap_fault]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Sender";
  f.code.subcode.emplace();
  f.code.subcode->value = "app:ValidationError";

  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Error";
  f.reason.push_back(std::move(rt));

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_2);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_2);

  REQUIRE(std::holds_alternative<soap::fault_1_2>(parsed));
  auto& p = std::get<soap::fault_1_2>(parsed);
  CHECK(p.code.value == "soap:Sender");
  REQUIRE(p.code.subcode.has_value());
  CHECK(p.code.subcode->value == "app:ValidationError");
}

TEST_CASE("soap fault: 1.2 recursive subcode 3 levels deep", "[soap_fault]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Sender";
  f.code.subcode.emplace();
  f.code.subcode->value = "app:Level1";
  f.code.subcode->subcode = std::make_unique<soap::fault_subcode>();
  f.code.subcode->subcode->value = "app:Level2";
  f.code.subcode->subcode->subcode = std::make_unique<soap::fault_subcode>();
  f.code.subcode->subcode->subcode->value = "app:Level3";

  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Deep error";
  f.reason.push_back(std::move(rt));

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_2);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_2);

  REQUIRE(std::holds_alternative<soap::fault_1_2>(parsed));
  auto& p = std::get<soap::fault_1_2>(parsed);
  REQUIRE(p.code.subcode.has_value());
  CHECK(p.code.subcode->value == "app:Level1");
  REQUIRE(p.code.subcode->subcode != nullptr);
  CHECK(p.code.subcode->subcode->value == "app:Level2");
  REQUIRE(p.code.subcode->subcode->subcode != nullptr);
  CHECK(p.code.subcode->subcode->subcode->value == "app:Level3");
  CHECK(p.code.subcode->subcode->subcode->subcode == nullptr);
}

TEST_CASE("soap fault: 1.2 with detail", "[soap_fault]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Receiver";

  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Server error";
  f.reason.push_back(std::move(rt));

  f.detail = xb::any_element(xb::qname("urn:app", "ErrorDetail"), {},
                             {std::string("stack trace here")});

  soap::fault flt = f;
  std::string xml = serialize_fault(flt, soap::soap_version::v1_2);
  soap::fault parsed = parse_fault(xml, soap::soap_version::v1_2);

  REQUIRE(std::holds_alternative<soap::fault_1_2>(parsed));
  auto& p = std::get<soap::fault_1_2>(parsed);
  REQUIRE(p.detail.has_value());
  CHECK(p.detail->name().local_name() == "ErrorDetail");
}

// -- read_fault validation ----------------------------------------------------

TEST_CASE("soap fault: read_fault throws on non-Fault element",
          "[soap_fault]") {
  std::string xml = R"(
    <soap:Body xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
      <app:Result xmlns:app="urn:app">ok</app:Result>
    </soap:Body>)";

  xb::expat_reader reader(xml);
  while (reader.read()) {
    if (reader.node_type() == xb::xml_node_type::start_element) {
      CHECK_THROWS_AS(soap::read_fault(reader, soap::soap_version::v1_2),
                      std::runtime_error);
      break;
    }
  }
}

// -- write_fault version/variant mismatch ------------------------------------

TEST_CASE("soap fault: write_fault throws on 1.1 version with 1.2 fault",
          "[soap_fault]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Sender";
  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Error";
  f.reason.push_back(std::move(rt));

  soap::fault flt = f;
  std::ostringstream os;
  xb::ostream_writer writer(os);
  CHECK_THROWS_AS(soap::write_fault(writer, flt, soap::soap_version::v1_1),
                  std::runtime_error);
}

TEST_CASE("soap fault: write_fault throws on 1.2 version with 1.1 fault",
          "[soap_fault]") {
  soap::fault_1_1 f;
  f.fault_code = "soap:Client";
  f.fault_string = "Bad request";

  soap::fault flt = f;
  std::ostringstream os;
  xb::ostream_writer writer(os);
  CHECK_THROWS_AS(soap::write_fault(writer, flt, soap::soap_version::v1_2),
                  std::runtime_error);
}

// -- is_fault detection -------------------------------------------------------

TEST_CASE("soap fault: is_fault detects 1.1 fault", "[soap_fault]") {
  xb::any_element fault_elem(
      xb::qname("http://schemas.xmlsoap.org/soap/envelope/", "Fault"), {}, {});
  CHECK(soap::is_fault(fault_elem, soap::soap_version::v1_1));

  xb::any_element other_elem(xb::qname("urn:app", "Result"), {}, {});
  CHECK_FALSE(soap::is_fault(other_elem, soap::soap_version::v1_1));
}

TEST_CASE("soap fault: is_fault detects 1.2 fault", "[soap_fault]") {
  xb::any_element fault_elem(
      xb::qname("http://www.w3.org/2003/05/soap-envelope", "Fault"), {}, {});
  CHECK(soap::is_fault(fault_elem, soap::soap_version::v1_2));

  xb::any_element other_elem(xb::qname("urn:app", "Result"), {}, {});
  CHECK_FALSE(soap::is_fault(other_elem, soap::soap_version::v1_2));
}
