add_library(xb STATIC
  any_element.cpp
  decimal.cpp
  expat_reader.cpp
  integer.cpp
  ostream_writer.cpp
  year_month_duration.cpp
  day_time_duration.cpp
  duration.cpp
  date.cpp
  time.cpp
  date_time.cpp
  type_map.cpp
  model_group.cpp
  schema_parser.cpp
  schema_set.cpp
  cpp_writer.cpp
  naming.cpp
  codegen.cpp
  xml_value.cpp
  xml_io.cpp
  xpath_expr.cpp
  doc_generator.cpp
  schema_fetcher.cpp
  rng_parser.cpp
  rng_simplify.cpp
  rng_translator.cpp
  rng_compact_parser.cpp
  dtd_parser.cpp
  dtd_translator.cpp
  schematron_parser.cpp
  schematron_overlay.cpp)

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
