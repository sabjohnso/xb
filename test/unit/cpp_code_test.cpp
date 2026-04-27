#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;

static const cpp_writer writer;

// TDD step 1: Empty file -> #pragma once + trailing newline
TEST_CASE("empty file produces pragma once", "[cpp_writer]") {
  cpp_file file;
  file.filename = "empty.hpp";

  auto result = writer.write(file);
  CHECK(result == "#pragma once\n");
}

// TDD step 2: File with system include
TEST_CASE("system include", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.includes.push_back({"<string>"});

  auto result = writer.write(file);
  CHECK(result == "#pragma once\n\n#include <string>\n");
}

// TDD step 3: File with local include
TEST_CASE("local include", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.includes.push_back({"\"xb/integer.hpp\""});

  auto result = writer.write(file);
  CHECK(result == "#pragma once\n\n#include \"xb/integer.hpp\"\n");
}

// TDD step 4: Empty struct
TEST_CASE("empty struct", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cpp_struct{"foo_bar", {}, false, {}}}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

struct foo_bar {};

} // namespace ns
)";
  CHECK(result == expected);
}

// TDD step 5: Struct with fields
TEST_CASE("struct with fields", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "order";
  s.generate_equality = false;
  s.fields.push_back({"std::string", "id", ""});
  s.fields.push_back({"int", "quantity", ""});
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

struct order {
  std::string id{};
  int quantity{};
};

} // namespace ns
)";
  CHECK(result == expected);
}

// TDD step 6: Struct with operator==
TEST_CASE("struct with defaulted equality", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "point";
  s.generate_equality = true;
  s.fields.push_back({"int", "x", ""});
  s.fields.push_back({"int", "y", ""});
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

struct point {
  int x{};
  int y{};

  bool operator==(const struct point&) const = default;
};

} // namespace ns
)";
  CHECK(result == expected);
}

// TDD step 7: Enum class
TEST_CASE("enum class", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_enum e;
  e.name = "color";
  e.values.push_back({"red", "red"});
  e.values.push_back({"green", "green"});
  e.values.push_back({"blue", "blue"});
  file.namespaces.push_back({"ns", {std::move(e)}});

  auto result = writer.write(file);
  // Enum generates to_string/from_string alongside the enum definition
  CHECK(result.find("enum class color {\n  red,\n  green,\n  blue,\n};") !=
        std::string::npos);
  CHECK(result.find("to_string(color v)") != std::string::npos);
  CHECK(result.find("color_from_string(std::string_view s)") !=
        std::string::npos);
}

// TDD step 8: Enum with to_string / from_string
TEST_CASE("enum to_string and from_string", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.includes.push_back({"<stdexcept>"});
  file.includes.push_back({"<string>"});
  file.includes.push_back({"<string_view>"});
  cpp_enum e;
  e.name = "side";
  e.values.push_back({"buy", "Buy"});
  e.values.push_back({"sell", "Sell"});
  file.namespaces.push_back({"ns", {std::move(e)}});

  auto result = writer.write(file);
  // Should contain to_string and from_string free functions
  CHECK(result.find("inline std::string_view to_string(side v)") !=
        std::string::npos);
  CHECK(result.find("inline side side_from_string(std::string_view s)") !=
        std::string::npos);
  // to_string maps to XML values
  CHECK(result.find("\"Buy\"") != std::string::npos);
  CHECK(result.find("\"Sell\"") != std::string::npos);
  // from_string parses XML values
  CHECK(result.find("return side::buy;") != std::string::npos);
  CHECK(result.find("return side::sell;") != std::string::npos);
}

// TDD step 9: Type alias
TEST_CASE("type alias", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back(
      {"ns", {cpp_type_alias{"order_id", "std::string"}}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

using order_id = std::string;

} // namespace ns
)";
  CHECK(result == expected);
}

// TDD step 10: Forward declaration
TEST_CASE("forward declaration", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cpp_forward_decl{"order"}}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

struct order;

} // namespace ns
)";
  CHECK(result == expected);
}

// TDD step 11: Namespace wrapping
TEST_CASE("namespace wrapping", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"my_lib", {cpp_type_alias{"id", "std::string"}}});

  auto result = writer.write(file);
  CHECK(result.find("namespace my_lib {") != std::string::npos);
  CHECK(result.find("} // namespace my_lib") != std::string::npos);
}

// TDD step 12: Nested namespaces
TEST_CASE("nested namespaces", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"a::b", {cpp_type_alias{"id", "std::string"}}});

  auto result = writer.write(file);
  CHECK(result.find("namespace a::b {") != std::string::npos);
  CHECK(result.find("} // namespace a::b") != std::string::npos);
}

