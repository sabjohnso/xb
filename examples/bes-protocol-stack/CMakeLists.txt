# BES Protocol Stack example
#
# Demonstrates frame layer definitions, frame-stack composition,
# and the generated frame parser for multi-layer protocol parsing.

xb_add_library(
  TARGET protocol_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/protocol.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(bes_protocol_stack main.cpp)
target_link_libraries(bes_protocol_stack PRIVATE protocol_types)
target_compile_features(bes_protocol_stack PRIVATE cxx_std_${xb_CXX_STANDARD})
