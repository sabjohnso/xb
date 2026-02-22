#include <xb/naming.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;

// TDD step 1: Simple lowercase -> unchanged
TEST_CASE("simple lowercase unchanged", "[naming]") {
  CHECK(to_snake_case("order") == "order");
}

// TDD step 2: PascalCase -> snake_case
TEST_CASE("PascalCase to snake_case", "[naming]") {
  CHECK(to_snake_case("OrderType") == "order_type");
  CHECK(to_snake_case("MyOrder") == "my_order");
}

// TDD step 3: camelCase -> snake_case
TEST_CASE("camelCase to snake_case", "[naming]") {
  CHECK(to_snake_case("orderQty") == "order_qty");
  CHECK(to_snake_case("firstName") == "first_name");
}

// TDD step 4: Abbreviation runs
TEST_CASE("abbreviation runs", "[naming]") {
  CHECK(to_snake_case("HTMLParser") == "html_parser");
  CHECK(to_snake_case("XMLReader") == "xml_reader");
  CHECK(to_snake_case("getHTTPResponse") == "get_http_response");
  CHECK(to_snake_case("IOError") == "io_error");
}

// TDD step 5: Already snake_case -> unchanged
TEST_CASE("already snake_case unchanged", "[naming]") {
  CHECK(to_snake_case("order_type") == "order_type");
  CHECK(to_snake_case("my_order") == "my_order");
}

// TDD step 6: Reserved C++ keyword -> trailing underscore
TEST_CASE("reserved keyword escaping", "[naming]") {
  CHECK(to_cpp_identifier("class") == "class_");
  CHECK(to_cpp_identifier("int") == "int_");
  CHECK(to_cpp_identifier("return") == "return_");
  CHECK(to_cpp_identifier("namespace") == "namespace_");
  CHECK(to_cpp_identifier("operator") == "operator_");
  // Non-keyword should not be modified (except snake_case conversion)
  CHECK(to_cpp_identifier("order") == "order");
  CHECK(to_cpp_identifier("OrderType") == "order_type");
}

// TDD step 7: Leading digit -> underscore prefix
TEST_CASE("leading digit", "[naming]") {
  CHECK(to_cpp_identifier("3DPoint") == "_3d_point");
  CHECK(to_cpp_identifier("2ndItem") == "_2nd_item");
}

// TDD step 8: Hyphen/dot in XSD name -> underscore
TEST_CASE("hyphen and dot to underscore", "[naming]") {
  CHECK(to_snake_case("foo-bar") == "foo_bar");
  CHECK(to_snake_case("foo.bar") == "foo_bar");
  CHECK(to_snake_case("my-element.name") == "my_element_name");
}

// TDD step 9: Namespace URI to C++ namespace
TEST_CASE("namespace URI to C++ namespace", "[naming]") {
  codegen_options opts;
  // Explicit mapping
  opts.namespace_map["http://example.com/order/v2"] = "example::order::v2";
  CHECK(cpp_namespace_for("http://example.com/order/v2", opts) ==
        "example::order::v2");
}

TEST_CASE("namespace URI auto-derivation", "[naming]") {
  codegen_options opts;
  // No explicit mapping -> derive from URI
  // Dots in hostname become namespace separators
  CHECK(cpp_namespace_for("http://example.com/order/v2", opts) ==
        "example::com::order::v2");
  CHECK(cpp_namespace_for("urn:example:messages", opts) == "example::messages");
}

TEST_CASE("empty namespace", "[naming]") {
  codegen_options opts;
  CHECK(cpp_namespace_for("", opts) == "");
}
