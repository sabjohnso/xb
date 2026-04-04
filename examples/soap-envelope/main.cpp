#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_fault.hpp>
#include <xb/soap_header.hpp>
#include <xb/soap_model.hpp>

#include <iostream>
#include <sstream>

// Build a SOAP 1.2 request envelope, serialize it, parse it back,
// then parse a fault response and demonstrate fault detection.

static void
print_xml(const std::string& label, const std::string& xml) {
  std::cout << "=== " << label << " ===\n" << xml << "\n\n";
}

int
main() {
  namespace soap = xb::soap;

  // -- Build a SOAP 1.2 request envelope --

  soap::envelope request;
  request.version = soap::soap_version::v1_2;

  // Add a custom header
  soap::header_block auth_hdr;
  auth_hdr.content =
      xb::any_element(xb::qname{"urn:example:auth", "Token"}, {},
                      {xb::any_element::child{std::string("my-jwt-token")}});
  auth_hdr.must_understand = true;
  request.headers.push_back(auth_hdr);

  // Add a body element
  xb::any_element city_elem(
      xb::qname{"urn:example:weather", "city"}, {},
      {xb::any_element::child{std::string("Springfield")}});
  xb::any_element body_elem(xb::qname{"urn:example:weather", "GetTemperature"},
                            {}, {xb::any_element::child{city_elem}});
  request.body.push_back(body_elem);

  // -- Serialize to XML --

  std::ostringstream req_xml;
  {
    xb::ostream_writer writer(req_xml);
    soap::write_envelope(writer, request);
  }
  print_xml("SOAP 1.2 Request", req_xml.str());

  // -- Parse it back --

  xb::expat_reader reader(req_xml.str());
  while (reader.read()) {
    if (reader.node_type() == xb::xml_node_type::start_element) {
      auto parsed = soap::read_envelope(reader);

      std::cout << "Parsed envelope:\n"
                << "  Version: "
                << (parsed.version == soap::soap_version::v1_2 ? "1.2" : "1.1")
                << "\n"
                << "  Headers: " << parsed.headers.size() << "\n"
                << "  Body elements: " << parsed.body.size() << "\n\n";
      break;
    }
  }

  // -- Header pipeline: process mustUnderstand headers --

  std::cout << "=== Header pipeline ===\n";
  {
    auto parsed = [&] {
      xb::expat_reader r(req_xml.str());
      while (r.read()) {
        if (r.node_type() == xb::xml_node_type::start_element)
          return soap::read_envelope(r);
      }
      throw std::runtime_error("no root element");
    }();

    soap::header_pipeline pipeline;

    // Register a handler that recognizes our auth token
    pipeline.add_handler(xb::qname{"urn:example:auth", "Token"},
                         [](const soap::header_block& /*hdr*/) -> bool {
                           std::cout << "  Processed auth token header\n";
                           return true;
                         });

    // Process — would throw if any mustUnderstand header is unhandled
    pipeline.process(parsed);
    std::cout << "  All headers processed successfully\n\n";
  }

  // -- Demonstrate fault detection --

  std::cout << "=== Fault detection ===\n";

  // Parse a SOAP 1.2 fault response from inline XML
  const char* fault_xml =
      "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\">"
      "<soap:Body>"
      "<soap:Fault>"
      "<soap:Code><soap:Value>soap:Sender</soap:Value></soap:Code>"
      "<soap:Reason><soap:Text xml:lang=\"en\">City not found"
      "</soap:Text></soap:Reason>"
      "</soap:Fault>"
      "</soap:Body>"
      "</soap:Envelope>";

  xb::expat_reader fault_reader(fault_xml);
  while (fault_reader.read()) {
    if (fault_reader.node_type() == xb::xml_node_type::start_element) {
      auto parsed = soap::read_envelope(fault_reader);

      if (!parsed.body.empty() &&
          soap::is_fault(parsed.body.front(), parsed.version)) {
        std::cout << "  Fault detected in response body\n";
      } else {
        std::cout << "  No fault in response\n";
      }
      break;
    }
  }

  return 0;
}
