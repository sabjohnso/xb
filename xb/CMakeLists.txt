configure_file(config.hpp.in config.hpp)

add_library(xb_header INTERFACE)
set_target_properties(xb_header PROPERTIES EXPORT_NAME header)
target_include_directories(xb_header
  INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${xb_INSTALL_INCLUDEDIR}>)

add_library(xb::header ALIAS xb_header)
  
install(TARGETS xb_header
  EXPORT xb_EXPORTS
  RUNTIME DESTINATION ${xb_INSTALL_BINDIR}
  LIBRARY DESTINATION ${xb_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${xb_INSTALL_LIBDIR})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
  DESTINATION ${xb_INSTALL_INCLUDEDIR}/xb)
