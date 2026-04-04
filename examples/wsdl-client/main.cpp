#include <weather.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_fault.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_support.hpp>
#include <xb/wsdl_transport.hpp>

#include <iostream>
#include <sstream>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------
// A simple SOAP server that dispatches by SOAPAction header and
// uses the generated types to parse requests and build responses.

class weather_server {
  static constexpr auto ws_ns = "http://example.com/weather";

public:
  xb::soap::envelope
  handle(const std::string& soap_action, const xb::soap::envelope& request) {
    if (soap_action == "http://example.com/weather/GetTemperature") {
      return handle_get_temperature(request);
    }
    return make_fault(request.version, "soap:Sender",
                      "Unknown operation: " + soap_action);
  }

private:
  xb::soap::envelope
  handle_get_temperature(const xb::soap::envelope& request) {
    if (request.body.empty()) {
      return make_fault(request.version, "soap:Sender", "Empty request body");
    }

    // Parse the typed request from the SOAP body
    auto req = xb::service::parse_body_element<weather::get_temperature_type>(
        request.body.front(), [](xb::xml_reader& r) {
          return weather::read_get_temperature_type(r);
        });

    std::cout << "[server] GetTemperature for city: " << req.city << "\n";

    // Look up the temperature (hardcoded for the example)
    std::unordered_map<std::string, double> temperatures = {
        {"Springfield", 72.5},
        {"Shelbyville", 68.0},
        {"Capital City", 75.3},
    };

    auto it = temperatures.find(req.city);
    if (it == temperatures.end()) {
      return make_fault(request.version, "soap:Sender",
                        "Unknown city: " + req.city);
    }

    // Build the typed response
    weather::get_temperature_response_type resp;
    resp.temperature = it->second;
    resp.unit = "F";
    resp.city = req.city;

    auto body = xb::service::make_body_element(
        xb::qname{ws_ns, "GetTemperatureResponse"}, resp,
        [](xb::xml_writer& w, const weather::get_temperature_response_type& v) {
          w.start_element(xb::qname{ws_ns, "GetTemperatureResponse"});
          w.namespace_declaration("", ws_ns);
          weather::write_get_temperature_response_type(v, w);
          w.end_element();
        });

    xb::soap::envelope response;
    response.version = request.version;
    response.body.push_back(body);
    return response;
  }

  static xb::soap::envelope
  make_fault(xb::soap::soap_version version, const std::string& code,
             const std::string& reason) {
    // Build the SOAP 1.2 Fault element as an any_element tree directly,
    // avoiding the write/re-parse round-trip (which has issues with the
    // reserved xml: prefix in xml:lang attributes).
    const std::string soap_ns = "http://www.w3.org/2003/05/soap-envelope";

    using child = xb::any_element::child;
    using elem = xb::any_element;

    auto value_elem = elem(xb::qname{soap_ns, "Value"}, {}, {child{code}});
    auto code_elem = elem(xb::qname{soap_ns, "Code"}, {}, {child{value_elem}});

    // Note: xml:lang is omitted here because any_element::write()
    // re-declares the reserved xml: prefix, which Expat rejects on
    // re-parse.  This is a known limitation tracked in ISSUES.org.
    auto text_elem = elem(xb::qname{soap_ns, "Text"}, {}, {child{reason}});
    auto reason_elem =
        elem(xb::qname{soap_ns, "Reason"}, {}, {child{text_elem}});

    auto fault_elem = elem(xb::qname{soap_ns, "Fault"}, {},
                           {child{code_elem}, child{reason_elem}});

    xb::soap::envelope env;
    env.version = version;
    env.body.push_back(fault_elem);
    return env;
  }
};

// ---------------------------------------------------------------------------
// In-process transport
// ---------------------------------------------------------------------------
// Connects the client to the server without network I/O.  In production
// you would use the libcurl-based HTTP transport instead.

class in_process_transport : public xb::service::transport {
  weather_server& server_;

public:
  explicit in_process_transport(weather_server& server) : server_(server) {}

  xb::soap::envelope
  call(const std::string& /*endpoint*/, const std::string& soap_action,
       const xb::soap::envelope& request) override {
    // Log the wire XML
    std::ostringstream wire;
    {
      xb::ostream_writer w(wire);
      xb::soap::write_envelope(w, request);
    }
    std::cout << "[transport] sending " << wire.str().size() << " bytes\n";

    // Dispatch to the server
    return server_.handle(soap_action, request);
  }
};

// ---------------------------------------------------------------------------
// Client helper
// ---------------------------------------------------------------------------

static weather::get_temperature_response_type
get_temperature(xb::service::transport& transport, const std::string& city) {
  namespace soap = xb::soap;
  constexpr auto ws_ns = "http://example.com/weather";

  // Build the typed request
  weather::get_temperature_type req;
  req.city = city;

  auto req_body = xb::service::make_body_element(
      xb::qname{ws_ns, "GetTemperature"}, req,
      [](xb::xml_writer& w, const weather::get_temperature_type& v) {
        w.start_element(xb::qname{ws_ns, "GetTemperature"});
        w.namespace_declaration("", ws_ns);
        weather::write_get_temperature_type(v, w);
        w.end_element();
      });

  soap::envelope request;
  request.version = soap::soap_version::v1_2;
  request.body.push_back(req_body);

  // Call the service
  auto response =
      transport.call("http://example.com/weather/service",
                     "http://example.com/weather/GetTemperature", request);

  // Check for faults (throws soap_call_fault on error)
  xb::service::check_fault(response);

  // Parse the typed response
  return xb::service::parse_body_element<
      weather::get_temperature_response_type>(
      response.body.front(), [](xb::xml_reader& r) {
        return weather::read_get_temperature_response_type(r);
      });
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int
main() {
  weather_server server;
  in_process_transport transport(server);

  // Successful request
  std::cout << "--- Request: Springfield ---\n";
  auto result = get_temperature(transport, "Springfield");
  std::cout << "[client] " << result.city << ": " << result.temperature << " "
            << result.unit << "\n\n";

  // Another successful request
  std::cout << "--- Request: Shelbyville ---\n";
  result = get_temperature(transport, "Shelbyville");
  std::cout << "[client] " << result.city << ": " << result.temperature << " "
            << result.unit << "\n\n";

  // Request that triggers a fault
  std::cout << "--- Request: Atlantis ---\n";
  try {
    get_temperature(transport, "Atlantis");
    std::cout << "[client] unexpected success\n";
    return 1;
  } catch (const xb::service::soap_call_fault& e) {
    std::cout << "[client] caught fault: " << e.what() << "\n";
  }

  return 0;
}
