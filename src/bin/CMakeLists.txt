# Template the version into xb.json
configure_file(xb.json ${CMAKE_CURRENT_BINARY_DIR}/xb.json @ONLY)

json_commander_add_executable(xb_cli
  SCHEMA ${CMAKE_CURRENT_BINARY_DIR}/xb.json
  MAIN xb_cli::run
  FROM_HEADER xb_main.hpp
  xb_commands.cpp)

# Overwrite the generated main to bypass schema::Loader which crashes on
# the recursive $ref in the json-commander metaschema.  The schema is
# validated at development time; runtime validation is unnecessary.
file(READ ${CMAKE_CURRENT_BINARY_DIR}/xb.json JCMD_SCHEMA_CONTENT)
set(JCMD_FROM_HEADER_INCLUDE "#include \"xb_main.hpp\"")
set(JCMD_MAIN_FN "xb_cli::run")
configure_file(
  xb_jcmd_main.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/xb_cli_jcmd_main.cpp
  @ONLY)

# Additional link dependencies beyond what json_commander provides
target_link_libraries(xb_cli PRIVATE xb)
if(CURL_FOUND)
  target_link_libraries(xb_cli PRIVATE CURL::libcurl)
  target_compile_definitions(xb_cli PRIVATE XB_HAS_CURL)
endif()

set_target_properties(xb_cli PROPERTIES
  OUTPUT_NAME xb
  EXPORT_NAME cli)

install(TARGETS xb_cli
  EXPORT xb_EXPORTS
  DESTINATION ${xb_INSTALL_BINDIR})
