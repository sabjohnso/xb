add_executable(xb_qname_test qname_test.cpp)
target_link_libraries(xb_qname_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_qname_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_qname_test TARGET xb_qname_test)

add_executable(xb_any_attribute_test any_attribute_test.cpp)
target_link_libraries(xb_any_attribute_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_any_attribute_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_any_attribute_test TARGET xb_any_attribute_test)

add_executable(xb_any_element_test any_element_test.cpp)
target_link_libraries(xb_any_element_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_any_element_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_any_element_test TARGET xb_any_element_test)

add_executable(xb_xml_reader_test xml_reader_test.cpp)
target_link_libraries(xb_xml_reader_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xml_reader_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xml_reader_test TARGET xb_xml_reader_test)

add_executable(xb_xml_writer_test xml_writer_test.cpp)
target_link_libraries(xb_xml_writer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xml_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xml_writer_test TARGET xb_xml_writer_test)

add_executable(xb_integer_test integer_test.cpp)
target_link_libraries(xb_integer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_integer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_integer_test TARGET xb_integer_test)

add_executable(xb_decimal_test decimal_test.cpp)
target_link_libraries(xb_decimal_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_decimal_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_decimal_test TARGET xb_decimal_test)

add_executable(xb_year_month_duration_test year_month_duration_test.cpp)
target_link_libraries(xb_year_month_duration_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_year_month_duration_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_year_month_duration_test TARGET xb_year_month_duration_test)

add_executable(xb_day_time_duration_test day_time_duration_test.cpp)
target_link_libraries(xb_day_time_duration_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_day_time_duration_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_day_time_duration_test TARGET xb_day_time_duration_test)

add_executable(xb_duration_test duration_test.cpp)
target_link_libraries(xb_duration_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_duration_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_duration_test TARGET xb_duration_test)

add_executable(xb_date_test date_test.cpp)
target_link_libraries(xb_date_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_date_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_date_test TARGET xb_date_test)

add_executable(xb_time_test time_test.cpp)
target_link_libraries(xb_time_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_time_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_time_test TARGET xb_time_test)

add_executable(xb_date_time_test date_time_test.cpp)
target_link_libraries(xb_date_time_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_date_time_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_date_time_test TARGET xb_date_time_test)

add_executable(xb_type_map_test type_map_test.cpp)
target_link_libraries(xb_type_map_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_type_map_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_type_map_test TARGET xb_type_map_test)

add_executable(xb_schema_model_test schema_model_test.cpp)
target_link_libraries(xb_schema_model_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schema_model_test TARGET xb_schema_model_test)

add_executable(xb_schema_test schema_test.cpp)
target_link_libraries(xb_schema_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schema_test TARGET xb_schema_test)

add_executable(xb_schema_set_test schema_set_test.cpp)
target_link_libraries(xb_schema_set_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_set_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schema_set_test TARGET xb_schema_set_test)

add_executable(xb_schema_parser_test schema_parser_test.cpp)
target_link_libraries(xb_schema_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_schema_parser_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema)
ctest_deps_add_test(NAME xb_schema_parser_test TARGET xb_schema_parser_test)

add_executable(xb_codegen_test codegen_test.cpp)
target_link_libraries(xb_codegen_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_codegen_test TARGET xb_codegen_test)

add_executable(xb_naming_test naming_test.cpp)
target_link_libraries(xb_naming_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_naming_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_naming_test TARGET xb_naming_test)

add_executable(xb_cpp_code_test cpp_code_test.cpp)
target_link_libraries(xb_cpp_code_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_cpp_code_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_cpp_code_test TARGET xb_cpp_code_test)

add_executable(xb_typemap_schema_test typemap_schema_test.cpp)
target_link_libraries(xb_typemap_schema_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_typemap_schema_test PRIVATE cxx_std_${xb_CXX_STANDARD})
target_compile_definitions(xb_typemap_schema_test PRIVATE
  XB_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/schema)
ctest_deps_add_test(NAME xb_typemap_schema_test TARGET xb_typemap_schema_test)

add_executable(xb_xml_value_test xml_value_test.cpp)
target_link_libraries(xb_xml_value_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xml_value_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xml_value_test TARGET xb_xml_value_test)

add_executable(xb_xml_io_test xml_io_test.cpp)
target_link_libraries(xb_xml_io_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xml_io_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xml_io_test TARGET xb_xml_io_test)

