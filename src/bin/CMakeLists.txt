# ---------------------------------------------------------------------------
# Stage 1: Bootstrap binary (no wire/BES dependencies)
# ---------------------------------------------------------------------------

add_executable(xb_bootstrap xb_bootstrap.cpp)
target_link_libraries(xb_bootstrap PRIVATE xb::xb)
target_compile_features(xb_bootstrap PRIVATE cxx_std_${xb_CXX_STANDARD})

# ---------------------------------------------------------------------------
# Stage 2: Generate BES encoding types from xb-encoding.xsd
# ---------------------------------------------------------------------------
#
# The generated encoding.hpp is written to the build tree so that
# switching Ninja Multi-Config configurations does not touch a shared
# source-tree file and cascade unnecessary rebuilds.  The build-tree
# include directory (src/include binary dir) is already on xb_header's
# include path and takes precedence over the source tree.
#
# Ninja Multi-Config shares .ninja_log across configs and tracks the
# command hash for each output.  Because the COMMAND uses
# $<TARGET_FILE:xb_bootstrap> (a config-specific path), a
# config-independent stamp would have different command hashes per
# config, causing Ninja to re-run the command on every config switch.
#
# To avoid this:
#   1. The stamp file is per-config ($<CONFIG> in path) so each
#      config has its own command-hash record.
#   2. The generator writes to a per-config temp dir, then
#      copy_if_different moves the result to the final location.
#      This prevents touching encoding.hpp's timestamp when the
#      content hasn't changed (which would cascade recompilation).

set(xb_ENCODING_XSD "${PROJECT_SOURCE_DIR}/schema/xb-encoding.xsd")
set(xb_ENCODING_TYPEMAP "${PROJECT_SOURCE_DIR}/schema/xb-encoding-typemap.xml")
set(xb_ENCODING_GEN_DIR "${PROJECT_BINARY_DIR}/src/include/xb/wire")
set(xb_ENCODING_TMP_DIR "${PROJECT_BINARY_DIR}/_encoding_tmp/$<CONFIG>")
set(xb_ENCODING_STAMP "${PROJECT_BINARY_DIR}/_encoding_stamps/$<CONFIG>/.encoding.stamp")

add_custom_command(
  OUTPUT "${xb_ENCODING_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${xb_ENCODING_GEN_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${xb_ENCODING_TMP_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/_encoding_stamps/$<CONFIG>"
  COMMAND "$<TARGET_FILE:xb_bootstrap>"
    -t "${xb_ENCODING_TYPEMAP}"
    -n "http://xb.dev/encoding=xb::bes"
    -o "${xb_ENCODING_TMP_DIR}"
    "${xb_ENCODING_XSD}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${xb_ENCODING_TMP_DIR}/encoding.hpp"
    "${xb_ENCODING_GEN_DIR}/encoding.hpp"
  COMMAND "${CMAKE_COMMAND}" -E touch "${xb_ENCODING_STAMP}"
  DEPENDS "${xb_ENCODING_XSD}" "${xb_ENCODING_TYPEMAP}"
  COMMENT "Generating BES encoding types from xb-encoding.xsd (bootstrap)"
  VERBATIM)

add_custom_target(xb_bootstrap_generate DEPENDS "${xb_ENCODING_STAMP}")
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
