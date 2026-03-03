#include <xb/wsdl_support.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_fault.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace svc = xb::service;
namespace soap = xb::soap;

// -- Helper types for testing -------------------------------------------------

namespace {

  struct price_request {
    std::string ticker;

    bool
    operator==(const price_request&) const = default;
  };

  void
  write_price_request(xb::xml_writer& w, const price_request& req) {
    w.start_element(xb::qname("urn:ex", "GetPrice"));
    w.namespace_declaration("ns", "urn:ex");
    w.start_element(xb::qname("urn:ex", "ticker"));
    w.characters(req.ticker);
    w.end_element();
    w.end_element();
  }

  price_request
  read_price_request(xb::xml_reader& r) {
    price_request req;
    // We're at the start of the GetPrice element
    int depth = static_cast<int>(r.depth());
    while (r.read()) {
      if (r.node_type() == xb::xml_node_type::start_element &&
          r.name().local_name() == "ticker") {
        // read text content
        while (r.read()) {
          if (r.node_type() == xb::xml_node_type::characters) {
            req.ticker = std::string(r.text());
          } else if (r.node_type() == xb::xml_node_type::end_element) {
            break;
          }
        }
      } else if (r.node_type() == xb::xml_node_type::end_element &&
                 static_cast<int>(r.depth()) == depth) {
        break;
      }
    }
    return req;
  }

} // namespace

// -- make_body_element + parse_body_element round-trip
// -------------------------

TEST_CASE("wsdl support: make/parse body element round-trip",
          "[wsdl_support]") {
  price_request original;
  original.ticker = "AAPL";

  auto elem =
      svc::make_body_element(xb::qname("urn:ex", "GetPrice"), original,
                             [](xb::xml_writer& w, const price_request& r) {
                               write_price_request(w, r);
                             });

  CHECK(elem.name().local_name() == "GetPrice");
  CHECK(elem.name().namespace_uri() == "urn:ex");

  auto parsed = svc::parse_body_element<price_request>(
      elem,
      [](xb::xml_reader& r) -> price_request { return read_price_request(r); });

  CHECK(parsed.ticker == "AAPL");
  CHECK(parsed == original);
}

// -- check_fault: clean envelope does not throw -------------------------------

TEST_CASE("wsdl support: check_fault with clean envelope does not throw",
          "[wsdl_support]") {
  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  env.body.emplace_back(xb::qname("urn:ex", "Resp"),
                        std::vector<xb::any_attribute>{},
                        std::vector<xb::any_element::child>{std::string("OK")});

  CHECK_NOTHROW(svc::check_fault(env));
}

// -- check_fault: SOAP 1.1 fault throws soap_call_fault -----------------------

TEST_CASE("wsdl support: check_fault with SOAP 1.1 fault throws",
          "[wsdl_support]") {
  // Build a SOAP 1.1 fault envelope
  soap::envelope env;
  env.version = soap::soap_version::v1_1;

  // Serialize a fault, then re-parse as any_element to put in body
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "Internal error";

  std::ostringstream os;
  xb::ostream_writer writer(os);
  soap::write_fault(writer, soap::fault{f}, soap::soap_version::v1_1);
  std::string fault_xml = os.str();

  xb::expat_reader reader(fault_xml);
  while (reader.read()) {
    if (reader.node_type() == xb::xml_node_type::start_element) {
      env.body.emplace_back(reader);
      break;
    }
  }

  CHECK_THROWS_AS(svc::check_fault(env), svc::soap_call_fault);
}

// -- check_fault: SOAP 1.2 fault throws soap_call_fault -----------------------

TEST_CASE("wsdl support: check_fault with SOAP 1.2 fault throws",
          "[wsdl_support]") {
  // Build a SOAP 1.2 fault body element directly as any_element,
  // avoiding the xml:lang re-serialization issue with reserved prefixes.
  static constexpr auto ns12 = "http://www.w3.org/2003/05/soap-envelope";

  soap::envelope env;
  env.version = soap::soap_version::v1_2;

  // Construct the Fault element tree matching SOAP 1.2 structure
  xb::any_element code_value(
      xb::qname(ns12, "Value"), {},
      std::vector<xb::any_element::child>{std::string("env:Receiver")});
  xb::any_element code(
      xb::qname(ns12, "Code"), {},
      std::vector<xb::any_element::child>{std::move(code_value)});
  xb::any_element reason_text(
      xb::qname(ns12, "Text"), {},
      std::vector<xb::any_element::child>{std::string("Server error")});
  xb::any_element reason(
      xb::qname(ns12, "Reason"), {},
      std::vector<xb::any_element::child>{std::move(reason_text)});
  xb::any_element fault_elem(
      xb::qname(ns12, "Fault"), {},
      std::vector<xb::any_element::child>{std::move(code), std::move(reason)});

  env.body.push_back(std::move(fault_elem));

  CHECK_THROWS_AS(svc::check_fault(env), svc::soap_call_fault);
}

// -- make_rpc_request builds wrapper element
// -----------------------------------

TEST_CASE("wsdl support: make_rpc_request builds wrapper element",
          "[wsdl_support]") {
  auto elem = svc::make_rpc_request(xb::qname("urn:rpc", "GetData"),
                                    {{"ticker", "AAPL"}, {"count", "10"}});

  CHECK(elem.name() == xb::qname("urn:rpc", "GetData"));
  CHECK(elem.children().size() == 2);

  // Each child should be an element with text content
  const auto& child0 = std::get<xb::any_element>(elem.children()[0]);
  CHECK(child0.name().local_name() == "ticker");
  REQUIRE(child0.children().size() == 1);
  CHECK(std::get<std::string>(child0.children()[0]) == "AAPL");

  const auto& child1 = std::get<xb::any_element>(elem.children()[1]);
  CHECK(child1.name().local_name() == "count");
  REQUIRE(child1.children().size() == 1);
  CHECK(std::get<std::string>(child1.children()[0]) == "10");
}
