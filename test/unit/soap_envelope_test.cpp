#include <xb/soap_envelope.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace soap = xb::soap;

namespace {

  // Helper: serialize envelope to XML string
  std::string
  serialize(const soap::envelope& env) {
    std::ostringstream os;
    xb::ostream_writer writer(os);
    soap::write_envelope(writer, env);
    return os.str();
  }

  // Helper: parse XML string into envelope
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

} // namespace

// -- SOAP 1.1 -----------------------------------------------------------------

TEST_CASE("soap envelope: round-trip SOAP 1.1 with body", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  env.body.emplace_back(
      xb::qname("urn:example", "GetPrice"), std::vector<xb::any_attribute>{},
      std::vector<xb::any_element::child>{std::string("item-1")});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_1);
  CHECK(parsed.headers.empty());
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed.body[0].name().local_name() == "GetPrice");
  CHECK(parsed == env);
}

TEST_CASE("soap envelope: SOAP 1.1 with headers and body", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Token"), {},
                               {std::string("abc123")});
  hb.must_understand = true;
  hb.role = "http://schemas.xmlsoap.org/soap/actor/next";
  env.headers.push_back(std::move(hb));

  env.body.emplace_back(xb::qname("urn:example", "DoWork"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_1);
  REQUIRE(parsed.headers.size() == 1);
  CHECK(parsed.headers[0].must_understand == true);
  CHECK(parsed.headers[0].role == "http://schemas.xmlsoap.org/soap/actor/next");
  CHECK(parsed.headers[0].content.name().local_name() == "Token");
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed == env);
}

// -- SOAP 1.2 -----------------------------------------------------------------

TEST_CASE("soap envelope: round-trip SOAP 1.2 with body", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  env.body.emplace_back(xb::qname("urn:example", "GetStatus"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_2);
  CHECK(parsed.headers.empty());
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed.body[0].name().local_name() == "GetStatus");
  CHECK(parsed == env);
}

TEST_CASE("soap envelope: SOAP 1.2 with headers and body", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb;
  hb.content = xb::any_element(xb::qname("urn:auth", "Session"), {},
                               {std::string("sess-42")});
  hb.must_understand = true;
  hb.role = "http://www.w3.org/2003/05/soap-envelope/role/next";
  env.headers.push_back(std::move(hb));

  env.body.emplace_back(xb::qname("urn:example", "Process"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_2);
  REQUIRE(parsed.headers.size() == 1);
  CHECK(parsed.headers[0].must_understand == true);
  CHECK(parsed.headers[0].role ==
        "http://www.w3.org/2003/05/soap-envelope/role/next");
  CHECK(parsed.headers[0].content.name().local_name() == "Session");
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed == env);
}

// -- Empty headers ------------------------------------------------------------

TEST_CASE("soap envelope: empty headers omit Header element",
          "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  env.body.emplace_back(xb::qname("urn:ex", "X"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  // No <Header> element should appear in the output
  CHECK(xml.find("Header") == std::string::npos);
}

// -- Version detection --------------------------------------------------------

TEST_CASE("soap envelope: version detection from 1.1 namespace",
          "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
      <soap:Body>
        <m:GetPrice xmlns:m="urn:example">item</m:GetPrice>
      </soap:Body>
    </soap:Envelope>)";

  soap::envelope env = parse(xml);
  CHECK(env.version == soap::soap_version::v1_1);
  REQUIRE(env.body.size() == 1);
  CHECK(env.body[0].name().local_name() == "GetPrice");
}

TEST_CASE("soap envelope: version detection from 1.2 namespace",
          "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
      <soap:Body>
        <m:GetStatus xmlns:m="urn:example"/>
      </soap:Body>
    </soap:Envelope>)";

  soap::envelope env = parse(xml);
  CHECK(env.version == soap::soap_version::v1_2);
  REQUIRE(env.body.size() == 1);
}

// -- mustUnderstand attribute -------------------------------------------------

