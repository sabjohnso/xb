# SOAP Envelope example
#
# Demonstrates building a SOAP 1.2 envelope with headers and body,
# serializing to XML, parsing it back, and checking for faults.
# No code generation — uses the xb runtime directly.

add_executable(soap_envelope main.cpp)
target_link_libraries(soap_envelope PRIVATE xb::library)
target_compile_features(soap_envelope PRIVATE cxx_std_${xb_CXX_STANDARD})
