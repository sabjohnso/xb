# Template the version into xb.json
configure_file(xb.json ${CMAKE_CURRENT_BINARY_DIR}/xb.json @ONLY)

json_commander_add_executable(xb_cli
  SCHEMA ${CMAKE_CURRENT_BINARY_DIR}/xb.json
  MAIN xb_cli::run
  FROM_HEADER xb_main.hpp
  xb_commands.cpp)

# Additional link dependencies beyond what json_commander provides
target_link_libraries(xb_cli PRIVATE xb::xb)
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
