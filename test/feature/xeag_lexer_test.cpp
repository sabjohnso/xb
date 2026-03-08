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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// Write generated files + test main to a temp directory, compile, link, and run
static bool
build_and_run(const std::vector<cpp_file>& files, const std::string& test_name,
              const std::string& test_code) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_xeag_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;

  for (const auto& file : files) {
    auto path = tmp_dir / file.filename;
    std::ofstream out(path);
    out << writer.write(file);
  }

  // Write test main.cpp
  auto main_path = tmp_dir / "main.cpp";
  {
    std::ofstream out(main_path);
    out << "#if defined(__GNUC__) && !defined(__clang__)\n";
    out << "#pragma GCC diagnostic push\n";
    out << "#pragma GCC diagnostic ignored \"-Wmaybe-uninitialized\"\n";
    out << "#endif\n\n";
    for (const auto& file : files) {
      if (file.kind == file_kind::header)
        out << "#include \"" << file.filename << "\"\n";
    }
    out << "\n";
    out << "#include <xb/ostream_writer.hpp>\n";
    out << "#include <xb/expat_reader.hpp>\n";
    out << "#include <sstream>\n";
    out << "#include <cassert>\n";
    out << "#include <iostream>\n";
    out << "\n";
    out << test_code;
  }

  // Collect generated source files for compilation
  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      source_files += ' ';
      source_files += (tmp_dir / file.filename).string();
    }
  }

  std::string include_dir = STRINGIFY(XB_INCLUDE_DIR);
  std::string lib_file = STRINGIFY(XB_LIB_FILE);
  auto exe_path = tmp_dir / "test_exe";

  std::string sanitizer_flags;
  if (XB_SANITIZERS)
    sanitizer_flags = "-fsanitize=undefined -fsanitize=address ";

  // Compile and link
  std::string cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" +
                    tmp_dir.string() + " -I" + include_dir + " -o " +
                    exe_path.string() + " " + main_path.string() +
                    source_files + " " + lib_file + " -lexpat 2>&1";
  int rc = std::system(cmd.c_str());

  if (rc != 0) {
    std::cerr << "Build failed for " << test_name << "\n";
    std::cerr << "Command: " << cmd << "\n";
    // Print generated files for debugging
    for (const auto& file : files) {
      if (file.kind == file_kind::header) {
        auto path = tmp_dir / file.filename;
        std::ifstream hdr(path);
        std::cerr << "=== " << file.filename << " ===\n";
        std::cerr << hdr.rdbuf() << "\n";
      }
    }
    fs::remove_all(tmp_dir);
    return false;
  }

  rc = std::system(exe_path.string().c_str());
  if (rc != 0) {
    std::cerr << "Run failed for " << test_name << " (exit code " << rc
              << ")\n";
  }

  fs::remove_all(tmp_dir);
  return rc == 0;
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

TEST_CASE("xeag-lexer: codegen produces header and source in split mode",
          "[xeag-lexer][codegen]") {
  auto schemas = parse_xeag_lexer_schema();
  codegen_options opts;
  opts.mode = output_mode::split;
  auto files = generate_xeag_lexer(schemas, opts);

  REQUIRE(files.size() == 2);
  bool has_header = false;
  bool has_source = false;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) has_header = true;
    if (f.kind == file_kind::source) has_source = true;
  }
  CHECK(has_header);
  CHECK(has_source);
}

TEST_CASE("xeag-lexer: mutually recursive types use unique_ptr",
          "[xeag-lexer][codegen][cycle]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  // Find the header
  const cpp_file* header = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) {
      header = &f;
      break;
    }
  }
  REQUIRE(header != nullptr);

  // Write to string and check for unique_ptr usage on recursive types
  cpp_writer writer;
  auto code = writer.write(*header);

  // pattern_group_type and pattern_choice_type are mutually recursive.
  // The variant in pattern_group_type should wrap the recursive alternatives
  // with std::unique_ptr to break the cycle.
  INFO("Generated header:\n" << code);
  CHECK(code.find("unique_ptr") != std::string::npos);
}

