# BES Hello World example
#
# Demonstrates the simplest possible BES-only workflow:
# one BES file → one xb_add_library call → link and use.

xb_add_library(
  TARGET hello_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/heartbeat.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(bes_hello main.cpp)
target_link_libraries(bes_hello PRIVATE hello_types)
target_compile_features(bes_hello PRIVATE cxx_std_${xb_CXX_STANDARD})
