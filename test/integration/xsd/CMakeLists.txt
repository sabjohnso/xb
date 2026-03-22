add_executable(xb_xsd_bootstrap_test xsd_bootstrap_test.cpp)
target_link_libraries(xb_xsd_bootstrap_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xsd_bootstrap_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xsd_bootstrap_test PRIVATE
  XB_XSD_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR}
  XB_XSD_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/generated)
add_test(NAME xb_xsd_bootstrap_test COMMAND xb_xsd_bootstrap_test)

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

add_custom_command(
  OUTPUT ${XSD_GEN_HPP} ${XSD_GEN_CPP}
  COMMAND $<TARGET_FILE:xb_cli> generate
    -o ${XSD_GEN_DIR}
    -n "http://www.w3.org/2001/XMLSchema=xs"
    -n "http://www.w3.org/XML/1998/namespace=xml_ns"
    --generate-docs
    ${CMAKE_CURRENT_SOURCE_DIR}/XMLSchema.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/xml.xsd
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/XMLSchema.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/xml.xsd
    xb_cli
  COMMENT "Generating XSD types for translation test"
  VERBATIM)

# Only compile the xs namespace source — xml_ns has a codegen bug with empty
# enum values that needs to be fixed separately.
add_library(xb_xsd_generated_lib STATIC ${XSD_GEN_CPP} xsd_parse_stubs.cpp)
target_include_directories(xb_xsd_generated_lib PUBLIC ${XSD_GEN_DIR})
target_link_libraries(xb_xsd_generated_lib PUBLIC xb::xb)
target_compile_features(xb_xsd_generated_lib PUBLIC cxx_std_${xb_CXX_STANDARD})
# TODO: Fix codegen to not emit empty std::visit bodies for choice
# groups that contain only group_ref/model_group particles (no direct
# element dispatches).  Remove this once fixed.
target_compile_options(xb_xsd_generated_lib PRIVATE -Wno-unused-local-typedef -Wno-unused-variable)

add_executable(xb_xsd_translate_test xsd_translate_test.cpp)
target_link_libraries(xb_xsd_translate_test
  PRIVATE Catch2::Catch2WithMain xb_xsd_generated_lib)
target_compile_features(xb_xsd_translate_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_xsd_translate_test PRIVATE
  XB_XSD_SCHEMA_DIR=${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME xb_xsd_translate_test COMMAND xb_xsd_translate_test)
