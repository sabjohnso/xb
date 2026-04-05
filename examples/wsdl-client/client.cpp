#include <weather.hpp>
#include <weather_port_client.hpp>

#include <xb/http_transport.hpp>
#include <xb/wsdl_support.hpp>
#include <xb/wsdl_transport.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Weather service client.
//
// Usage: weather_client [ENDPOINT] [CITY...]
//
// Uses the generated WeatherPort client stub from weather.wsdl to
// make typed GetTemperature SOAP calls.

int
main(int argc, char* argv[]) {
  std::string endpoint = "http://127.0.0.1:8080/";
  if (argc > 1) endpoint = argv[1];

  std::vector<std::string> cities;
  if (argc > 2) {
    for (int i = 2; i < argc; ++i)
      cities.emplace_back(argv[i]);
  } else {
    cities = {"Springfield", "Shelbyville", "Atlantis"};
  }

  xb::service::http_transport transport;
  xb::client::weather_port_client client(transport, endpoint);

  for (const auto& city : cities) {
    std::cout << "GetTemperature(" << city << "): ";
    try {
      weather::get_temperature req;
      req.city = city;
      auto resp = client.get_temperature(req);
      std::cout << resp.temperature << " " << resp.unit << "\n";
    } catch (const xb::service::soap_call_fault& e) {
      std::cout << "FAULT: " << e.what() << "\n";
    } catch (const xb::service::transport_error& e) {
      std::cout << "ERROR: " << e.what() << "\n";
    }
  }

  return 0;
}
