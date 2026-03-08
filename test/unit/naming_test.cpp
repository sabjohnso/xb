#include <xb/naming.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>

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

TEST_CASE("to_cpp_identifier: special characters become valid identifiers",
          "[naming]") {
  // Question mark (FIXML SecurityType_enum_t has "?" as a value)
  auto q = to_cpp_identifier("?");
  CHECK(!q.empty());
  CHECK(q.find('?') == std::string::npos);

  // Plus sign
  auto plus = to_cpp_identifier("+");
  CHECK(!plus.empty());
  CHECK(plus.find('+') == std::string::npos);

  // Mixed: alphanumeric + special
  auto mixed = to_cpp_identifier("A+B");
  CHECK(mixed.find('+') == std::string::npos);
  CHECK(!mixed.empty());
}

// --- Group 1: File Suffixes ---

TEST_CASE("codegen_options has configurable file suffixes", "[naming]") {
  codegen_options opts;
  // Defaults
  CHECK(opts.header_suffix == ".hpp");
  CHECK(opts.source_suffix == ".cpp");

  // Custom suffixes
  opts.header_suffix = ".h";
  opts.source_suffix = ".cc";
  CHECK(opts.header_suffix == ".h");
  CHECK(opts.source_suffix == ".cc");
}

// --- Group 1: Default Namespace ---

TEST_CASE("default_namespace_for extracts last segment", "[naming]") {
  // HTTP URL — last path segment
  CHECK(default_namespace_for("http://www.fixprotocol.org/FIXML-5-0-SP2", {}) ==
        "fixml_5_0_sp2");

  // URN — last colon segment
  CHECK(default_namespace_for("urn:oasis:names:specification:ubl:schema:xsd:"
                              "CommonBasicComponents-2",
                              {}) == "common_basic_components_2");

  // Simple URL with version path
  CHECK(default_namespace_for("http://example.com/order/v2", {}) == "v2");
}

TEST_CASE("default_namespace_for falls back on empty URI", "[naming]") {
  CHECK(default_namespace_for("", {}).empty());
}

TEST_CASE("default_namespace_for produces valid C++ identifier", "[naming]") {
  // Namespace ending with a number
  auto ns = default_namespace_for("http://example.com/2", {});
  CHECK(!ns.empty());
  // Must not start with digit
  CHECK(!std::isdigit(static_cast<unsigned char>(ns[0])));
}

TEST_CASE("default_namespace_for disambiguates collisions", "[naming]") {
  // Two namespaces with the same last segment need disambiguation
  std::set<std::string> all_ns = {"http://example.com/v1/types",
                                  "http://example.com/v2/types"};

  auto ns1 = default_namespace_for("http://example.com/v1/types", all_ns);
  auto ns2 = default_namespace_for("http://example.com/v2/types", all_ns);
  CHECK(ns1 != ns2);
}

TEST_CASE("default_namespace_for respects explicit mapping", "[naming]") {
  codegen_options opts;
  opts.namespace_map["http://example.com/order"] = "my::order";

  // When an explicit mapping exists, cpp_namespace_for still takes precedence
  // default_namespace_for should also respect it
  CHECK(default_namespace_for("http://example.com/order", {}, opts) ==
        "my::order");
}

// --- Group 2: Naming Conventions ---

// -- Conversion functions --

TEST_CASE("to_pascal_case from snake_case", "[naming]") {
  CHECK(to_pascal_case("order_type") == "OrderType");
  CHECK(to_pascal_case("my_order") == "MyOrder");
  CHECK(to_pascal_case("simple") == "Simple");
}

TEST_CASE("to_pascal_case from camelCase", "[naming]") {
  CHECK(to_pascal_case("orderType") == "OrderType");
  CHECK(to_pascal_case("firstName") == "FirstName");
}

TEST_CASE("to_pascal_case from PascalCase unchanged", "[naming]") {
  CHECK(to_pascal_case("OrderType") == "OrderType");
}

TEST_CASE("to_pascal_case with abbreviations", "[naming]") {
  CHECK(to_pascal_case("html_parser") == "HtmlParser");
  CHECK(to_pascal_case("xml_reader") == "XmlReader");
}

