# BES Market Data example
#
# Demonstrates multiple message types, discriminant dispatch,
# designated initializers, and validation levels.

xb_add_library(
  TARGET market_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/market.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(bes_market_data main.cpp)
target_link_libraries(bes_market_data PRIVATE market_types)
target_compile_features(bes_market_data PRIVATE cxx_std_${xb_CXX_STANDARD})