// TDD step 13: Complete file with includes + namespace + multiple declarations
TEST_CASE("complete file", "[cpp_writer]") {
  cpp_file file;
  file.filename = "order.hpp";
  file.includes.push_back({"<optional>"});
  file.includes.push_back({"<string>"});
  file.includes.push_back({"<vector>"});

  cpp_enum status;
  status.name = "order_status";
  status.values.push_back({"pending", "Pending"});
  status.values.push_back({"filled", "Filled"});

  cpp_struct order;
  order.name = "order";
  order.generate_equality = true;
  order.fields.push_back({"std::string", "id", ""});
  order.fields.push_back({"order_status", "status", ""});
  order.fields.push_back({"std::vector<std::string>", "items", ""});

  cpp_namespace ns;
  ns.name = "trading";
  ns.declarations.push_back(std::move(status));
  ns.declarations.push_back(std::move(order));
  file.namespaces.push_back(std::move(ns));

  auto result = writer.write(file);
  // Verify structure
  CHECK(result.find("#pragma once") != std::string::npos);
  CHECK(result.find("#include <optional>") != std::string::npos);
  CHECK(result.find("#include <string>") != std::string::npos);
  CHECK(result.find("#include <vector>") != std::string::npos);
  CHECK(result.find("namespace trading {") != std::string::npos);
  CHECK(result.find("enum class order_status {") != std::string::npos);
  CHECK(result.find("struct order {") != std::string::npos);
  CHECK(result.find("std::string id{};") != std::string::npos);
  CHECK(result.find("order_status status{};") != std::string::npos);
  CHECK(result.find("bool operator==(const struct order&) const = default;") !=
        std::string::npos);
  CHECK(result.find("} // namespace trading") != std::string::npos);
}

// TDD step 14: Fields with optional, vector, variant
TEST_CASE("fields with template types", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "message";
  s.generate_equality = false;
  s.fields.push_back({"std::optional<std::string>", "header", ""});
  s.fields.push_back({"std::vector<int>", "items", ""});
  s.fields.push_back({"std::variant<int, std::string>", "payload", ""});
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("using header_type = std::optional<std::string>;") !=
        std::string::npos);
  CHECK(result.find("header_type header{};") != std::string::npos);
  CHECK(result.find("using items_type = std::vector<int>;") !=
        std::string::npos);
  CHECK(result.find("items_type items{};") != std::string::npos);
  CHECK(result.find("using payload_type = std::variant<int, std::string>;") !=
        std::string::npos);
  CHECK(result.find("payload_type payload{};") != std::string::npos);
}

// Additional: Field with default value
TEST_CASE("field with default value", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "config";
  s.generate_equality = false;
  s.fields.push_back({"int", "timeout", "30"});
  s.fields.push_back({"std::string", "name", "\"default\""});
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("int timeout = 30;") != std::string::npos);
  CHECK(result.find("std::string name = \"default\";") != std::string::npos);
}

// Additional: Multiple namespaces in one file
TEST_CASE("multiple namespaces", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns1", {cpp_type_alias{"a", "int"}}});
  file.namespaces.push_back({"ns2", {cpp_type_alias{"b", "double"}}});

  auto result = writer.write(file);
  CHECK(result.find("namespace ns1 {") != std::string::npos);
  CHECK(result.find("namespace ns2 {") != std::string::npos);
}

// Additional: System includes sorted before local includes
TEST_CASE("system and local includes ordering", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.includes.push_back({"\"xb/types.hpp\""});
  file.includes.push_back({"<string>"});
  file.includes.push_back({"\"xb/base.hpp\""});
  file.includes.push_back({"<vector>"});

  auto result = writer.write(file);
  // System includes should come before local includes
  auto sys_pos = result.find("#include <string>");
  auto local_pos = result.find("#include \"xb/types.hpp\"");
  CHECK(sys_pos < local_pos);
}

// ===== file_kind and write_options =====

TEST_CASE("non-inline function in header mode renders declaration only",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.kind = file_kind::header;
  cpp_function fn;
  fn.return_type = "int";
  fn.name = "compute";
  fn.parameters = "int a, int b";
  fn.body = "  return a + b;\n";
  fn.is_inline = false;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("int compute(int a, int b);") != std::string::npos);
  CHECK(result.find("return a + b") == std::string::npos);
}

