#include <weather.hpp>
#include <weather_port_server.hpp>

#include <xb/http_listener.hpp>

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

// Weather service server.
//
// Usage: weather_server [PORT]
//
// Implements the WeatherPort interface generated from weather.wsdl,
// wires it to a dispatcher, and serves via http_listener.
// Stop with Ctrl-C.

static xb::service::http_listener* g_server = nullptr;

static void
handle_signal(int) {
  if (g_server) g_server->stop();
}

// Implement the generated interface
class weather_impl : public xb::server::weather_port_interface {
public:
  weather::get_temperature_response
  get_temperature(const weather::get_temperature& req) override {
    std::cout << "GetTemperature: " << req.city << "\n";

    static const std::unordered_map<std::string, double> temperatures = {
        {"Springfield", 72.5},
        {"Shelbyville", 68.0},
        {"Capital City", 75.3},
    };

    auto it = temperatures.find(req.city);
    if (it == temperatures.end())
      throw std::runtime_error("Unknown city: " + req.city);

    return weather::get_temperature_response{
        .temperature = it->second,
        .unit = std::string("F"),
        .city = std::string(req.city),
    };
  }
};

int
main(int argc, char* argv[]) {
  std::uint16_t port = 8080;
  if (argc > 1) port = static_cast<std::uint16_t>(std::atoi(argv[1]));

  // Wire: implementation → dispatcher → listener
  weather_impl impl;
  xb::server::weather_port_dispatcher dispatcher(impl);

  xb::service::http_listener server(
      {.bind_address = "127.0.0.1", .port = port});
  g_server = &server;

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  std::cout << "Weather server listening on port " << server.listening_port()
            << "\n"
            << "Press Ctrl-C to stop.\n";

  server.serve(
      [&](const std::string& action, const xb::soap::envelope& request) {
        return dispatcher.dispatch(action, request);
      });

  std::cout << "Server stopped.\n";
  return 0;
}
