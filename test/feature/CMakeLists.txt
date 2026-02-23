add_executable(xb_any_element_round_trip_test any_element_round_trip_test.cpp)
target_link_libraries(xb_any_element_round_trip_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_any_element_round_trip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_any_element_round_trip_test COMMAND xb_any_element_round_trip_test)

add_executable(xb_xml_round_trip_test xml_round_trip_test.cpp)
target_link_libraries(xb_xml_round_trip_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_xml_round_trip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_xml_round_trip_test COMMAND xb_xml_round_trip_test)

add_executable(xb_codegen_compile_test codegen_compile_test.cpp)
target_link_libraries(xb_codegen_compile_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_codegen_compile_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_codegen_compile_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include)
add_test(NAME xb_codegen_compile_test COMMAND xb_codegen_compile_test)

add_executable(xb_serialization_test serialization_test.cpp)
target_link_libraries(xb_serialization_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_serialization_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_serialization_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb>)
add_test(NAME xb_serialization_test COMMAND xb_serialization_test)