TEST_CASE("non-inline function in source mode renders definition",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  cpp_function fn;
  fn.return_type = "int";
  fn.name = "compute";
  fn.parameters = "int a, int b";
  fn.body = "  return a + b;\n";
  fn.is_inline = false;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("int compute(int a, int b) {") != std::string::npos);
  CHECK(result.find("return a + b") != std::string::npos);
  CHECK(result.find("inline") == std::string::npos);
}

TEST_CASE("source mode skips structs", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  cpp_struct s;
  s.name = "point";
  s.fields.push_back({"int", "x", ""});
  s.generate_equality = false;
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("struct") == std::string::npos);
}

TEST_CASE("source mode skips enums", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  cpp_enum e;
  e.name = "color";
  e.values.push_back({"red", "red"});
  file.namespaces.push_back({"ns", {std::move(e)}});

  auto result = writer.write(file);
  CHECK(result.find("enum") == std::string::npos);
}

TEST_CASE("source mode skips type aliases", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  file.namespaces.push_back({"ns", {cpp_type_alias{"my_id", "std::string"}}});

  auto result = writer.write(file);
  CHECK(result.find("using") == std::string::npos);
}

TEST_CASE("source mode skips forward declarations", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  file.namespaces.push_back({"ns", {cpp_forward_decl{"order"}}});

  auto result = writer.write(file);
  CHECK(result.find("struct order") == std::string::npos);
}

TEST_CASE("source mode skips inline functions", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  cpp_function fn;
  fn.return_type = "void";
  fn.name = "helper";
  fn.body = "  // noop\n";
  fn.is_inline = true;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("helper") == std::string::npos);
}

TEST_CASE("source mode omits pragma once", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;

  auto result = writer.write(file);
  CHECK(result.find("#pragma once") == std::string::npos);
}

TEST_CASE("header mode with inline functions unchanged", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.kind = file_kind::header;
  cpp_function fn;
  fn.return_type = "void";
  fn.name = "foo";
  fn.body = "";
  fn.is_inline = true;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("inline void foo()") != std::string::npos);
  CHECK(result.find("#pragma once") != std::string::npos);
}

TEST_CASE("default write reads file.kind", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.cpp";
  file.kind = file_kind::source;
  cpp_function fn;
  fn.return_type = "void";
  fn.name = "setup";
  fn.parameters = "int x";
  fn.body = "  (void)x;\n";
  fn.is_inline = false;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  // Default write() should use file.kind (source), rendering the definition
  auto result = writer.write(file);
  CHECK(result.find("void setup(int x) {") != std::string::npos);
  CHECK(result.find("(void)x;") != std::string::npos);
}

// ===== cpp_function rendering =====

// TDD step 15: Render empty inline function
TEST_CASE("render empty inline function", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_function fn;
  fn.return_type = "void";
  fn.name = "foo";
  fn.body = "";
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("inline void foo() {\n}\n") != std::string::npos);
}

// TDD step 16: Render function with parameters and body
TEST_CASE("render function with params and body", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_function fn;
  fn.return_type = "int";
  fn.name = "add";
  fn.parameters = "int a, int b";
  fn.body = "  return a + b;\n";
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  CHECK(result.find("inline int add(int a, int b) {\n  return a + b;\n}\n") !=
        std::string::npos);
}

// TDD step 17: Render non-inline function in header -> declaration only
TEST_CASE("render non-inline function in header is declaration only",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_function fn;
  fn.return_type = "void";
  fn.name = "setup";
  fn.parameters = "int x";
  fn.body = "  (void)x;\n";
  fn.is_inline = false;
  file.namespaces.push_back({"ns", {std::move(fn)}});

  auto result = writer.write(file);
  // In header mode, non-inline -> declaration only
  CHECK(result.find("void setup(int x);") != std::string::npos);
  CHECK(result.find("(void)x") == std::string::npos);
  CHECK(result.find("inline void setup") == std::string::npos);
}

