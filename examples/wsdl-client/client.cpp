#include <weather.hpp>

#include <xb/http_transport.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_support.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

// Weather service client.
//
// Usage: weather_client [ENDPOINT] [CITY...]
//
// Makes GetTemperature SOAP requests to the weather server.

static constexpr auto ws_ns = "http://example.com/weather";

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

  xb::service::http_transport client;

  for (const auto& city : cities) {
    std::cout << "GetTemperature(" << city << "): ";
    try {
      auto result = get_temperature(client, endpoint, city);
      std::cout << result.temperature << " " << result.unit << "\n";
    } catch (const xb::service::soap_call_fault& e) {
      std::cout << "FAULT: " << e.what() << "\n";
    } catch (const xb::service::transport_error& e) {
      std::cout << "ERROR: " << e.what() << "\n";
    }
  }

  return 0;
}
