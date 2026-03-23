add_executable(xb_any_element_round_trip_test any_element_round_trip_test.cpp)
target_link_libraries(xb_any_element_round_trip_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_any_element_round_trip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_any_element_round_trip_test TARGET xb_any_element_round_trip_test)

add_executable(xb_xml_round_trip_test xml_round_trip_test.cpp)
target_link_libraries(xb_xml_round_trip_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xml_round_trip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xml_round_trip_test TARGET xb_xml_round_trip_test)

add_executable(xb_codegen_compile_test codegen_compile_test.cpp)
target_link_libraries(xb_codegen_compile_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_codegen_compile_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_codegen_compile_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb::xb>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
ctest_deps_add_test(NAME xb_codegen_compile_test TARGET xb_codegen_compile_test)

add_executable(xb_serialization_test serialization_test.cpp)
target_link_libraries(xb_serialization_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_serialization_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_serialization_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb::xb>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
ctest_deps_add_test(NAME xb_serialization_test TARGET xb_serialization_test)

add_executable(xb_cli_test cli_test.cpp)
target_link_libraries(xb_cli_test PRIVATE Catch2::Catch2WithMain)
target_compile_features(xb_cli_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_cli_test PRIVATE
  XB_CLI=$<TARGET_FILE:xb::cli>
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema)
ctest_deps_add_test(NAME xb_cli_test TARGET xb_cli_test)

xb_generate_cpp(
  TARGET xb_cmake_gen_test_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_gen
  MODE HEADER_ONLY)

add_executable(xb_cmake_gen_test cmake_integration_test.cpp)
target_link_libraries(xb_cmake_gen_test
  PRIVATE Catch2::Catch2WithMain xb_cmake_gen_test_types xb::xb)
target_compile_features(xb_cmake_gen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_cmake_gen_test TARGET xb_cmake_gen_test)

xb_generate_cpp(
  TARGET xb_cmake_gen_split_test_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_gen_split
  MODE SPLIT)

add_executable(xb_cmake_gen_split_test cmake_split_integration_test.cpp)
target_link_libraries(xb_cmake_gen_split_test
  PRIVATE Catch2::Catch2WithMain xb_cmake_gen_split_test_types xb::xb)
target_compile_features(xb_cmake_gen_split_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_cmake_gen_split_test TARGET xb_cmake_gen_split_test)

xb_add_library(
  TARGET xb_cmake_add_lib_test_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xb-typemap.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cmake_add_lib
  MODE HEADER_ONLY)

add_executable(xb_cmake_add_lib_test cmake_add_library_test.cpp)
target_link_libraries(xb_cmake_add_lib_test
  PRIVATE Catch2::Catch2WithMain xb_cmake_add_lib_test_types)
target_compile_features(xb_cmake_add_lib_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_cmake_add_lib_test TARGET xb_cmake_add_lib_test)

add_executable(xb_json_round_trip_test json_round_trip_test.cpp)
target_link_libraries(xb_json_round_trip_test PRIVATE Catch2::Catch2WithMain xb::xb nlohmann_json::nlohmann_json)
target_compile_features(xb_json_round_trip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_json_round_trip_test PRIVATE
  XB_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/include
  XB_LIB_FILE=$<TARGET_FILE:xb::xb>
  XB_JSON_INCLUDE_DIR=$<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
  XB_SANITIZERS=$<IF:$<CONFIG:RelWithDebInfo>,1,0>)
ctest_deps_add_test(NAME xb_json_round_trip_test TARGET xb_json_round_trip_test)

# xeag-lexer IR tests (schema parsing, codegen IR inspection — no compilation)
add_executable(xb_xeag_lexer_test xeag_lexer_test.cpp)
target_link_libraries(xb_xeag_lexer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xeag_lexer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xeag_lexer_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema)
ctest_deps_add_test(NAME xb_xeag_lexer_test TARGET xb_xeag_lexer_test)

# xeag-lexer: wrapped header-only generated types
xb_add_library(
  TARGET xb_xeag_wrapped_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xeag-lexer.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_xeag_wrapped
  MODE HEADER_ONLY
  ENCAPSULATION wrapped)

add_executable(xb_xeag_lexer_wrapped_test xeag_lexer_wrapped_test.cpp)
target_link_libraries(xb_xeag_lexer_wrapped_test
  PRIVATE Catch2::Catch2WithMain xb_xeag_wrapped_types)
target_compile_features(xb_xeag_lexer_wrapped_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xeag_lexer_wrapped_test TARGET xb_xeag_lexer_wrapped_test)

# xeag-lexer: header-only generated types
xb_add_library(
  TARGET xb_xeag_header_only_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xeag-lexer.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_xeag_header_only
  MODE HEADER_ONLY)

add_executable(xb_xeag_lexer_header_only_test xeag_lexer_header_only_test.cpp)
target_link_libraries(xb_xeag_lexer_header_only_test
  PRIVATE Catch2::Catch2WithMain xb_xeag_header_only_types)
target_compile_features(xb_xeag_lexer_header_only_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xeag_lexer_header_only_test TARGET xb_xeag_lexer_header_only_test)

# xeag-lexer: pascal naming style generated types
xb_add_library(
  TARGET xb_xeag_pascal_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/schema/xeag-lexer.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_xeag_pascal
  MODE HEADER_ONLY
  TYPE_STYLE pascal)

add_executable(xb_xeag_lexer_pascal_test xeag_lexer_pascal_test.cpp)
target_link_libraries(xb_xeag_lexer_pascal_test
  PRIVATE Catch2::Catch2WithMain xb_xeag_pascal_types)
target_compile_features(xb_xeag_lexer_pascal_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xeag_lexer_pascal_test TARGET xb_xeag_lexer_pascal_test)

# --- BES-only code generation test (no XSD) ---
xb_generate_cpp(
  TARGET xb_bes_only_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/itch/itch.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_bes_only_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_bes_only_test bes_only_test.cpp)
target_link_libraries(xb_bes_only_test
  PRIVATE Catch2::Catch2WithMain xb_bes_only_types xb::header)
target_compile_features(xb_bes_only_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_bes_only_test TARGET xb_bes_only_test)

# --- BES backward-compatibility test: no namespace ---
xb_generate_cpp(
  TARGET xb_bes_global_ns_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/itch/itch-nonamespace.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_bes_global_ns_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_bes_global_ns_test bes_global_ns_test.cpp)
target_link_libraries(xb_bes_global_ns_test
  PRIVATE Catch2::Catch2WithMain xb_bes_global_ns_types xb::header)
target_compile_features(xb_bes_global_ns_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_bes_global_ns_test TARGET xb_bes_global_ns_test)

# --- ITCH binary encoding integration test ---
xb_generate_cpp(
  TARGET xb_itch_binary_types
  SCHEMAS ${PROJECT_SOURCE_DIR}/test/feature/itch/itch.xsd
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/itch/itch.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_itch_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_itch_binary_test itch_binary_test.cpp)
target_link_libraries(xb_itch_binary_test
  PRIVATE Catch2::Catch2WithMain xb_itch_binary_types xb::header)
target_compile_features(xb_itch_binary_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_itch_binary_test TARGET xb_itch_binary_test)

add_executable(xb_itch_benchmark_test itch_benchmark_test.cpp)
target_link_libraries(xb_itch_benchmark_test
  PRIVATE Catch2::Catch2WithMain xb_itch_binary_types xb::header)
target_compile_features(xb_itch_benchmark_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_itch_benchmark_test TARGET xb_itch_benchmark_test)

# --- xb_add_library with BES-only mode ---
xb_add_library(
  TARGET xb_bes_add_library_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/itch/itch.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_bes_add_library_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_bes_add_library_test bes_add_library_test.cpp)
target_link_libraries(xb_bes_add_library_test
  PRIVATE Catch2::Catch2WithMain xb_bes_add_library_types)
target_compile_features(xb_bes_add_library_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_bes_add_library_test TARGET xb_bes_add_library_test)

# --- MessagePack BES test ---
xb_add_library(
  TARGET xb_msgpack_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/msgpack/msgpack.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_msgpack_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_msgpack_test msgpack_test.cpp)
target_link_libraries(xb_msgpack_test
  PRIVATE Catch2::Catch2WithMain xb_msgpack_types)
target_compile_features(xb_msgpack_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_msgpack_test TARGET xb_msgpack_test)

# --- CBOR (RFC 8949) BES test ---
xb_add_library(
  TARGET xb_cbor_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/cbor/cbor.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_cbor_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_cbor_test cbor_test.cpp)
target_link_libraries(xb_cbor_test
  PRIVATE Catch2::Catch2WithMain xb_cbor_types)
target_compile_features(xb_cbor_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_cbor_test TARGET xb_cbor_test)

# --- FXT (Fuchsia Trace Format) BES test ---
xb_add_library(
  TARGET xb_fxt_types
  ENCODING ${PROJECT_SOURCE_DIR}/test/feature/fxt/fxt.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xb_fxt_gen
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(xb_fxt_test fxt_test.cpp)
target_link_libraries(xb_fxt_test
  PRIVATE Catch2::Catch2WithMain xb_fxt_types)
target_compile_features(xb_fxt_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_fxt_test TARGET xb_fxt_test)

add_executable(xb_cmake_fetch_install_test cmake_fetch_install_test.cpp)
target_link_libraries(xb_cmake_fetch_install_test PRIVATE Catch2::Catch2WithMain)
target_compile_features(xb_cmake_fetch_install_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_cmake_fetch_install_test PRIVATE
  XB_BUILD_DIR=${CMAKE_BINARY_DIR}
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema
  XB_BUILD_CONFIG=$<CONFIG>)
ctest_deps_add_test(NAME xb_cmake_fetch_install_test TARGET xb_cmake_fetch_install_test)