add_executable(xb_xpath_expr_test xpath_expr_test.cpp)
target_link_libraries(xb_xpath_expr_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xpath_expr_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xpath_expr_test TARGET xb_xpath_expr_test)

add_executable(xb_doc_generator_test doc_generator_test.cpp)
target_link_libraries(xb_doc_generator_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_doc_generator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_doc_generator_test TARGET xb_doc_generator_test)

add_executable(xb_schema_fetcher_test schema_fetcher_test.cpp)
target_link_libraries(xb_schema_fetcher_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_fetcher_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schema_fetcher_test TARGET xb_schema_fetcher_test)

add_executable(xb_rng_pattern_test rng_pattern_test.cpp)
target_link_libraries(xb_rng_pattern_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_rng_pattern_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_pattern_test TARGET xb_rng_pattern_test)

add_executable(xb_rng_parser_test rng_parser_test.cpp)
target_link_libraries(xb_rng_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_parser_test TARGET xb_rng_parser_test)

add_executable(xb_rng_simplify_test rng_simplify_test.cpp)
target_link_libraries(xb_rng_simplify_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_simplify_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_simplify_test TARGET xb_rng_simplify_test)

add_executable(xb_rng_translator_test rng_translator_test.cpp)
target_link_libraries(xb_rng_translator_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_translator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_translator_test TARGET xb_rng_translator_test)

add_executable(xb_rng_compact_parser_test rng_compact_parser_test.cpp)
target_link_libraries(xb_rng_compact_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_compact_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_compact_parser_test TARGET xb_rng_compact_parser_test)

add_executable(xb_dtd_model_test dtd_model_test.cpp)
target_link_libraries(xb_dtd_model_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_dtd_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_dtd_model_test TARGET xb_dtd_model_test)

add_executable(xb_dtd_parser_test dtd_parser_test.cpp)
target_link_libraries(xb_dtd_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_dtd_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_dtd_parser_test TARGET xb_dtd_parser_test)

add_executable(xb_dtd_translator_test dtd_translator_test.cpp)
target_link_libraries(xb_dtd_translator_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_dtd_translator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_dtd_translator_test TARGET xb_dtd_translator_test)

add_executable(xb_schematron_model_test schematron_model_test.cpp)
target_link_libraries(xb_schematron_model_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_schematron_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schematron_model_test TARGET xb_schematron_model_test)

add_executable(xb_schematron_parser_test schematron_parser_test.cpp)
target_link_libraries(xb_schematron_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schematron_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schematron_parser_test TARGET xb_schematron_parser_test)

add_executable(xb_schematron_overlay_test schematron_overlay_test.cpp)
target_link_libraries(xb_schematron_overlay_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schematron_overlay_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schematron_overlay_test TARGET xb_schematron_overlay_test)

add_executable(xb_soap_model_test soap_model_test.cpp)
target_link_libraries(xb_soap_model_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_soap_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_soap_model_test TARGET xb_soap_model_test)

add_executable(xb_soap_envelope_test soap_envelope_test.cpp)
target_link_libraries(xb_soap_envelope_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_soap_envelope_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_soap_envelope_test TARGET xb_soap_envelope_test)

add_executable(xb_soap_fault_test soap_fault_test.cpp)
target_link_libraries(xb_soap_fault_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_soap_fault_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_soap_fault_test TARGET xb_soap_fault_test)

add_executable(xb_soap_header_test soap_header_test.cpp)
target_link_libraries(xb_soap_header_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_soap_header_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_soap_header_test TARGET xb_soap_header_test)

add_executable(xb_soap_integration_test soap_integration_test.cpp)
target_link_libraries(xb_soap_integration_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_soap_integration_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_soap_integration_test TARGET xb_soap_integration_test)

add_executable(xb_wsdl_model_test wsdl_model_test.cpp)
target_link_libraries(xb_wsdl_model_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_model_test TARGET xb_wsdl_model_test)

add_executable(xb_wsdl_parser_test wsdl_parser_test.cpp)
target_link_libraries(xb_wsdl_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_parser_test TARGET xb_wsdl_parser_test)

add_executable(xb_service_model_test service_model_test.cpp)
target_link_libraries(xb_service_model_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_service_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_service_model_test TARGET xb_service_model_test)

