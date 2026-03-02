#include <xb/soap_envelope.hpp>
#include <xb/soap_fault.hpp>
#include <xb/soap_header.hpp>
#include <xb/soap_model.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace soap = xb::soap;

namespace {

  std::string
  serialize(const soap::envelope& env) {
    std::ostringstream os;
    xb::ostream_writer writer(os);
    soap::write_envelope(writer, env);
    return os.str();
  }

  soap::envelope
  parse(const std::string& xml) {
    xb::expat_reader reader(xml);
    while (reader.read()) {
      if (reader.node_type() == xb::xml_node_type::start_element) {
        return soap::read_envelope(reader);
      }
    }
    throw std::runtime_error("no root element");
  }

  soap::fault
  parse_fault_from_body(const xb::any_element& body_child,
                        soap::soap_version ver) {
    std::ostringstream os;
    xb::ostream_writer writer(os);
    body_child.write(writer);
    std::string xml = os.str();

    xb::expat_reader reader(xml);
    while (reader.read()) {
      if (reader.node_type() == xb::xml_node_type::start_element) {
        return soap::read_fault(reader, ver);
      }
    }
    throw std::runtime_error("no fault element");
  }

} // namespace

// -- End-to-end: SOAP 1.2 envelope with headers, body, and pipeline ----------

TEST_CASE("soap integration: 1.2 envelope round-trip with header pipeline",
          "[soap_integration]") {
  // 1. Build envelope
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block auth_hdr;
  auth_hdr.content = xb::any_element(xb::qname("urn:auth", "Token"), {},
                                     {std::string("jwt-token-value")});
  auth_hdr.must_understand = true;
  auth_hdr.role = "http://www.w3.org/2003/05/soap-envelope/role/next";
  env.headers.push_back(std::move(auth_hdr));

  env.body.emplace_back(
      xb::qname("urn:api", "GetUserRequest"), std::vector<xb::any_attribute>{},
      std::vector<xb::any_element::child>{xb::any_element(
          xb::qname("urn:api", "UserId"), {}, {std::string("42")})});

  // 2. Serialize to XML
  std::string xml = serialize(env);
  CHECK_FALSE(xml.empty());

  // 3. Parse back
  soap::envelope parsed = parse(xml);
  CHECK(parsed.version == soap::soap_version::v1_2);
  CHECK(parsed == env);

  // 4. Run through header pipeline
  bool auth_processed = false;
  soap::header_pipeline pipeline;
  pipeline.add_handler(xb::qname("urn:auth", "Token"),
                       [&](const soap::header_block& hb) {
                         auth_processed = true;
                         CHECK(hb.must_understand == true);
                         return true;
                       });

  CHECK_NOTHROW(pipeline.process(parsed));
  CHECK(auth_processed);

  // 5. Verify body payload
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed.body[0].name().local_name() == "GetUserRequest");
  CHECK(parsed.body[0].name().namespace_uri() == "urn:api");
}

// -- End-to-end: SOAP 1.1 envelope with fault ---------------------------------

TEST_CASE("soap integration: 1.1 envelope with fault round-trip",
          "[soap_integration]") {
  // 1. Build envelope with fault in body
  soap::envelope env;
  env.version = soap::soap_version::v1_1;

  // Build the fault
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "Database connection failed";
  f.fault_actor = "http://example.org/db-service";
  f.detail = xb::any_element(xb::qname("urn:errors", "DbError"), {},
                             {std::string("timeout after 30s")});

  // Serialize fault to create body child
  std::ostringstream fault_os;
  xb::ostream_writer fault_writer(fault_os);
  soap::write_fault(fault_writer, f, soap::soap_version::v1_1);
  std::string fault_xml = fault_os.str();

  // Parse fault XML into any_element for body
  xb::expat_reader fault_reader(fault_xml);
  while (fault_reader.read()) {
    if (fault_reader.node_type() == xb::xml_node_type::start_element) {
      env.body.emplace_back(fault_reader);
      break;
    }
  }

  // 2. Serialize envelope
  std::string xml = serialize(env);

  // 3. Parse back
  soap::envelope parsed = parse(xml);
  CHECK(parsed.version == soap::soap_version::v1_1);

  // 4. Detect fault in body
  REQUIRE(parsed.body.size() == 1);
  CHECK(soap::is_fault(parsed.body[0], soap::soap_version::v1_1));

  // 5. Parse the fault from body
  soap::fault parsed_fault =
      parse_fault_from_body(parsed.body[0], soap::soap_version::v1_1);
  REQUIRE(std::holds_alternative<soap::fault_1_1>(parsed_fault));
  auto& pf = std::get<soap::fault_1_1>(parsed_fault);
  CHECK(pf.fault_code == "soap:Server");
  CHECK(pf.fault_string == "Database connection failed");
  CHECK(pf.fault_actor == "http://example.org/db-service");
  REQUIRE(pf.detail.has_value());
  CHECK(pf.detail->name().local_name() == "DbError");
}

// -- End-to-end: SOAP 1.2 with mustUnderstand fault ---------------------------

TEST_CASE("soap integration: 1.2 mustUnderstand fault generation",
          "[soap_integration]") {
  // Build envelope with unhandled mustUnderstand header
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content =
      xb::any_element(xb::qname("urn:unknown", "CustomHeader"), {}, {});
  hb.must_understand = true;
  env.headers.push_back(std::move(hb));

  env.body.emplace_back(xb::qname("urn:api", "Request"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  // Serialize and parse
  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  // Run through empty pipeline — should throw
  soap::header_pipeline pipeline;
  try {
    pipeline.process(parsed);
    FAIL("expected soap_fault_exception");
  } catch (const soap::soap_fault_exception& e) {
    // Verify the generated fault
    REQUIRE(std::holds_alternative<soap::fault_1_2>(e.fault_value));
    auto& f = std::get<soap::fault_1_2>(e.fault_value);
    CHECK(f.code.value == "soap:MustUnderstand");
    CHECK_FALSE(f.reason.empty());

    // Serialize the fault to verify it produces valid XML
    std::ostringstream os;
    xb::ostream_writer writer(os);
    soap::write_fault(writer, e.fault_value, soap::soap_version::v1_2);
    std::string fault_xml = os.str();
    CHECK_FALSE(fault_xml.empty());
  }
}

// -- End-to-end: SOAP 1.1 simple request/response pattern ---------------------

TEST_CASE("soap integration: 1.1 simple body-only envelope",
          "[soap_integration]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  env.body.emplace_back(
      xb::qname("urn:stock", "GetQuote"), std::vector<xb::any_attribute>{},
      std::vector<xb::any_element::child>{xb::any_element(
          xb::qname("urn:stock", "Symbol"), {}, {std::string("AAPL")})});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_1);
  CHECK(parsed.headers.empty());
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed.body[0].name().local_name() == "GetQuote");
  CHECK(parsed == env);
}
