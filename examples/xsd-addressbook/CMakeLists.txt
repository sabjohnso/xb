# XSD Addressbook example
#
# Demonstrates generating C++ types from an XSD schema, constructing
# values, serializing to XML, and deserializing back.

xb_add_library(
  TARGET addressbook_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/addressbook.xsd
  MODE HEADER_ONLY
  NAMESPACE_MAP "http://example.com/addressbook=addressbook")

add_executable(xsd_addressbook main.cpp)
target_link_libraries(xsd_addressbook PRIVATE addressbook_types)
target_compile_features(xsd_addressbook PRIVATE cxx_std_${xb_CXX_STANDARD})