add_executable(xb_wsdl_resolver_test wsdl_resolver_test.cpp)
target_link_libraries(xb_wsdl_resolver_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_resolver_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_resolver_test TARGET xb_wsdl_resolver_test)

add_executable(xb_wsdl_support_test wsdl_support_test.cpp)
target_link_libraries(xb_wsdl_support_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_support_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_support_test TARGET xb_wsdl_support_test)

add_executable(xb_wsdl_codegen_test wsdl_codegen_test.cpp)
target_link_libraries(xb_wsdl_codegen_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_codegen_test TARGET xb_wsdl_codegen_test)

add_executable(xb_wsdl_server_codegen_test wsdl_server_codegen_test.cpp)
target_link_libraries(xb_wsdl_server_codegen_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl_server_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl_server_codegen_test TARGET xb_wsdl_server_codegen_test)

add_executable(xb_http_transport_test http_transport_test.cpp)
target_link_libraries(xb_http_transport_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_http_transport_test PRIVATE cxx_std_${xb_CXX_STANDARD})
if(CURL_FOUND)
  target_compile_definitions(xb_http_transport_test PRIVATE XB_HAS_CURL)
endif()
ctest_deps_add_test(NAME xb_http_transport_test TARGET xb_http_transport_test)

add_executable(xb_wsa_test wsa_test.cpp)
target_link_libraries(xb_wsa_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wsa_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsa_test TARGET xb_wsa_test)

add_executable(xb_wsa_headers_test wsa_headers_test.cpp)
target_link_libraries(xb_wsa_headers_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsa_headers_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsa_headers_test TARGET xb_wsa_headers_test)

add_executable(xb_wsa_handler_test wsa_handler_test.cpp)
target_link_libraries(xb_wsa_handler_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsa_handler_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsa_handler_test TARGET xb_wsa_handler_test)

add_executable(xb_wss_test wss_test.cpp)
target_link_libraries(xb_wss_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wss_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wss_test TARGET xb_wss_test)

add_executable(xb_wss_headers_test wss_headers_test.cpp)
target_link_libraries(xb_wss_headers_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wss_headers_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wss_headers_test TARGET xb_wss_headers_test)

add_executable(xb_wss_handler_test wss_handler_test.cpp)
target_link_libraries(xb_wss_handler_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wss_handler_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wss_handler_test TARGET xb_wss_handler_test)

add_executable(xb_wss_crypto_test wss_crypto_test.cpp)
target_link_libraries(xb_wss_crypto_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wss_crypto_test PRIVATE cxx_std_${xb_CXX_STANDARD})
if(OPENSSL_FOUND)
  target_compile_definitions(xb_wss_crypto_test PRIVATE XB_HAS_OPENSSL)
endif()
ctest_deps_add_test(NAME xb_wss_crypto_test TARGET xb_wss_crypto_test)

add_executable(xb_mime_multipart_test mime_multipart_test.cpp)
target_link_libraries(xb_mime_multipart_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_mime_multipart_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_mime_multipart_test TARGET xb_mime_multipart_test)

add_executable(xb_xop_test xop_test.cpp)
target_link_libraries(xb_xop_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xop_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xop_test TARGET xb_xop_test)

add_executable(xb_mtom_transport_test mtom_transport_test.cpp)
target_link_libraries(xb_mtom_transport_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_mtom_transport_test PRIVATE cxx_std_${xb_CXX_STANDARD})
if(CURL_FOUND)
  target_compile_definitions(xb_mtom_transport_test PRIVATE XB_HAS_CURL)
endif()
ctest_deps_add_test(NAME xb_mtom_transport_test TARGET xb_mtom_transport_test)

add_executable(xb_wsdl2_model_test wsdl2_model_test.cpp)
target_link_libraries(xb_wsdl2_model_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wsdl2_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl2_model_test TARGET xb_wsdl2_model_test)

add_executable(xb_wsdl2_parser_test wsdl2_parser_test.cpp)
target_link_libraries(xb_wsdl2_parser_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl2_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl2_parser_test TARGET xb_wsdl2_parser_test)

add_executable(xb_wsdl2_resolver_test wsdl2_resolver_test.cpp)
target_link_libraries(xb_wsdl2_resolver_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl2_resolver_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl2_resolver_test TARGET xb_wsdl2_resolver_test)