TEST_CASE("cpp_class renders wrapper with accessors", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "order";
  cls.raw_struct_name = "order_data";
  cls.fields = {
      {"std::string", "cl_ord_id", ""},
      {"std::optional<std::string>", "side", ""},
      {"std::vector<fill>", "fills", ""},
  };
  cls.doc_comment = "An order.";

  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Doc comment in Doxygen block format
  CHECK(result.find("/**") != std::string::npos);
  CHECK(result.find("@brief") != std::string::npos);
  CHECK(result.find("An order.") != std::string::npos);
  CHECK(result.find("*/") != std::string::npos);
  // Class declaration
  CHECK(result.find("class order") != std::string::npos);
  // Raw struct member (no detail:: prefix)
  CHECK(result.find("order_data data_") != std::string::npos);
  CHECK(result.find("detail::") == std::string::npos);
  // Const ref getter for scalar
  CHECK(result.find("const std::string& cl_ord_id() const") !=
        std::string::npos);
  // Setter for scalar
  CHECK(result.find("void set_cl_ord_id(std::string value)") !=
        std::string::npos);
  // Optional clear
  CHECK(result.find("void clear_side()") != std::string::npos);
  // Sequence indexed access
  CHECK(result.find("fills(std::size_t i)") != std::string::npos);
  // Sequence push_back
  CHECK(result.find("fills_push_back(") != std::string::npos);
  // Sequence size
  CHECK(result.find("fills_size()") != std::string::npos);
  // Equality
  CHECK(result.find("operator==") != std::string::npos);
  // Constructor from raw struct (no detail:: prefix)
  CHECK(result.find("explicit order(order_data") != std::string::npos);
  // No data() accessor — encapsulation must not be broken
  CHECK(result.find("data()") == std::string::npos);
}

TEST_CASE("cpp_class split mode: header has declarations, source has "
          "definitions",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "order";
  cls.raw_struct_name = "order_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::string", "cl_ord_id", ""},
      {"std::optional<std::string>", "side", ""},
      {"std::vector<fill>", "fills", ""},
  };

  file.namespaces.push_back({"ns", {cls}});

  // Header: declarations only, no bodies
  auto header = writer.write(file, {file_kind::header});
  CHECK(header.find("class order") != std::string::npos);
  CHECK(header.find("cl_ord_id() const;") != std::string::npos);
  CHECK(header.find("set_cl_ord_id(") != std::string::npos);
  // No detail:: prefix
  CHECK(header.find("detail::") == std::string::npos);
  // Should NOT contain function bodies (return data_.)
  CHECK(header.find("return data_.") == std::string::npos);
  CHECK(header.find("std::move(value)") == std::string::npos);
  // No data() accessor
  CHECK(header.find("data()") == std::string::npos);

  // Source: out-of-line definitions with class:: prefix
  auto source = writer.write(file, {file_kind::source});
  CHECK(source.find("order::cl_ord_id()") != std::string::npos);
  CHECK(source.find("order::set_cl_ord_id(") != std::string::npos);
  CHECK(source.find("order::side()") != std::string::npos);
  CHECK(source.find("order::clear_side()") != std::string::npos);
  CHECK(source.find("order::fills(std::size_t i)") != std::string::npos);
  CHECK(source.find("order::fills_push_back(") != std::string::npos);
  CHECK(source.find("order::fills_size()") != std::string::npos);
  CHECK(source.find("return data_.") != std::string::npos);
  // No data() accessor in source either
  CHECK(source.find("data()") == std::string::npos);
}

TEST_CASE("struct fields use default initialization", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "msg";
  s.generate_equality = false;
  s.fields = {
      {"int32_t", "seq", ""},
      {"std::string", "text", ""},
  };
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("int32_t seq{};") != std::string::npos);
  CHECK(result.find("std::string text{};") != std::string::npos);
}

TEST_CASE("struct field with explicit default keeps it", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "config";
  s.generate_equality = false;
  s.fields = {
      {"int", "retries", "3"},
  };
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("int retries = 3;") != std::string::npos);
  // Should NOT also have {}
  CHECK(result.find("retries{}") == std::string::npos);
}

TEST_CASE("template specialization fields get type aliases in struct",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "message";
  s.generate_equality = false;
  s.fields = {
      {"std::variant<int, std::string>", "payload", ""},
      {"std::vector<int>", "items", ""},
      {"std::optional<std::string>", "header", ""},
      {"std::string", "tag", ""},
      {"int", "count", ""},
  };
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  // Type aliases for all template specializations
  CHECK(result.find("using payload_type = std::variant<int, std::string>;") !=
        std::string::npos);
  CHECK(result.find("using items_type = std::vector<int>;") !=
        std::string::npos);
  CHECK(result.find("using header_type = std::optional<std::string>;") !=
        std::string::npos);
  // Fields use the aliases
  CHECK(result.find("payload_type payload{};") != std::string::npos);
  CHECK(result.find("items_type items{};") != std::string::npos);
  CHECK(result.find("header_type header{};") != std::string::npos);
  // Non-template fields are unchanged
  CHECK(result.find("std::string tag{};") != std::string::npos);
  CHECK(result.find("int count{};") != std::string::npos);
  // No alias for non-template types
  CHECK(result.find("using tag_type") == std::string::npos);
  CHECK(result.find("using count_type") == std::string::npos);
}

