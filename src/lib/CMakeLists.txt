add_library(xb_library STATIC
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
  test_value_generator.cpp
  test_vector_generator.cpp
  test_doc_generator.cpp
  test_harness_generator.cpp
  coverage_analyzer.cpp
  xinclude_reader.cpp
  railroad.cpp
  railroad_svg.cpp
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
  xop.cpp
  rng_writer.cpp
  rng_compact_writer.cpp
  xsd_to_rng.cpp
  dtd_to_rng.cpp
  dtd_writer.cpp
  schema_to_dtd.cpp
  xsd_writer.cpp
  json_codegen.cpp
  code_formatter.cpp)

if(CURL_FOUND)
  target_sources(xb_library PRIVATE http_transport.cpp mtom_transport.cpp)
  target_link_libraries(xb_library PRIVATE $<BUILD_INTERFACE:CURL::libcurl>)
  target_compile_definitions(xb_library PUBLIC XB_HAS_CURL)
endif()

include(CheckIncludeFileCXX)
check_include_file_cxx("sys/socket.h" XB_HAS_SYS_SOCKET)
if(XB_HAS_SYS_SOCKET)
  target_sources(xb_library PRIVATE http_listener.cpp)
  target_compile_definitions(xb_library PUBLIC XB_HAS_POSIX_SOCKETS)
endif()

if(OPENSSL_FOUND)
  target_sources(xb_library PRIVATE wss_crypto.cpp)
  target_link_libraries(xb_library PRIVATE $<BUILD_INTERFACE:OpenSSL::Crypto>)
  target_compile_definitions(xb_library PUBLIC XB_HAS_OPENSSL)
endif()

target_link_libraries(xb_library
  PUBLIC xb::header
  PRIVATE EXPAT::EXPAT)

target_compile_features(xb_library PRIVATE cxx_std_${xb_CXX_STANDARD})

set_target_properties(xb_library PROPERTIES EXPORT_NAME library)

add_library(xb::library ALIAS xb_library)

install(TARGETS xb_library
  EXPORT xb_EXPORTS
  RUNTIME DESTINATION ${xb_INSTALL_BINDIR}
  LIBRARY DESTINATION ${xb_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${xb_INSTALL_LIBDIR})