add_executable(xb_wsdl2_codegen_test wsdl2_codegen_test.cpp)
target_link_libraries(xb_wsdl2_codegen_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wsdl2_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wsdl2_codegen_test TARGET xb_wsdl2_codegen_test)

add_executable(xb_rng_writer_test rng_writer_test.cpp)
target_link_libraries(xb_rng_writer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_writer_test TARGET xb_rng_writer_test)

add_executable(xb_rng_compact_writer_test rng_compact_writer_test.cpp)
target_link_libraries(xb_rng_compact_writer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_compact_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_compact_writer_test TARGET xb_rng_compact_writer_test)

add_executable(xb_rng_roundtrip_test rng_roundtrip_test.cpp)
target_link_libraries(xb_rng_roundtrip_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_rng_roundtrip_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_rng_roundtrip_test TARGET xb_rng_roundtrip_test)

add_executable(xb_json_value_test json_value_test.cpp)
target_link_libraries(xb_json_value_test PRIVATE Catch2::Catch2WithMain xb::xb nlohmann_json::nlohmann_json)
target_compile_features(xb_json_value_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_json_value_test TARGET xb_json_value_test)

add_executable(xb_json_codegen_test json_codegen_test.cpp)
target_link_libraries(xb_json_codegen_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_json_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_json_codegen_test TARGET xb_json_codegen_test)

add_executable(xb_xsd_to_rng_test xsd_to_rng_test.cpp)
target_link_libraries(xb_xsd_to_rng_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xsd_to_rng_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xsd_to_rng_test TARGET xb_xsd_to_rng_test)

add_executable(xb_dtd_to_rng_test dtd_to_rng_test.cpp)
target_link_libraries(xb_dtd_to_rng_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_dtd_to_rng_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_dtd_to_rng_test TARGET xb_dtd_to_rng_test)

add_executable(xb_dtd_writer_test dtd_writer_test.cpp)
target_link_libraries(xb_dtd_writer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_dtd_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_dtd_writer_test TARGET xb_dtd_writer_test)

add_executable(xb_schema_to_dtd_test schema_to_dtd_test.cpp)
target_link_libraries(xb_schema_to_dtd_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_schema_to_dtd_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_schema_to_dtd_test TARGET xb_schema_to_dtd_test)

add_executable(xb_xsd_writer_test xsd_writer_test.cpp)
target_link_libraries(xb_xsd_writer_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xsd_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xsd_writer_test TARGET xb_xsd_writer_test)

add_executable(xb_code_formatter_test code_formatter_test.cpp)
target_link_libraries(xb_code_formatter_test PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_code_formatter_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_code_formatter_test TARGET xb_code_formatter_test)

add_executable(xb_wire_byteswap_test wire_byteswap_test.cpp)
target_link_libraries(xb_wire_byteswap_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_byteswap_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_byteswap_test TARGET xb_wire_byteswap_test)

add_executable(xb_wire_bits_test wire_bits_test.cpp)
target_link_libraries(xb_wire_bits_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_bits_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_bits_test TARGET xb_wire_bits_test)

add_executable(xb_wire_types_test wire_types_test.cpp)
target_link_libraries(xb_wire_types_test PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_types_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_types_test TARGET xb_wire_types_test)

# --- BES encoding types (checked-in, see src/include/xb/wire/encoding.hpp) ---

add_executable(xb_wire_encoding_model_test wire_encoding_model_test.cpp)
target_link_libraries(xb_wire_encoding_model_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wire_encoding_model_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_encoding_model_test TARGET xb_wire_encoding_model_test)

add_executable(xb_wire_selector_test wire_selector_test.cpp)
target_link_libraries(xb_wire_selector_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_selector_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_selector_test TARGET xb_wire_selector_test)

add_executable(xb_wire_bes_resolver_test wire_bes_resolver_test.cpp)
target_link_libraries(xb_wire_bes_resolver_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_bes_resolver_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_bes_resolver_test TARGET xb_wire_bes_resolver_test)

add_executable(xb_wire_encoding_resolver_test wire_encoding_resolver_test.cpp)
target_link_libraries(xb_wire_encoding_resolver_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wire_encoding_resolver_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_encoding_resolver_test TARGET xb_wire_encoding_resolver_test)

add_executable(xb_wire_layout_engine_test wire_layout_engine_test.cpp)
target_link_libraries(xb_wire_layout_engine_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_layout_engine_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_layout_engine_test TARGET xb_wire_layout_engine_test)

