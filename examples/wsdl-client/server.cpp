#include <weather.hpp>

#include <xb/http_listener.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_support.hpp>

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

// Weather service server.
//
// Usage: weather_server [PORT]
//
// Listens for SOAP requests and responds to GetTemperature operations.
// Stop with Ctrl-C.

static constexpr auto ws_ns = "http://example.com/weather";

static xb::service::http_listener* g_server = nullptr;

static void
handle_signal(int) {
  if (g_server) g_server->stop();
}

static xb::soap::envelope
handle_get_temperature(const xb::soap::envelope& request) {
  auto req = xb::service::parse_body_element<weather::get_temperature_type>(
      request.body.front(),
      [](xb::xml_reader& r) { return weather::read_get_temperature_type(r); });

  std::cout << "GetTemperature: " << req.city << "\n";

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

int
main(int argc, char* argv[]) {
  std::uint16_t port = 8080;
  if (argc > 1) port = static_cast<std::uint16_t>(std::atoi(argv[1]));

  xb::service::http_listener server(
      {.bind_address = "127.0.0.1", .port = port});
  g_server = &server;

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  std::cout << "Weather server listening on port " << server.listening_port()
            << "\n"
            << "Press Ctrl-C to stop.\n";

  server.serve(dispatch);

  std::cout << "Server stopped.\n";
  return 0;
}