TEST_CASE("soap envelope: mustUnderstand=1 parsed as true", "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
      <soap:Header>
        <auth:Token xmlns:auth="urn:auth"
                    soap:mustUnderstand="1">secret</auth:Token>
      </soap:Header>
      <soap:Body/>
    </soap:Envelope>)";

  soap::envelope env = parse(xml);
  REQUIRE(env.headers.size() == 1);
  CHECK(env.headers[0].must_understand == true);
}

TEST_CASE("soap envelope: mustUnderstand=true parsed as true",
          "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
      <soap:Header>
        <auth:Token xmlns:auth="urn:auth"
                    soap:mustUnderstand="true">secret</auth:Token>
      </soap:Header>
      <soap:Body/>
    </soap:Envelope>)";

  soap::envelope env = parse(xml);
  REQUIRE(env.headers.size() == 1);
  CHECK(env.headers[0].must_understand == true);
}

TEST_CASE("soap envelope: mustUnderstand absent defaults to false",
          "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
      <soap:Header>
        <auth:Token xmlns:auth="urn:auth">secret</auth:Token>
      </soap:Header>
      <soap:Body/>
    </soap:Envelope>)";

  soap::envelope env = parse(xml);
  REQUIRE(env.headers.size() == 1);
  CHECK(env.headers[0].must_understand == false);
}

// -- Empty body ---------------------------------------------------------------

TEST_CASE("soap envelope: empty body round-trip", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  // No body children

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_2);
  CHECK(parsed.headers.empty());
  CHECK(parsed.body.empty());
  CHECK(parsed == env);
}

// -- Unknown SOAP namespace ---------------------------------------------------

TEST_CASE("soap envelope: unknown namespace throws", "[soap_envelope]") {
  std::string xml = R"(
    <soap:Envelope xmlns:soap="http://example.org/unknown-soap">
      <soap:Body/>
    </soap:Envelope>)";

  CHECK_THROWS_AS(parse(xml), std::runtime_error);
}

// -- Multiple header blocks from different namespaces -------------------------

TEST_CASE("soap envelope: multiple header blocks from different namespaces",
          "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  soap::header_block hb1;
  hb1.content = xb::any_element(xb::qname("urn:auth", "Token"), {},
                                {std::string("jwt-value")});
  hb1.must_understand = true;
  env.headers.push_back(std::move(hb1));

  soap::header_block hb2;
  hb2.content = xb::any_element(xb::qname("urn:trace", "TraceId"), {},
                                {std::string("trace-123")});
  hb2.must_understand = false;
  env.headers.push_back(std::move(hb2));

  soap::header_block hb3;
  hb3.content = xb::any_element(xb::qname("urn:logging", "LogLevel"), {},
                                {std::string("DEBUG")});
  hb3.must_understand = false;
  hb3.role = "http://www.w3.org/2003/05/soap-envelope/role/next";
  env.headers.push_back(std::move(hb3));

  env.body.emplace_back(xb::qname("urn:api", "Request"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);

  CHECK(parsed.version == soap::soap_version::v1_2);
  REQUIRE(parsed.headers.size() == 3);
  CHECK(parsed.headers[0].content.name().namespace_uri() == "urn:auth");
  CHECK(parsed.headers[0].must_understand == true);
  CHECK(parsed.headers[1].content.name().namespace_uri() == "urn:trace");
  CHECK(parsed.headers[1].must_understand == false);
  CHECK(parsed.headers[2].content.name().namespace_uri() == "urn:logging");
  CHECK(parsed.headers[2].role ==
        "http://www.w3.org/2003/05/soap-envelope/role/next");
  REQUIRE(parsed.body.size() == 1);
  CHECK(parsed == env);
}

// -- Multiple body children ---------------------------------------------------

TEST_CASE("soap envelope: multiple body children", "[soap_envelope]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_2;
  env.body.emplace_back(xb::qname("urn:ex", "A"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});
  env.body.emplace_back(xb::qname("urn:ex", "B"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{});

  std::string xml = serialize(env);
  soap::envelope parsed = parse(xml);
  CHECK(parsed.body.size() == 2);
  CHECK(parsed.body[0].name().local_name() == "A");
  CHECK(parsed.body[1].name().local_name() == "B");
}