TEST_CASE("xeag-lexer: forward declarations emitted for cycle types",
          "[xeag-lexer][codegen][cycle]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  const cpp_file* header = nullptr;
  for (const auto& f : files)
    if (f.kind == file_kind::header) header = &f;
  REQUIRE(header != nullptr);

  cpp_writer writer;
  auto code = writer.write(*header);

  INFO("Generated header:\n" << code);

  // Forward declarations should appear for the recursive types
  CHECK(code.find("struct pattern_group_type;") != std::string::npos);
  CHECK(code.find("struct pattern_choice_type;") != std::string::npos);
}

// =========================================================================
// Compilation tests
// =========================================================================

TEST_CASE("xeag-lexer: generated code compiles", "[xeag-lexer][compile]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  auto test_code = R"(
int main() {
  using namespace xeag::org::lexer;

  // Verify basic type construction
  lexer_type lex;
  match_type m;
  skip_type sk;
  pattern_literal_type lit;
  char_class_type cc;
  pattern_choice_type choice;
  pattern_group_type group;

  // Verify enum
  auto r = repetition_type::zero_or_more;
  (void)r;

  std::cout << "Compilation OK\n";
  return 0;
}
)";
  REQUIRE(build_and_run(files, "compile", test_code));
}

TEST_CASE("xeag-lexer: nested recursive construction compiles",
          "[xeag-lexer][compile]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  // Build a nested structure: group containing a choice containing groups
  auto test_code = R"(
int main() {
  using namespace xeag::org::lexer;

  // Build: group { choice { option { literal "a" }, option { literal "b" } } }
  pattern_literal_type lit_a;
  lit_a.value = "a";

  pattern_literal_type lit_b;
  lit_b.value = "b";

  pattern_group_type opt_a;
  opt_a.choice.push_back(lit_a);

  pattern_group_type opt_b;
  opt_b.choice.push_back(lit_b);

  pattern_choice_type choice;
  choice.option.push_back(std::make_unique<pattern_group_type>(std::move(opt_a)));
  choice.option.push_back(std::make_unique<pattern_group_type>(std::move(opt_b)));

  pattern_group_type outer;
  outer.choice.push_back(std::make_unique<pattern_choice_type>(std::move(choice)));
  outer.repetition = repetition_type::zero_or_more;

  // Build a complete lexer with this pattern
  match_type m;
  m.name = "TOKEN";
  m.choice.push_back(std::make_unique<pattern_group_type>(std::move(outer)));

  lexer_type lex;
  lex.match.push_back(std::move(m));

  std::cout << "Nested construction OK\n";
  return 0;
}
)";
  REQUIRE(build_and_run(files, "nested_construct", test_code));
}

// =========================================================================
// Round-trip serialization tests
// =========================================================================

TEST_CASE("xeag-lexer: round-trip simple lexer", "[xeag-lexer][round-trip]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  auto test_code = R"TEST(
int main() {
  using namespace xeag::org::lexer;

  // Parse a simple lexer document
  std::string xml = R"XML(<?xml version="1.0"?>
<lexer xmlns="http://xeag.org/lexer">
  <skip chars=" &#10;&#9;" repetition="one-or-more"/>
  <match name="PLUS" value="+"/>
  <match name="MINUS" value="-"/>
  <match name="NUMBER" chars="0-9" repetition="one-or-more"/>
</lexer>
)XML";

  // Read
  xb::expat_reader reader(xml);
  reader.read(); // advance to root element
  auto lex = read_lexer_type(reader);

  assert(lex.skip.size() == 1);
  assert(lex.match.size() == 3);
  assert(lex.match[0].name == "PLUS");
  assert(lex.match[0].value.has_value());
  assert(*lex.match[0].value == "+");
  assert(lex.match[2].name == "NUMBER");
  assert(lex.match[2].chars.has_value());
  assert(*lex.match[2].chars == "0-9");
  assert(lex.match[2].repetition.has_value());
  assert(*lex.match[2].repetition == repetition_type::one_or_more);

  // Write
  std::ostringstream oss;
  xb::ostream_writer writer(oss);
  
  writer.start_element(xb::qname{"http://xeag.org/lexer", "lexer"});
  writer.namespace_declaration("", "http://xeag.org/lexer");
  write_lexer_type(lex, writer);
  writer.end_element();
  

  std::string output = oss.str();

  // Re-read the written output
  xb::expat_reader reader2(output);
  reader2.read();
  auto lex2 = read_lexer_type(reader2);

  // Verify round-trip
  assert(lex2.skip.size() == lex.skip.size());
  assert(lex2.match.size() == lex.match.size());
  assert(lex2.match[0].name == lex.match[0].name);
  assert(lex2.match[0].value == lex.match[0].value);
  assert(lex2.match[2].name == lex.match[2].name);
  assert(lex2.match[2].chars == lex.match[2].chars);
  assert(lex2.match[2].repetition == lex.match[2].repetition);

  std::cout << "Simple round-trip OK\n";
  return 0;
}
)TEST";
  REQUIRE(build_and_run(files, "simple_roundtrip", test_code));
}

