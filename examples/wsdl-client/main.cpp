#include <weather.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/http_listener.hpp>
#include <xb/http_transport.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_support.hpp>
#include <xb/wsdl_transport.hpp>

#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Server implementation
// ---------------------------------------------------------------------------
// Handles SOAP requests by dispatching on SOAPAction and using the
// generated read/write functions for typed request/response marshalling.

static constexpr auto ws_ns = "http://example.com/weather";

static xb::soap::envelope
handle_get_temperature(const xb::soap::envelope& request) {
  auto req = xb::service::parse_body_element<weather::get_temperature_type>(
      request.body.front(),
      [](xb::xml_reader& r) { return weather::read_get_temperature_type(r); });

  std::cout << "[server] GetTemperature for: " << req.city << "\n";

  // Look up the temperature (hardcoded for the example)
  static const std::unordered_map<std::string, double> temperatures = {
      {"Springfield", 72.5},
      {"Shelbyville", 68.0},
      {"Capital City", 75.3},
  };

  auto it = temperatures.find(req.city);
  if (it == temperatures.end())
    throw std::runtime_error("Unknown city: " + req.city);

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
dispatch(const std::string& soap_action, const xb::soap::envelope& request) {
  if (soap_action == "http://example.com/weather/GetTemperature")
    return handle_get_temperature(request);
  throw std::runtime_error("Unknown operation: " + soap_action);
}

// ---------------------------------------------------------------------------
// Client helper
// ---------------------------------------------------------------------------

static weather::get_temperature_response_type
get_temperature(xb::service::transport& transport, const std::string& endpoint,
                const std::string& city) {
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

  xb::soap::envelope request;
  request.version = xb::soap::soap_version::v1_2;
  request.body.push_back(req_body);

  auto response = transport.call(
      endpoint, "http://example.com/weather/GetTemperature", request);
  xb::service::check_fault(response);

  return xb::service::parse_body_element<
      weather::get_temperature_response_type>(
      response.body.front(), [](xb::xml_reader& r) {
        return weather::read_get_temperature_response_type(r);
      });
}

// ---------------------------------------------------------------------------
// main — starts server, makes client calls, shuts down
// ---------------------------------------------------------------------------

int
main() {
  // Start HTTP listener on an OS-assigned port
  xb::service::http_listener server({.bind_address = "127.0.0.1", .port = 0});
  auto port = server.listening_port();
  std::string endpoint = "http://127.0.0.1:" + std::to_string(port) + "/";

  std::cout << "Server listening on port " << port << "\n\n";

  std::thread server_thread([&] { server.serve(dispatch); });

  // Create an HTTP client transport
  xb::service::http_transport client;

  // Successful request
  std::cout << "--- Request: Springfield ---\n";
  auto result = get_temperature(client, endpoint, "Springfield");
  std::cout << "[client] " << result.city << ": " << result.temperature << " "
            << result.unit << "\n\n";

  // Another successful request
  std::cout << "--- Request: Shelbyville ---\n";
  result = get_temperature(client, endpoint, "Shelbyville");
  std::cout << "[client] " << result.city << ": " << result.temperature << " "
            << result.unit << "\n\n";

  // Request that triggers a fault
  std::cout << "--- Request: Atlantis ---\n";
  try {
    get_temperature(client, endpoint, "Atlantis");
    std::cout << "[client] unexpected success\n";
  } catch (const xb::service::soap_call_fault& e) {
    std::cout << "[client] caught fault: " << e.what() << "\n";
  }

  // Shut down
  server.stop();
  server_thread.join();
  std::cout << "\nServer stopped.\n";

  return 0;
}
