add_executable(xb_xsd_bootstrap_test xsd_bootstrap_test.cpp)
target_link_libraries(xb_xsd_bootstrap_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xsd_bootstrap_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xsd_bootstrap_test PRIVATE
  XB_XSD_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR}
  XB_XSD_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/generated)
ctest_deps_add_test(NAME xb_xsd_bootstrap_test TARGET xb_xsd_bootstrap_test)

# --- XSD Translation Test ---
# Generate C++ types from the XMLSchema.xsd schema-for-schemas at build time,
# then compile the generated code alongside a translation layer that converts
# the generated xs:: types to xb:: semantic model types.

xb_generate_cpp(
  TARGET xb_xsd_generated_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/XMLSchema.xsd ${CMAKE_CURRENT_SOURCE_DIR}/xml.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/xsd_generated
  MODE SPLIT
  NAMESPACE_MAP "http://www.w3.org/2001/XMLSchema=xs" "http://www.w3.org/XML/1998/namespace=xml_ns"
  GENERATE_DOCS)

# --- XSD Translation Test ---
# Use a proper custom_command with OUTPUT so Ninja can track the dependency
# across all configs.
set(XSD_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/xsd_generated)
set(XSD_GEN_HPP ${XSD_GEN_DIR}/xml_schema.hpp)
set(XSD_GEN_CPP ${XSD_GEN_DIR}/xml_schema.cpp)
set(XSD_GEN_NS_HPP ${XSD_GEN_DIR}/namespace.hpp)
set(XSD_GEN_NS_CPP ${XSD_GEN_DIR}/namespace.cpp)

# Per-config stamp + copy_if_different to avoid re-triggering on
# config switch (same pattern as xbGenerateCpp.cmake).
set(XSD_GEN_TMP ${CMAKE_BINARY_DIR}/_xb_gen_tmp/xsd_translate/$<CONFIG>)
set(XSD_GEN_STAMP_DIR ${CMAKE_BINARY_DIR}/_xb_gen_stamps/xsd_translate/$<CONFIG>)
set(XSD_GEN_STAMP ${XSD_GEN_STAMP_DIR}/.xsd_translate.stamp)

add_custom_command(
  OUTPUT ${XSD_GEN_STAMP} ${XSD_GEN_HPP} ${XSD_GEN_CPP} ${XSD_GEN_NS_HPP} ${XSD_GEN_NS_CPP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${XSD_GEN_TMP}
  COMMAND $<TARGET_FILE:xb_cli> generate
    -o ${XSD_GEN_TMP}
    -n "http://www.w3.org/2001/XMLSchema=xs"
    -n "http://www.w3.org/XML/1998/namespace=xml_ns"
    --generate-docs
    ${CMAKE_CURRENT_SOURCE_DIR}/XMLSchema.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/xml.xsd
  COMMAND ${CMAKE_COMMAND}
    -DSRC_DIR=${XSD_GEN_TMP} -DDST_DIR=${XSD_GEN_DIR}
    -P ${PROJECT_SOURCE_DIR}/cmake/xbCopyIfDifferent.cmake
  COMMAND ${CMAKE_COMMAND} -E touch ${XSD_GEN_STAMP}
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/XMLSchema.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/xml.xsd
  COMMENT "Generating XSD types for translation test"
  VERBATIM)

add_custom_target(xsd_translate_generate DEPENDS ${XSD_GEN_STAMP})
add_dependencies(xsd_translate_generate xb_cli)

# Only compile the xs namespace source — xml_ns has a codegen bug with empty
# enum values that needs to be fixed separately.
add_library(xb_xsd_generated_lib STATIC ${XSD_GEN_CPP} xsd_parse_stubs.cpp)
add_dependencies(xb_xsd_generated_lib xsd_translate_generate)
target_include_directories(xb_xsd_generated_lib PUBLIC ${XSD_GEN_DIR})
target_link_libraries(xb_xsd_generated_lib PUBLIC xb::xb)
target_compile_features(xb_xsd_generated_lib PUBLIC cxx_std_${xb_CXX_STANDARD})

add_executable(xb_xsd_translate_test xsd_translate_test.cpp)
target_link_libraries(xb_xsd_translate_test
  PRIVATE Catch2::Catch2WithMain xb_xsd_generated_lib)
target_compile_features(xb_xsd_translate_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xsd_translate_test PRIVATE
  XB_XSD_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR})
ctest_deps_add_test(NAME xb_xsd_translate_test TARGET xb_xsd_translate_test)
