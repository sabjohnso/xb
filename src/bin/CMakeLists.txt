add_executable(xb_cli main.cpp)
target_link_libraries(xb_cli PRIVATE xb)
target_compile_features(xb_cli PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_cli PRIVATE XB_VERSION="${PROJECT_VERSION}")
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
