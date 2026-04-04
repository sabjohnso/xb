#include <weather.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_support.hpp>
#include <xb/wsdl_transport.hpp>

#include <iostream>
#include <sstream>

// A mock transport that returns a canned response instead of
// making a real HTTP call.  In production you would use the
// libcurl-based HTTP transport.

class mock_transport : public xb::service::transport {
public:
  xb::soap::envelope
  call(const std::string& /*endpoint*/, const std::string& /*soap_action*/,
       const xb::soap::envelope& request) override {

    // Log the request
    std::ostringstream req_xml;
    {
      xb::ostream_writer w(req_xml);
      xb::soap::write_envelope(w, request);
    }
    std::cout << "=== Request sent to mock transport ===\n"
              << req_xml.str() << "\n\n";

    // Build a canned response
    weather::get_temperature_response_type resp;
    resp.temperature = 72.5;
    resp.unit = "F";
    resp.city = "Springfield";

    auto body = xb::service::make_body_element(
        xb::qname{"http://example.com/weather", "GetTemperatureResponse"}, resp,
        [](xb::xml_writer& w, const weather::get_temperature_response_type& v) {
          w.start_element(xb::qname{"http://example.com/weather",
                                    "GetTemperatureResponse"});
          w.namespace_declaration("", "http://example.com/weather");
          weather::write_get_temperature_response_type(v, w);
          w.end_element();
        });

    xb::soap::envelope response;
    response.version = request.version;
    response.body.push_back(body);
    return response;
  }
};

int
main() {
  namespace soap = xb::soap;
  const std::string ws_ns = "http://example.com/weather";

  // -- Build request --

  weather::get_temperature_type req;
  req.city = "Springfield";

  auto req_body = xb::service::make_body_element(
      xb::qname{ws_ns, "GetTemperature"}, req,
      [&](xb::xml_writer& w, const weather::get_temperature_type& v) {
        w.start_element(xb::qname{ws_ns, "GetTemperature"});
        w.namespace_declaration("", ws_ns);
        weather::write_get_temperature_type(v, w);
        w.end_element();
      });

  soap::envelope request;
  request.version = soap::soap_version::v1_2;
  request.body.push_back(req_body);

  // -- Send via mock transport --

  mock_transport transport;
  auto response =
      transport.call("http://example.com/weather/service",
                     "http://example.com/weather/GetTemperature", request);

  // -- Check for faults --

  try {
    xb::service::check_fault(response);
  } catch (const xb::service::soap_call_fault& e) {
    std::cerr << "SOAP fault: " << e.what() << "\n";
    return 1;
  }

  // -- Parse response body --

  auto result =
      xb::service::parse_body_element<weather::get_temperature_response_type>(
          response.body.front(), [](xb::xml_reader& r) {
            return weather::read_get_temperature_response_type(r);
          });

  std::cout << "=== Response ===\n"
            << "  City:        " << result.city << "\n"
            << "  Temperature: " << result.temperature << "\n"
            << "  Unit:        " << result.unit << "\n";

  return 0;
}