TEST_CASE("alias skipped when it would collide with struct field type",
          "[cpp_writer]") {
  // A field named "mapping" of type std::vector<mapping_type> would produce
  // "using mapping_type = std::vector<mapping_type>" — a self-referential
  // alias. The writer must skip the alias and use the raw type instead.
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "typemap";
  s.generate_equality = false;
  s.fields = {
      {"std::vector<mapping_type>", "mapping", ""},
  };
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  // No alias should be emitted (it would be self-referential)
  CHECK(result.find("using mapping_type") == std::string::npos);
  // Field uses raw type directly
  CHECK(result.find("std::vector<mapping_type> mapping{};") !=
        std::string::npos);
}

TEST_CASE("nested specialization emits inner element alias", "[cpp_writer]") {
  // std::vector<std::variant<A,B>> should emit two aliases:
  //   using choice_element_type = std::variant<A, B>;
  //   using choice_type = std::vector<choice_element_type>;
  cpp_file file;
  file.filename = "test.hpp";
  cpp_struct s;
  s.name = "match_type_data";
  s.generate_equality = false;
  s.fields = {
      {"std::vector<std::variant<int, std::string>>", "choice", ""},
      {"std::optional<std::string>", "value", ""},
  };
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  // Inner alias for the variant element
  CHECK(result.find(
            "using choice_element_type = std::variant<int, std::string>;") !=
        std::string::npos);
  // Outer alias uses inner alias
  CHECK(result.find("using choice_type = std::vector<choice_element_type>;") !=
        std::string::npos);
  // Field uses outer alias
  CHECK(result.find("choice_type choice{};") != std::string::npos);
  // Non-nested template still gets a simple alias
  CHECK(result.find("using value_type = std::optional<std::string>;") !=
        std::string::npos);
}

