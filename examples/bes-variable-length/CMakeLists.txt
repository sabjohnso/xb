# BES Variable-Length Message Stream example
#
# Demonstrates length-prefixed messages of different types and sizes
# in a single message stream, with discriminant-based dispatch.

xb_add_library(
  TARGET varlen_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/messages.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(bes_variable_length main.cpp)
target_link_libraries(bes_variable_length PRIVATE varlen_types)
target_compile_features(bes_variable_length PRIVATE cxx_std_${xb_CXX_STANDARD})
