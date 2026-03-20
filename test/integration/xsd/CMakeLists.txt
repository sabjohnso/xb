add_executable(xb_xsd_bootstrap_test xsd_bootstrap_test.cpp)
target_link_libraries(xb_xsd_bootstrap_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xsd_bootstrap_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xsd_bootstrap_test PRIVATE
  XB_XSD_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR}
  XB_XSD_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/generated)
add_test(NAME xb_xsd_bootstrap_test COMMAND xb_xsd_bootstrap_test)
