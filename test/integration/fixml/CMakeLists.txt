add_executable(xb_fixml_integration_test fixml_test.cpp)
target_link_libraries(xb_fixml_integration_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_fixml_integration_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_fixml_integration_test PRIVATE
  XB_FIXML_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
add_test(NAME xb_fixml_integration_test COMMAND xb_fixml_integration_test)