TEST_CASE("xeag-lexer: round-trip complex pattern with nested groups",
          "[xeag-lexer][round-trip]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  // This tests the mutual recursion path: group -> choice -> group -> literal
  auto test_code = R"TEST(
int main() {
  using namespace xeag::org::lexer;

  // A lexer for identifiers: [a-zA-Z_][a-zA-Z0-9_]*
  // and multi-line comments: /* ... */
  std::string xml = R"XML(<?xml version="1.0"?>
<lexer xmlns="http://xeag.org/lexer">
  <skip chars=" &#10;&#9;&#13;" repetition="one-or-more"/>
  <skip>
    <literal value="/*"/>
    <char-class chars="*" complement="true" repetition="zero-or-more"/>
    <literal value="*/"/>
  </skip>
  <match name="IDENT">
    <char-class chars="a-zA-Z_"/>
    <char-class chars="a-zA-Z0-9_" repetition="zero-or-more"/>
  </match>
  <match name="NUMBER">
    <choice>
      <option>
        <char-class chars="0-9" repetition="one-or-more"/>
        <group repetition="optional">
          <literal value="."/>
          <char-class chars="0-9" repetition="one-or-more"/>
        </group>
      </option>
      <option>
        <literal value="."/>
        <char-class chars="0-9" repetition="one-or-more"/>
      </option>
    </choice>
  </match>
  <match name="EQUALS" value="="/>
  <match name="SEMI" value=";"/>
</lexer>
)XML";

  // Read
  xb::expat_reader reader(xml);
  reader.read();
  auto lex = read_lexer_type(reader);

  assert(lex.skip.size() == 2);
  assert(lex.match.size() == 4);

  // Check the comment skip pattern (complex pattern with child elements)
  auto& comment_skip = lex.skip[1];
  assert(comment_skip.choice.size() == 3); // literal, char-class, literal

  // Check IDENT match (two char-class elements)
  assert(lex.match[0].name == "IDENT");
  assert(lex.match[0].choice.size() == 2);

  // Check NUMBER match has a choice with two options
  assert(lex.match[1].name == "NUMBER");
  assert(lex.match[1].choice.size() == 1); // one choice element

  // Write and re-read
  std::ostringstream oss;
  xb::ostream_writer writer(oss);
  
  writer.start_element(xb::qname{"http://xeag.org/lexer", "lexer"});
  writer.namespace_declaration("", "http://xeag.org/lexer");
  write_lexer_type(lex, writer);
  writer.end_element();
  

  std::string output = oss.str();

  xb::expat_reader reader2(output);
  reader2.read();
  auto lex2 = read_lexer_type(reader2);

  // Verify structure preserved
  assert(lex2.skip.size() == 2);
  assert(lex2.match.size() == 4);
  assert(lex2.match[0].name == "IDENT");
  assert(lex2.match[1].name == "NUMBER");
  assert(lex2.match[0].choice.size() == 2);
  assert(lex2.match[1].choice.size() == 1);
  assert(lex2.skip[1].choice.size() == 3);

  std::cout << "Complex pattern round-trip OK\n";
  return 0;
}
)TEST";
  REQUIRE(build_and_run(files, "complex_roundtrip", test_code));
}

