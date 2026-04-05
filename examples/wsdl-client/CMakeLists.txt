# WSDL Client/Server example
#
# Demonstrates a SOAP service with both client and server sides using
# xb-generated types, http_listener for the server, and http_transport
# for the client, communicating over HTTP on localhost.

xb_add_library(
  TARGET weather_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/weather.xsd
  MODE HEADER_ONLY
  NAMESPACE_MAP "http://example.com/weather=weather")

add_executable(wsdl_client main.cpp)
target_link_libraries(wsdl_client PRIVATE weather_types)
target_compile_features(wsdl_client PRIVATE cxx_std_${xb_CXX_STANDARD})
