add_executable(xb_qname_test qname_test.cpp)
target_link_libraries(xb_qname_test PRIVATE Catch2::Catch2WithMain xb_header)
target_compile_features(xb_qname_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_qname_test COMMAND xb_qname_test)

add_executable(xb_any_attribute_test any_attribute_test.cpp)
target_link_libraries(xb_any_attribute_test PRIVATE Catch2::Catch2WithMain xb_header)
target_compile_features(xb_any_attribute_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_any_attribute_test COMMAND xb_any_attribute_test)

add_executable(xb_any_element_test any_element_test.cpp)
target_link_libraries(xb_any_element_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_any_element_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_any_element_test COMMAND xb_any_element_test)

add_executable(xb_xml_reader_test xml_reader_test.cpp)
target_link_libraries(xb_xml_reader_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_xml_reader_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_xml_reader_test COMMAND xb_xml_reader_test)

add_executable(xb_xml_writer_test xml_writer_test.cpp)
target_link_libraries(xb_xml_writer_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_xml_writer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_xml_writer_test COMMAND xb_xml_writer_test)

add_executable(xb_integer_test integer_test.cpp)
target_link_libraries(xb_integer_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_integer_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_integer_test COMMAND xb_integer_test)

add_executable(xb_decimal_test decimal_test.cpp)
target_link_libraries(xb_decimal_test PRIVATE Catch2::Catch2WithMain xb)
target_compile_features(xb_decimal_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_decimal_test COMMAND xb_decimal_test)
