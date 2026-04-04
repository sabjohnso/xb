# WSDL Client example
#
# Demonstrates using xb-generated types with the SOAP support utilities
# and a mock transport to make a WSDL-style service call.

xb_add_library(
  TARGET weather_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/weather.xsd
  MODE HEADER_ONLY
  NAMESPACE_MAP "http://example.com/weather=weather")

add_executable(wsdl_client main.cpp)
target_link_libraries(wsdl_client PRIVATE weather_types)
target_compile_features(wsdl_client PRIVATE cxx_std_${xb_CXX_STANDARD})
