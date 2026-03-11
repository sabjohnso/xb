// GCC 12 emits a false -Wmaybe-uninitialized for std::variant containing
// std::unique_ptr in particle::term_type at -O3. Suppress it here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace xb;

namespace fs = std::filesystem;

static schema_set
parse_xeag_lexer_schema() {
  std::string schema_dir = STRINGIFY(XB_SCHEMA_DIR);
  auto path = fs::path(schema_dir) / "xeag-lexer.xsd";
  std::ifstream in(path);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  auto content = ss.str();

  schema_set schemas;
  expat_reader reader(content);
  schema_parser parser;
  schemas.add(parser.parse(reader));
  schemas.resolve();
  return schemas;
}

static std::vector<cpp_file>
generate_xeag_lexer(const schema_set& schemas, codegen_options opts = {}) {
  auto types = type_map::defaults();
  codegen gen(schemas, types, opts);
  return gen.generate();
}

static codegen_options
wrapped_file_per_type_opts() {
  codegen_options opts;
  opts.encapsulation = encapsulation_mode::wrapped;
  opts.mode = output_mode::file_per_type;
  return opts;
}

// =========================================================================
// Schema parsing tests
// =========================================================================

TEST_CASE("xeag-lexer: schema parses successfully", "[xeag-lexer][schema]") {
  auto schemas = parse_xeag_lexer_schema();
  // Should have one schema
  REQUIRE(schemas.schemas().size() == 1);
  // Should find the complex types
  auto ns = "http://xeag.org/lexer";
  CHECK(schemas.find_complex_type(qname{ns, "Lexer-Type"}) != nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Match-Type"}) != nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Skip-Type"}) != nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Pattern-Literal-Type"}) !=
        nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Char-Class-Type"}) != nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Pattern-Choice-Type"}) != nullptr);
  CHECK(schemas.find_complex_type(qname{ns, "Pattern-Group-Type"}) != nullptr);
}

// =========================================================================
// Code generation IR tests
// =========================================================================

TEST_CASE("xeag-lexer: codegen produces multiple files in file-per-type mode",
          "[xeag-lexer][codegen]") {
  auto schemas = parse_xeag_lexer_schema();
  auto opts = wrapped_file_per_type_opts();
  auto files = generate_xeag_lexer(schemas, opts);

  int headers = 0;
  int sources = 0;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) ++headers;
    if (f.kind == file_kind::source) ++sources;
  }
  CHECK(headers > 1);
  CHECK(sources > 0);
}

TEST_CASE("xeag-lexer: mutually recursive types use unique_ptr",
          "[xeag-lexer][codegen][cycle]") {
  auto schemas = parse_xeag_lexer_schema();
  auto opts = wrapped_file_per_type_opts();
  auto files = generate_xeag_lexer(schemas, opts);

  cpp_writer writer;
  std::string all_code;
  for (const auto& f : files)
    if (f.kind == file_kind::header) all_code += writer.write(f);

  INFO("Generated headers:\n" << all_code);
  CHECK(all_code.find("unique_ptr") != std::string::npos);
}

TEST_CASE("xeag-lexer: forward declarations use class for wrapped types",
          "[xeag-lexer][codegen][cycle]") {
  auto schemas = parse_xeag_lexer_schema();
  auto opts = wrapped_file_per_type_opts();
  auto files = generate_xeag_lexer(schemas, opts);

  cpp_writer writer;
  std::string all_code;
  for (const auto& f : files)
    if (f.kind == file_kind::header) all_code += writer.write(f);

  INFO("Generated headers:\n" << all_code);

  // Forward declarations should use 'class' since types are wrapped
  CHECK(all_code.find("class pattern_group_type;") != std::string::npos);
  CHECK(all_code.find("class pattern_choice_type;") != std::string::npos);
  // Should NOT use 'struct' for forward-declared wrapped types
  CHECK(all_code.find("struct pattern_group_type;") == std::string::npos);
  CHECK(all_code.find("struct pattern_choice_type;") == std::string::npos);
}

// =========================================================================
// Naming style IR tests
// =========================================================================

TEST_CASE("xeag-lexer: pascal naming style", "[xeag-lexer][codegen][naming]") {
  auto schemas = parse_xeag_lexer_schema();
  codegen_options opts;
  opts.naming.type_style = naming_style::pascal_case;
  auto files = generate_xeag_lexer(schemas, opts);

  const cpp_file* header = nullptr;
  for (const auto& f : files)
    if (f.kind == file_kind::header) header = &f;
  REQUIRE(header != nullptr);

  cpp_writer writer;
  auto code = writer.write(*header);

  INFO("Generated header:\n" << code);
  CHECK(code.find("struct LexerType") != std::string::npos);
  CHECK(code.find("struct MatchType") != std::string::npos);
  CHECK(code.find("struct PatternGroupType") != std::string::npos);
  CHECK(code.find("struct PatternChoiceType") != std::string::npos);
}

TEST_CASE("xeag-lexer: header-only mode produces single header",
          "[xeag-lexer][codegen]") {
  auto schemas = parse_xeag_lexer_schema();
  codegen_options opts;
  opts.mode = output_mode::header_only;
  auto files = generate_xeag_lexer(schemas, opts);

  REQUIRE(files.size() == 1);
  CHECK(files[0].kind == file_kind::header);
}