TEST_CASE("to_camel_case from snake_case", "[naming]") {
  CHECK(to_camel_case("order_type") == "orderType");
  CHECK(to_camel_case("my_order") == "myOrder");
  CHECK(to_camel_case("simple") == "simple");
}

TEST_CASE("to_camel_case from PascalCase", "[naming]") {
  CHECK(to_camel_case("OrderType") == "orderType");
}

TEST_CASE("to_upper_snake_case from snake_case", "[naming]") {
  CHECK(to_upper_snake_case("order_type") == "ORDER_TYPE");
  CHECK(to_upper_snake_case("my_order") == "MY_ORDER");
}

TEST_CASE("to_upper_snake_case from camelCase", "[naming]") {
  CHECK(to_upper_snake_case("orderType") == "ORDER_TYPE");
}

TEST_CASE("to_upper_snake_case from PascalCase", "[naming]") {
  CHECK(to_upper_snake_case("OrderType") == "ORDER_TYPE");
}

// -- apply_naming_style dispatch --

TEST_CASE("apply_naming_style dispatches to correct function", "[naming]") {
  CHECK(apply_naming_style("OrderType", naming_style::snake_case) ==
        "order_type");
  CHECK(apply_naming_style("order_type", naming_style::pascal_case) ==
        "OrderType");
  CHECK(apply_naming_style("order_type", naming_style::camel_case) ==
        "orderType");
  CHECK(apply_naming_style("order_type", naming_style::upper_snake) ==
        "ORDER_TYPE");
  CHECK(apply_naming_style("OrderType", naming_style::original) == "OrderType");
}

// -- Idempotence property --

TEST_CASE("naming style conversions are idempotent", "[naming]") {
  std::vector<std::string> names = {"order_type", "OrderType", "orderType",
                                    "ORDER_TYPE", "HTMLParser"};
  for (const auto& name : names) {
    auto s = to_snake_case(name);
    CHECK(to_snake_case(s) == s);

    auto p = to_pascal_case(name);
    CHECK(to_pascal_case(p) == p);

    auto c = to_camel_case(name);
    CHECK(to_camel_case(c) == c);

    auto u = to_upper_snake_case(name);
    CHECK(to_upper_snake_case(u) == u);
  }
}

// -- naming_options defaults --

TEST_CASE("naming_options has sensible defaults", "[naming]") {
  naming_options opts;
  CHECK(opts.type_style == naming_style::snake_case);
  CHECK(opts.field_style == naming_style::snake_case);
  CHECK(opts.enum_style == naming_style::snake_case);
  CHECK(opts.function_style == naming_style::snake_case);
  CHECK(opts.type_rules.empty());
  CHECK(opts.field_rules.empty());
}

// -- apply_naming with category --

TEST_CASE("apply_naming applies style then rules", "[naming]") {
  naming_options opts;
  opts.type_style = naming_style::pascal_case;
  opts.type_rules = {{"Type$", ""}}; // strip trailing "Type"

  CHECK(apply_naming("order_type", naming_category::type_, opts) == "Order");
}

TEST_CASE("apply_naming sanitizes result as C++ identifier", "[naming]") {
  naming_options opts;
  opts.field_style = naming_style::snake_case;

  // "class" is a keyword — should get suffixed
  CHECK(apply_naming("class", naming_category::field, opts) == "class_");
}

TEST_CASE("apply_naming with different categories", "[naming]") {
  naming_options opts;
  opts.type_style = naming_style::pascal_case;
  opts.field_style = naming_style::snake_case;
  opts.enum_style = naming_style::upper_snake;
  opts.function_style = naming_style::camel_case;

  CHECK(apply_naming("order_type", naming_category::type_, opts) ==
        "OrderType");
  CHECK(apply_naming("OrderType", naming_category::field, opts) ==
        "order_type");
  CHECK(apply_naming("active_status", naming_category::enum_value, opts) ==
        "ACTIVE_STATUS");
  CHECK(apply_naming("read_order", naming_category::function, opts) ==
        "readOrder");
}