TEST_CASE("class accessors reference nested inner alias from raw struct",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "match_type";
  cls.raw_struct_name = "match_type_data";
  cls.fields = {
      {"std::vector<std::variant<int, std::string>>", "choice", ""},
      {"std::string", "name", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  // Indexed access uses element alias
  CHECK(result.find(
            "match_type_data::choice_element_type& choice(std::size_t i)") !=
        std::string::npos);
  // Iterator uses container alias
  CHECK(result.find("match_type_data::choice_type::iterator choice_begin()") !=
        std::string::npos);
}

TEST_CASE("class accessors use template alias from raw struct",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "event";
  cls.raw_struct_name = "event_data";
  cls.fields = {
      {"std::variant<int, std::string>", "payload", ""},
      {"std::vector<int>", "items", ""},
      {"std::string", "tag", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  // Getter/setter for variant field uses alias
  CHECK(result.find("const event_data::payload_type&") != std::string::npos);
  CHECK(result.find("event_data::payload_type value") != std::string::npos);
  // Non-template field uses raw type
  CHECK(result.find("const std::string&") != std::string::npos);
}

TEST_CASE("doc comment uses doxygen block format with brief and details",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  // Class with multi-line doc comment
  cpp_class cls;
  cls.name = "person";
  cls.raw_struct_name = "person_data";
  cls.doc_comment = "A person record.\nContains name and age.";
  cls.fields = {{"std::string", "name", ""}};
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  // Should use /** ... */ block style
  CHECK(result.find("/**") != std::string::npos);
  CHECK(result.find(" */") != std::string::npos);
  // Should have @brief with generated summary
  CHECK(result.find("@brief") != std::string::npos);
  // Should have @details with the annotation
  CHECK(result.find("@details A person record.") != std::string::npos);
  CHECK(result.find("Contains name and age.") != std::string::npos);
  // Should have @nosubgrouping for classes
  CHECK(result.find("@nosubgrouping") != std::string::npos);
  // Should NOT use old /// style
  CHECK(result.find("///") == std::string::npos);
}

TEST_CASE("struct doc comment uses doxygen block format", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_struct s;
  s.name = "point";
  s.doc_comment = "A 2D point.";
  s.fields = {{"int", "x", ""}, {"int", "y", ""}};
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  CHECK(result.find("/**") != std::string::npos);
  CHECK(result.find("A 2D point.") != std::string::npos);
  CHECK(result.find(" */") != std::string::npos);
  CHECK(result.find("///") == std::string::npos);
}

TEST_CASE("struct doc comment escapes embedded comment terminators",
          "[cpp_writer][security]") {
  // A schema annotation that contains "*/" must not be allowed to
  // close the surrounding /** ... */ block in the generated header.
  // If unescaped, an attacker who controls the schema text can inject
  // arbitrary C++ tokens into the codegen output.
  cpp_file file;
  file.filename = "test.hpp";

  cpp_struct s;
  s.name = "point";
  s.doc_comment = "harmless prefix */ int evil_injection; /*";
  s.fields = {{"int", "x", ""}};
  file.namespaces.push_back({"ns", {std::move(s)}});

  auto result = writer.write(file);
  // After sanitisation, "*/" becomes "* /" inside the comment text,
  // so the injected sequence "*/ int evil_injection;" no longer
  // appears verbatim in the output.
  CHECK(result.find("*/ int evil_injection") == std::string::npos);
  // The annotation text itself is still emitted (as a benign comment),
  // but split so the "*/" cannot close the Doxygen block.
  CHECK(result.find("* / int evil_injection") != std::string::npos);
  // Exactly one comment block was opened and closed.
  std::size_t opens = 0;
  for (std::size_t p = result.find("/**"); p != std::string::npos;
       p = result.find("/**", p + 1)) {
    ++opens;
  }
  std::size_t closes = 0;
  for (std::size_t p = result.find("*/"); p != std::string::npos;
       p = result.find("*/", p + 1)) {
    ++closes;
  }
  CHECK(opens == closes);
}

TEST_CASE("class doc comment escapes embedded comment terminators",
          "[cpp_writer][security]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "order";
  cls.raw_struct_name = "order_data";
  cls.doc_comment = "details */ int evil_class_injection; /*";
  cls.fields = {{"int", "id", ""}};
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  CHECK(result.find("*/ int evil_class_injection") == std::string::npos);
  CHECK(result.find("* / int evil_class_injection") != std::string::npos);
}

TEST_CASE("class without doc comment gets generated brief", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "widget";
  cls.raw_struct_name = "widget_data";
  cls.fields = {{"int", "id", ""}};
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  // Should still get a doc comment with generated brief
  CHECK(result.find("/**") != std::string::npos);
  CHECK(result.find("@brief") != std::string::npos);
  CHECK(result.find("@nosubgrouping") != std::string::npos);
}

TEST_CASE("generate_member_docs emits doxygen for all public members",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "order";
  cls.raw_struct_name = "order_data";
  cls.generate_member_docs = true;
  cls.fields = {
      {"std::string", "cl_ord_id", ""},
      {"std::optional<std::string>", "side", ""},
      {"std::vector<fill>", "fills", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Constructor doc
  CHECK(result.find("/** @brief Construct from raw data. */") !=
        std::string::npos);
  // Default constructor doc
  CHECK(result.find("/** @brief Default constructor. */") != std::string::npos);

  // Scalar getter doc
  CHECK(result.find("/** @brief Get the cl_ord_id value. */") !=
        std::string::npos);
  // Scalar setter doc
  CHECK(result.find("/** @brief Set the cl_ord_id value. */") !=
        std::string::npos);

  // Optional getter doc
  CHECK(result.find("/** @brief Get the side value. */") != std::string::npos);
  // Optional setter doc
  CHECK(result.find("/** @brief Set the side value. */") != std::string::npos);
  // Optional clear doc
  CHECK(result.find("/** @brief Clear the side value. */") !=
        std::string::npos);

  // Sequence indexed access doc
  CHECK(result.find("/** @brief Access fills element at index. */") !=
        std::string::npos);
  // Sequence iterator doc
  CHECK(
      result.find("/** @brief Return iterator to the beginning of fills. */") !=
      std::string::npos);
  CHECK(result.find("/** @brief Return iterator past the end of fills. */") !=
        std::string::npos);
  // Sequence size doc
  CHECK(result.find("/** @brief Return the number of fills elements. */") !=
        std::string::npos);
  // Sequence clear doc
  CHECK(result.find("/** @brief Remove all fills elements. */") !=
        std::string::npos);
  // Sequence push_back doc
  CHECK(result.find("/** @brief Append an element to fills. */") !=
        std::string::npos);
  // Sequence pop_back doc
  CHECK(result.find("/** @brief Remove the last fills element. */") !=
        std::string::npos);

  // Equality doc
  CHECK(result.find("/** @brief Equality comparison. */") != std::string::npos);
}

TEST_CASE("generate_member_docs false suppresses member docs", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_class cls;
  cls.name = "widget";
  cls.raw_struct_name = "widget_data";
  cls.generate_member_docs = false;
  cls.fields = {{"int", "id", ""}};
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Class-level doc is always generated
  CHECK(result.find("@brief Class corresponding to") != std::string::npos);
  // But member docs should not appear
  CHECK(result.find("Default constructor") == std::string::npos);
  CHECK(result.find("Get the id") == std::string::npos);
}

TEST_CASE("sequence field emits iterator-based API in class decl",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "container";
  cls.raw_struct_name = "container_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<int>", "items", ""},
      {"std::string", "name", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Indexed access (const and mutable)
  CHECK(result.find("const int& items(std::size_t i) const;") !=
        std::string::npos);
  CHECK(result.find("int& items(std::size_t i);") != std::string::npos);

  // Iterators
  CHECK(result.find("container_data::items_type::iterator items_begin();") !=
        std::string::npos);
  CHECK(result.find("container_data::items_type::iterator items_end();") !=
        std::string::npos);
  CHECK(
      result.find(
          "container_data::items_type::const_iterator items_begin() const;") !=
      std::string::npos);
  CHECK(result.find(
            "container_data::items_type::const_iterator items_end() const;") !=
        std::string::npos);
  CHECK(
      result.find(
          "container_data::items_type::const_iterator items_cbegin() const;") !=
      std::string::npos);
  CHECK(result.find(
            "container_data::items_type::const_iterator items_cend() const;") !=
        std::string::npos);

  // Size and clear (kept from before)
  CHECK(result.find("std::size_t items_size() const;") != std::string::npos);
  CHECK(result.find("void clear_items();") != std::string::npos);

  // Mutators
  CHECK(result.find("items_type::iterator items_insert(") != std::string::npos);
  CHECK(result.find("items_type::iterator items_erase(") != std::string::npos);
  CHECK(result.find("void items_push_back(") != std::string::npos);
  CHECK(result.find("void items_pop_back();") != std::string::npos);
  CHECK(result.find("items_emplace_back(") != std::string::npos);
  CHECK(result.find("items_emplace(") != std::string::npos);
  CHECK(result.find("items_insert_range(") != std::string::npos);
  CHECK(result.find("items_append_range(") != std::string::npos);

  // Should NOT have the old container-ref return
  CHECK(result.find("const container_data::items_type& items() const") ==
        std::string::npos);
  // Should NOT have old add_ method
  CHECK(result.find("add_item") == std::string::npos);
}

TEST_CASE(
    "sequence field with nested type uses element alias in indexed access",
    "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "match_type";
  cls.raw_struct_name = "match_type_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<std::variant<int, std::string>>", "choice", ""},
      {"std::string", "name", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Indexed access uses element alias
  CHECK(result.find("const match_type_data::choice_element_type& "
                    "choice(std::size_t i) const;") != std::string::npos);
  CHECK(result.find(
            "match_type_data::choice_element_type& choice(std::size_t i);") !=
        std::string::npos);

  // Iterator types use container alias
  CHECK(result.find("match_type_data::choice_type::iterator choice_begin();") !=
        std::string::npos);
  CHECK(result.find("match_type_data::choice_type::const_iterator "
                    "choice_begin() const;") != std::string::npos);
}

TEST_CASE("sole sequence field gets unprefixed aliases", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "list_type";
  cls.raw_struct_name = "list_type_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<int>", "items", ""},
      {"std::string", "name", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Prefixed methods should exist
  CHECK(result.find("items_begin()") != std::string::npos);
  CHECK(result.find("items_end()") != std::string::npos);
  CHECK(result.find("items_size()") != std::string::npos);
  CHECK(result.find("items(std::size_t i)") != std::string::npos);

  // Unprefixed aliases should also exist (sole sequence)
  CHECK(result.find("begin()") != std::string::npos);
  CHECK(result.find("end()") != std::string::npos);
  CHECK(result.find("size()") != std::string::npos);
}

TEST_CASE("unprefixed aliases get docs noting they alias prefixed methods",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "list_type";
  cls.raw_struct_name = "list_type_data";
  cls.generate_member_docs = true;
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<int>", "items", ""},
      {"std::string", "name", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Unprefixed aliases should have docs noting the aliased method
  CHECK(result.find("Alias for items_begin") != std::string::npos);
  CHECK(result.find("Alias for items_end") != std::string::npos);
  CHECK(result.find("Alias for items_cbegin") != std::string::npos);
  CHECK(result.find("Alias for items_cend") != std::string::npos);
  CHECK(result.find("Alias for items_size") != std::string::npos);
}

TEST_CASE("multiple sequence fields do not get unprefixed aliases",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "multi";
  cls.raw_struct_name = "multi_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<int>", "items", ""},
      {"std::vector<std::string>", "tags", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);

  // Prefixed methods should exist for both
  CHECK(result.find("items_begin()") != std::string::npos);
  CHECK(result.find("tags_begin()") != std::string::npos);

  // Check that there is no standalone "begin()" that isn't prefixed.
  // We search for "\n  ... begin()" patterns without a field prefix.
  // More precisely: unprefixed begin/end/size should NOT appear
  auto has_unprefixed = [&](const std::string& method) {
    // Look for the method not preceded by a field name + underscore
    std::string pat = " " + method + "(";
    auto pos = result.find(pat);
    while (pos != std::string::npos) {
      // Check if preceded by _ (prefixed) or not
      if (pos > 0 && result[pos - 1] != '_' && result[pos - 1] != ':') {
        // Check it's not items_begin or tags_begin
        bool is_prefixed = false;
        for (const auto& f : cls.fields) {
          std::string prefix = f.name + "_" + method;
          if (pos >= f.name.size() + 1) {
            auto start = pos - f.name.size() - 1;
            if (result.substr(start + 1, f.name.size() + 1 + method.size()) ==
                prefix)
              is_prefixed = true;
          }
        }
        if (!is_prefixed) return true;
      }
      pos = result.find(pat, pos + 1);
    }
    return false;
  };
  CHECK_FALSE(has_unprefixed("begin"));
  CHECK_FALSE(has_unprefixed("end"));
  CHECK_FALSE(has_unprefixed("size"));
}

TEST_CASE("blank line after each function prototype and definition",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";

  cpp_function f1;
  f1.return_type = "int";
  f1.name = "foo";
  f1.parameters = "";
  f1.body = "  return 42;\n";
  f1.is_inline = true;

  cpp_function f2;
  f2.return_type = "void";
  f2.name = "bar";
  f2.parameters = "int x";
  f2.body = "  (void)x;\n";
  f2.is_inline = true;

  file.namespaces.push_back({"ns", {f1, f2}});

  auto result = writer.write(file);
  // Each function definition should be followed by a blank line
  CHECK(result.find("return 42;\n}\n\n") != std::string::npos);
  CHECK(result.find("(void)x;\n}\n\n") != std::string::npos);
}

TEST_CASE("blank line after each class method in source mode", "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  cpp_class cls;
  cls.name = "item";
  cls.raw_struct_name = "item_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::string", "name", ""},
      {"int32_t", "count", ""},
  };
  file.namespaces.push_back({"ns", {cls}});

  auto source = writer.write(file, {file_kind::source});
  // Each method definition ends with }\n\n (blank line after)
  // Constructor
  CHECK(source.find("data_(std::move(d)) {}\n\n") != std::string::npos);
  // Getter and setter should each be followed by blank line
  CHECK(source.find("return data_.name; }\n\n") != std::string::npos);
  CHECK(source.find("std::move(value); }\n\n") != std::string::npos);
}

TEST_CASE("forward declaration with is_class emits class keyword",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cpp_forward_decl{"order", true}}});

  auto result = writer.write(file);
  auto expected = R"(#pragma once

namespace ns {

class order;

} // namespace ns
)";
  CHECK(result == expected);
}

