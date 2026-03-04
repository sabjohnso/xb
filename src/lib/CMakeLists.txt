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
  schematron_overlay.cpp
  soap_envelope.cpp
  soap_fault.cpp
  wsdl_parser.cpp
  wsdl_resolver.cpp
  wsdl_codegen.cpp
  wsdl2_parser.cpp
  wsdl2_resolver.cpp
  wsa_headers.cpp
  wss_headers.cpp
  mime_multipart.cpp
  xop.cpp)

if(CURL_FOUND)
  target_sources(xb PRIVATE http_transport.cpp mtom_transport.cpp)
  target_link_libraries(xb PRIVATE $<BUILD_INTERFACE:CURL::libcurl>)
  target_compile_definitions(xb PUBLIC XB_HAS_CURL)
endif()

if(OPENSSL_FOUND)
  target_sources(xb PRIVATE wss_crypto.cpp)
  target_link_libraries(xb PRIVATE $<BUILD_INTERFACE:OpenSSL::Crypto>)
  target_compile_definitions(xb PUBLIC XB_HAS_OPENSSL)
endif()

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
