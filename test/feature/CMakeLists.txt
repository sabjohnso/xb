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
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
add_test(NAME xb_codegen_compile_test COMMAND xb_codegen_compile_test)

add_executable(xb_serialization_test serialization_test.cpp)
target_link_libraries(xb_serialization_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_serialization_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_serialization_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
add_test(NAME xb_serialization_test COMMAND xb_serialization_test)

add_executable(xb_cli_test cli_test.cpp)
target_link_libraries(xb_cli_test PRIVATE Catch2::Catch2WithMain)
target_compile_features(xb_cli_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_cli_test PRIVATE
  XB_CLI=$<TARGET_FILE:xb_cli>
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema)
add_test(NAME xb_cli_test COMMAND xb_cli_test)

xb_generate_cpp(
  TARGET xb_cmake_gen_test_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_gen
  MODE HEADER_ONLY)

add_executable(xb_cmake_gen_test cmake_integration_test.cpp)
target_link_libraries(xb_cmake_gen_test
  PRIVATE Catch2::Catch2WithMain xb_cmake_gen_test_types xb)
target_compile_features(xb_cmake_gen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_cmake_gen_test COMMAND xb_cmake_gen_test)

xb_generate_cpp(
  TARGET xb_cmake_gen_split_test_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_gen_split
  MODE SPLIT)

add_executable(xb_cmake_gen_split_test cmake_split_integration_test.cpp)
target_link_libraries(xb_cmake_gen_split_test
  PRIVATE Catch2::Catch2WithMain xb_cmake_gen_split_test_types xb)
target_compile_features(xb_cmake_gen_split_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_cmake_gen_split_test COMMAND xb_cmake_gen_split_test)

# --- xb_fetch_schemas integration test ---
# The fetch function runs at configure time and needs the xb binary.
# In an in-tree build the binary only exists after the first build,
# so we probe known locations from prior builds.
set(_xb_fetch_test_exe)
foreach(_config Release Debug RelWithDebInfo MinSizeRel)
  if(EXISTS "${CMAKE_BINARY_DIR}/src/bin/${_config}/xb")
    set(_xb_fetch_test_exe "${CMAKE_BINARY_DIR}/src/bin/${_config}/xb")
    break()
  endif()
endforeach()
if(NOT _xb_fetch_test_exe AND EXISTS "${CMAKE_BINARY_DIR}/src/bin/xb")
  set(_xb_fetch_test_exe "${CMAKE_BINARY_DIR}/src/bin/xb")
endif()

if(_xb_fetch_test_exe)
  xb_fetch_schemas(
    EXECUTABLE ${_xb_fetch_test_exe}
    URL ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_fetched_test
    SCHEMAS_VAR XB_FETCHED_SCHEMAS)

  xb_generate_cpp(
    TARGET xb_cmake_fetch_test_types
    SCHEMAS ${XB_FETCHED_SCHEMAS}
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_fetch_gen
    MODE HEADER_ONLY)

  add_executable(xb_cmake_fetch_test cmake_fetch_integration_test.cpp)
  target_link_libraries(xb_cmake_fetch_test
    PRIVATE Catch2::Catch2WithMain xb_cmake_fetch_test_types xb)
  target_compile_features(xb_cmake_fetch_test PRIVATE cxx_std_${xb_CXX_STANDARD})
  add_test(NAME xb_cmake_fetch_test COMMAND xb_cmake_fetch_test)
else()
  message(STATUS "xb_fetch_schemas test skipped: xb binary not yet built. "
    "Build first, then re-run cmake to enable.")
endif()
