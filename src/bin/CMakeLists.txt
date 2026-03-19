# ---------------------------------------------------------------------------
# Stage 1: Bootstrap binary (no wire/BES dependencies)
# ---------------------------------------------------------------------------

add_executable(xb_bootstrap xb_bootstrap.cpp)
target_link_libraries(xb_bootstrap PRIVATE xb::xb)
target_compile_features(xb_bootstrap PRIVATE cxx_std_${xb_CXX_STANDARD})

# ---------------------------------------------------------------------------
# Stage 2: Generate BES encoding types from xb-encoding.xsd
# ---------------------------------------------------------------------------

set(xb_ENCODING_XSD "${PROJECT_SOURCE_DIR}/schema/xb-encoding.xsd")
set(xb_ENCODING_TYPEMAP "${PROJECT_SOURCE_DIR}/schema/xb-encoding-typemap.xml")
set(xb_ENCODING_GEN_DIR "${PROJECT_SOURCE_DIR}/src/include/xb/wire")
set(xb_ENCODING_HPP "${xb_ENCODING_GEN_DIR}/encoding.hpp")

add_custom_command(
  OUTPUT "${xb_ENCODING_HPP}"
  COMMAND "$<TARGET_FILE:xb_bootstrap>"
    -t "${xb_ENCODING_TYPEMAP}"
    -n "http://xb.dev/encoding=xb::bes"
    -o "${xb_ENCODING_GEN_DIR}"
    "${xb_ENCODING_XSD}"
  DEPENDS "${xb_ENCODING_XSD}" "${xb_ENCODING_TYPEMAP}"
  COMMENT "Generating BES encoding types from xb-encoding.xsd (bootstrap)"
  VERBATIM)

add_custom_target(xb_bootstrap_generate DEPENDS "${xb_ENCODING_HPP}")
add_dependencies(xb_bootstrap_generate xb_bootstrap)

# ---------------------------------------------------------------------------
# Stage 3: Full CLI (depends on generated encoding types)
# ---------------------------------------------------------------------------

# Template the version into xb.json
configure_file(xb.json ${CMAKE_CURRENT_BINARY_DIR}/xb.json @ONLY)

json_commander_add_executable(xb_cli
  SCHEMA ${CMAKE_CURRENT_BINARY_DIR}/xb.json
  MAIN xb_cli::run
  FROM_HEADER xb_main.hpp
  xb_commands.cpp)

# Additional link dependencies beyond what json_commander provides
target_link_libraries(xb_cli PRIVATE xb::xb)
add_dependencies(xb_cli xb_bootstrap_generate)
if(CURL_FOUND)
  target_link_libraries(xb_cli PRIVATE CURL::libcurl)
  target_compile_definitions(xb_cli PRIVATE XB_HAS_CURL)
endif()

set_target_properties(xb_cli PROPERTIES
  OUTPUT_NAME xb
  EXPORT_NAME cli)

add_executable(xb::cli ALIAS xb_cli)

install(TARGETS xb_cli
  EXPORT xb_EXPORTS
  DESTINATION ${xb_INSTALL_BINDIR})