add_executable(xb_wire_binary_codegen_test wire_binary_codegen_test.cpp)
target_link_libraries(xb_wire_binary_codegen_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_binary_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_binary_codegen_test TARGET xb_wire_binary_codegen_test)

add_executable(xb_wire_view_decode_test wire_view_decode_test.cpp)
target_link_libraries(xb_wire_view_decode_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_view_decode_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_view_decode_test TARGET xb_wire_view_decode_test)

add_executable(xb_wire_owned_codegen_test wire_owned_codegen_test.cpp)
target_link_libraries(xb_wire_owned_codegen_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_owned_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_owned_codegen_test TARGET xb_wire_owned_codegen_test)

add_executable(xb_wire_concept_codegen_test wire_concept_codegen_test.cpp)
target_link_libraries(xb_wire_concept_codegen_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_concept_codegen_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_concept_codegen_test TARGET xb_wire_concept_codegen_test)

add_executable(xb_wire_layout_validation_test wire_layout_validation_test.cpp)
target_link_libraries(xb_wire_layout_validation_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_wire_layout_validation_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_layout_validation_test TARGET xb_wire_layout_validation_test)

add_executable(xb_wire_encoding_inheritance_test wire_encoding_inheritance_test.cpp)
target_link_libraries(xb_wire_encoding_inheritance_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_wire_encoding_inheritance_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_wire_encoding_inheritance_test TARGET xb_wire_encoding_inheritance_test)

add_executable(xb_xpath_parser_test xpath_parser_test.cpp)
target_link_libraries(xb_xpath_parser_test
  PRIVATE Catch2::Catch2WithMain xb::header)
target_compile_features(xb_xpath_parser_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xpath_parser_test TARGET xb_xpath_parser_test)

add_executable(xb_xpath_schema_eval_test xpath_schema_eval_test.cpp)
target_link_libraries(xb_xpath_schema_eval_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xpath_schema_eval_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xpath_schema_eval_test TARGET xb_xpath_schema_eval_test)

add_executable(xb_bes_to_xsd_test bes_to_xsd_test.cpp)
target_link_libraries(xb_bes_to_xsd_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_bes_to_xsd_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_bes_to_xsd_test TARGET xb_bes_to_xsd_test)

add_executable(xb_test_value_generator_test test_value_generator_test.cpp)
target_link_libraries(xb_test_value_generator_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_test_value_generator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_test_value_generator_test TARGET xb_test_value_generator_test)

add_executable(xb_test_vector_generator_test test_vector_generator_test.cpp)
target_link_libraries(xb_test_vector_generator_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_test_vector_generator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_test_vector_generator_test TARGET xb_test_vector_generator_test)

add_executable(xb_test_doc_generator_v2_test test_doc_generator_v2_test.cpp)
target_link_libraries(xb_test_doc_generator_v2_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_test_doc_generator_v2_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_test_doc_generator_v2_test TARGET xb_test_doc_generator_v2_test)

add_executable(xb_test_harness_generator_test test_harness_generator_test.cpp)
target_link_libraries(xb_test_harness_generator_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_test_harness_generator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_test_harness_generator_test TARGET xb_test_harness_generator_test)

add_executable(xb_coverage_analyzer_test coverage_analyzer_test.cpp)
target_link_libraries(xb_coverage_analyzer_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_coverage_analyzer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_coverage_analyzer_test TARGET xb_coverage_analyzer_test)

add_executable(xb_xinclude_reader_test xinclude_reader_test.cpp)
target_link_libraries(xb_xinclude_reader_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_xinclude_reader_test PRIVATE cxx_std_${xb_CXX_STANDARD})
ctest_deps_add_test(NAME xb_xinclude_reader_test TARGET xb_xinclude_reader_test)

add_executable(xb_bes_value_generator_test
  bes_value_generator_test.cpp
  ${PROJECT_SOURCE_DIR}/src/lib/bes_value_generator.cpp)
target_link_libraries(xb_bes_value_generator_test
  PRIVATE Catch2::Catch2WithMain xb::xb)
target_compile_features(xb_bes_value_generator_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_dependencies(xb_bes_value_generator_test xb_bootstrap_generate)
ctest_deps_add_test(NAME xb_bes_value_generator_test TARGET xb_bes_value_generator_test)
