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
  file.namespaces.push_back({"ns", {cpp_struct{"foo_bar", {}, false}}});

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
  std::string id;
  int quantity;
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
  int x;
  int y;

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
  CHECK(result.find("std::string id;") != std::string::npos);
  CHECK(result.find("order_status status;") != std::string::npos);
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
  CHECK(result.find("std::optional<std::string> header;") != std::string::npos);
  CHECK(result.find("std::vector<int> items;") != std::string::npos);
  CHECK(result.find("std::variant<int, std::string> payload;") !=
        std::string::npos);
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
