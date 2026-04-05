# WSDL Client/Server example
#
# Two separate executables: a SOAP server and a SOAP client, both using
# types, client stubs, and server skeletons generated from a WSDL file.
#
# Usage:
#   ./weather_server 8080
#   ./weather_client http://127.0.0.1:8080/ Springfield Shelbyville

xb_add_library(
  TARGET weather_service
  WSDL ${CMAKE_CURRENT_SOURCE_DIR}/weather.wsdl
  MODE HEADER_ONLY
  NAMESPACE_MAP "http://example.com/weather=weather")

add_executable(weather_server server.cpp)
target_link_libraries(weather_server PRIVATE weather_service)
target_compile_features(weather_server PRIVATE cxx_std_${xb_CXX_STANDARD})

add_executable(weather_client client.cpp)
target_link_libraries(weather_client PRIVATE weather_service)
target_compile_features(weather_client PRIVATE cxx_std_${xb_CXX_STANDARD})
