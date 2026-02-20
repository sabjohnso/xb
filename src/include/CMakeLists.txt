add_subdirectory(xb)

add_library(xb_header INTERFACE)
set_target_properties(xb_header PROPERTIES EXPORT_NAME header)
target_include_directories(xb_header
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${xb_INSTALL_INCLUDEDIR}>)

install(TARGETS xb_header
  EXPORT xb_EXPORTS
  RUNTIME DESTINATION ${xb_INSTALL_BINDIR}
  LIBRARY DESTINATION ${xb_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${xb_INSTALL_LIBDIR})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
  xb/qname.hpp
  xb/xml_reader.hpp
  xb/xml_writer.hpp
  xb/expat_reader.hpp
  xb/ostream_writer.hpp
  DESTINATION ${xb_INSTALL_INCLUDEDIR}/xb)
