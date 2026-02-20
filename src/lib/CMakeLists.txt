add_library(xb STATIC
  any_element.cpp
  expat_reader.cpp
  ostream_writer.cpp)

target_link_libraries(xb
  PUBLIC xb_header
  PRIVATE EXPAT::EXPAT)

target_compile_features(xb PRIVATE cxx_std_${xb_CXX_STANDARD})

set_target_properties(xb PROPERTIES EXPORT_NAME xb)

install(TARGETS xb
  EXPORT xb_EXPORTS
  RUNTIME DESTINATION ${xb_INSTALL_BINDIR}
  LIBRARY DESTINATION ${xb_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${xb_INSTALL_LIBDIR})