TEST_CASE("forward declaration without is_class emits struct keyword",
          "[cpp_writer]") {
  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cpp_forward_decl{"order", false}}});

  auto result = writer.write(file);
  CHECK(result.find("struct order;") != std::string::npos);
}

TEST_CASE("sequence insert takes by value for move-only types",
          "[cpp_writer]") {
  // Inline class with a vector of strings — insert should take by value
  // (not const ref) so move-only element types work.
  cpp_class cls;
  cls.name = "container";
  cls.raw_struct_name = "container_data";
  cls.inline_methods = true;
  cls.fields = {
      {"std::vector<std::string>", "items", ""},
  };

  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cls}});

  auto result = writer.write(file);
  // insert should take by value, not const ref
  CHECK(result.find("std::string value)") != std::string::npos);
  CHECK(result.find("const std::string& value)") == std::string::npos);
  // Should use std::move
  CHECK(result.find("std::move(value)") != std::string::npos);
}

TEST_CASE("sequence insert source takes by value", "[cpp_writer]") {
  cpp_class cls;
  cls.name = "container";
  cls.raw_struct_name = "container_data";
  cls.inline_methods = false;
  cls.fields = {
      {"std::vector<std::string>", "items", ""},
  };

  cpp_file file;
  file.filename = "test.hpp";
  file.namespaces.push_back({"ns", {cls}});

  auto source = writer.write(file, {file_kind::source});
  // Out-of-line insert should also take by value
  CHECK(source.find("std::string value)") != std::string::npos);
  CHECK(source.find("const std::string& value)") == std::string::npos);
  CHECK(source.find("std::move(value)") != std::string::npos);
}
