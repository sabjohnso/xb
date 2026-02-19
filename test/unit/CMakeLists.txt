add_executable(xb_qname_test qname_test.cpp)
target_link_libraries(xb_qname_test PRIVATE Catch2::Catch2WithMain xb_header)
target_compile_features(xb_qname_test PRIVATE cxx_std_${xb_CXX_STANDARD})
add_test(NAME xb_qname_test COMMAND xb_qname_test)