TEST_CASE("xeag-lexer: round-trip deeply nested groups",
          "[xeag-lexer][round-trip]") {
  auto schemas = parse_xeag_lexer_schema();
  auto files = generate_xeag_lexer(schemas);

  // Exercise deep nesting: group { group { choice { option { group { ... }}}}
  auto test_code = R"TEST(
int main() {
  using namespace xeag::org::lexer;

  std::string xml = R"XML(<?xml version="1.0"?>
<lexer xmlns="http://xeag.org/lexer">
  <match name="NESTED">
    <group>
      <literal value="("/>
      <group repetition="zero-or-more">
        <choice>
          <option>
            <char-class chars="a-z"/>
          </option>
          <option>
            <group>
              <literal value="["/>
              <char-class chars="0-9" repetition="one-or-more"/>
              <literal value="]"/>
            </group>
          </option>
        </choice>
      </group>
      <literal value=")"/>
    </group>
  </match>
</lexer>
)XML";

  xb::expat_reader reader(xml);
  reader.read();
  auto lex = read_lexer_type(reader);

  assert(lex.match.size() == 1);
  assert(lex.match[0].name == "NESTED");

  // Write and re-read
  std::ostringstream oss;
  xb::ostream_writer writer(oss);
  
  writer.start_element(xb::qname{"http://xeag.org/lexer", "lexer"});
  writer.namespace_declaration("", "http://xeag.org/lexer");
  write_lexer_type(lex, writer);
  writer.end_element();
  

  xb::expat_reader reader2(oss.str());
  reader2.read();
  auto lex2 = read_lexer_type(reader2);

  assert(lex2.match.size() == 1);
  assert(lex2.match[0].name == "NESTED");
  // The outer group contains 3 elements: literal, group, literal
  assert(lex2.match[0].choice.size() == 1); // one outer group

  std::cout << "Deeply nested round-trip OK\n";
  return 0;
}
)TEST";
  REQUIRE(build_and_run(files, "deep_nested_roundtrip", test_code));
}

// =========================================================================
// Wrapped mode tests
// =========================================================================

TEST_CASE("xeag-lexer: wrapped mode generates and compiles",
          "[xeag-lexer][compile][wrapped][!mayfail]") {
  auto schemas = parse_xeag_lexer_schema();
  codegen_options opts;
  opts.encapsulation = encapsulation_mode::wrapped;
  auto files = generate_xeag_lexer(schemas, opts);

  auto test_code = R"(
int main() {
  using namespace xeag::org::lexer;

  lexer_type lex;
  match_type m;
  m.set_name("TOKEN");
  m.set_value("+");
  lex.match_push_back(std::move(m));

  assert(lex.match_size() == 1);
  assert(lex.match(0).name() == "TOKEN");

  std::cout << "Wrapped mode OK\n";
  return 0;
}
)";
  REQUIRE(build_and_run(files, "wrapped_compile", test_code));
}

// =========================================================================
// Naming style tests
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

// =========================================================================
// Header-only mode tests
// =========================================================================

TEST_CASE("xeag-lexer: header-only mode compiles",
          "[xeag-lexer][compile][header-only]") {
  auto schemas = parse_xeag_lexer_schema();
  codegen_options opts;
  opts.mode = output_mode::header_only;
  auto files = generate_xeag_lexer(schemas, opts);

  // Should produce a single header file
  REQUIRE(files.size() == 1);
  CHECK(files[0].kind == file_kind::header);

  auto test_code = R"(
int main() {
  using namespace xeag::org::lexer;
  lexer_type lex;
  match_type m;
  m.name = "X";
  m.value = "x";
  lex.match.push_back(std::move(m));
  std::cout << "Header-only OK\n";
  return 0;
}
)";
  REQUIRE(build_and_run(files, "header_only", test_code));
}
