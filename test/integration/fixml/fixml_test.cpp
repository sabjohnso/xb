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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

using namespace xb;

namespace fs = std::filesystem;

static const std::string fixml_ns = "http://www.fixprotocol.org/FIXML-5-0-SP2";

// Recursively parse a schema file and all its xs:include'd files.
// Tracks already-parsed files to avoid duplicates.
static void
parse_schema_recursive(const fs::path& path, schema_set& ss,
                       std::set<fs::path>& parsed) {
  auto canonical = fs::canonical(path);
  if (parsed.count(canonical)) return;
  parsed.insert(canonical);

  std::ifstream file(canonical);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  auto s = parser.parse(reader);

  // Follow includes before adding this schema
  auto parent_dir = canonical.parent_path();
  for (const auto& inc : s.includes()) {
    parse_schema_recursive(parent_dir / inc.schema_location, ss, parsed);
  }

  ss.add(std::move(s));
}

static schema_set
parse_fixml_full() {
  schema_set ss;
  std::set<fs::path> parsed;
  fs::path main_xsd =
      fs::path(STRINGIFY(XB_FIXML_SCHEMA_DIR)) / "fixml-main-5-0-SP2.xsd";
  parse_schema_recursive(main_xsd, ss, parsed);
  ss.resolve();
  return ss;
}

// Merge cpp_file objects with the same filename.
// All FIXML schemas share one namespace, so codegen produces multiple
// cpp_file objects with the same filename. Merge their namespaces and
// includes so each filename appears once.
static std::vector<cpp_file>
merge_files(const std::vector<cpp_file>& files) {
  // Preserve order of first occurrence
  std::vector<std::string> order;
  std::map<std::string, cpp_file> by_name;

  for (const auto& f : files) {
    std::string key = f.filename + (f.kind == file_kind::header ? ":h" : ":s");
    auto it = by_name.find(key);
    if (it == by_name.end()) {
      order.push_back(key);
      by_name[key] = f;
    } else {
      // Merge includes (deduplicated)
      std::set<std::string> existing;
      for (const auto& inc : it->second.includes)
        existing.insert(inc.path);
      for (const auto& inc : f.includes) {
        if (!existing.count(inc.path)) {
          it->second.includes.push_back(inc);
          existing.insert(inc.path);
        }
      }
      // Merge namespaces: if same namespace name, append declarations
      for (const auto& ns : f.namespaces) {
        bool found = false;
        for (auto& existing_ns : it->second.namespaces) {
          if (existing_ns.name == ns.name) {
            for (const auto& decl : ns.declarations)
              existing_ns.declarations.push_back(decl);
            found = true;
            break;
          }
        }
        if (!found) it->second.namespaces.push_back(ns);
      }
    }
  }

  std::vector<cpp_file> result;
  for (const auto& key : order)
    result.push_back(std::move(by_name[key]));
  return result;
}

// ===== Phase A: Schema parsing =====

TEST_CASE("FIXML schema files all parse without errors",
          "[fixml][integration][parse]") {
  schema_set ss;
  std::set<fs::path> parsed;
  fs::path schema_dir = STRINGIFY(XB_FIXML_SCHEMA_DIR);
  fs::path main_xsd = schema_dir / "fixml-main-5-0-SP2.xsd";

  REQUIRE(fs::exists(main_xsd));

  REQUIRE_NOTHROW(parse_schema_recursive(main_xsd, ss, parsed));

  // 54 of 55 files are reached via xs:include; fixml-metadata-5-0-SP2.xsd
  // is in a different namespace and referenced via xsi:schemaLocation only.
  CHECK(parsed.size() == 54);
}

TEST_CASE("FIXML schema resolves without errors",
          "[fixml][integration][resolve]") {
  auto ss = parse_fixml_full();

  // Verify key structural types exist
  SECTION("Abstract_message_t exists") {
    auto* msg = ss.find_complex_type({fixml_ns, "Abstract_message_t"});
    REQUIRE(msg != nullptr);
  }

  SECTION("Message element exists and is abstract") {
    auto* msg_elem = ss.find_element({fixml_ns, "Message"});
    REQUIRE(msg_elem != nullptr);
    CHECK(msg_elem->abstract());
  }

  SECTION("FIXML root element exists") {
    auto* fixml = ss.find_element({fixml_ns, "FIXML"});
    REQUIRE(fixml != nullptr);
  }

  SECTION("Custom simple types exist") {
    auto* qty = ss.find_simple_type({fixml_ns, "Qty"});
    REQUIRE(qty != nullptr);

    auto* price = ss.find_simple_type({fixml_ns, "Price"});
    REQUIRE(price != nullptr);

    auto* boolean = ss.find_simple_type({fixml_ns, "Boolean"});
    REQUIRE(boolean != nullptr);
  }

  SECTION("Component types exist") {
    auto* instrument = ss.find_complex_type({fixml_ns, "Instrument_Block_t"});
    REQUIRE(instrument != nullptr);

    auto* parties = ss.find_complex_type({fixml_ns, "Parties_Block_t"});
    REQUIRE(parties != nullptr);
  }

  SECTION("Named model groups exist") {
    auto* exec_elems =
        ss.find_model_group_def({fixml_ns, "ExecutionReportElements"});
    REQUIRE(exec_elems != nullptr);

    auto* base_header =
        ss.find_model_group_def({fixml_ns, "BaseHeaderElements"});
    REQUIRE(base_header != nullptr);
  }

  SECTION("Named attribute groups exist") {
    auto* exec_attrs =
        ss.find_attribute_group_def({fixml_ns, "ExecutionReportAttributes"});
    REQUIRE(exec_attrs != nullptr);

    auto* fixml_attrs =
        ss.find_attribute_group_def({fixml_ns, "FixmlAttributes"});
    REQUIRE(fixml_attrs != nullptr);
  }
}

// ===== Phase B/C: Code generation and compilation =====

static bool
compile_generated_files(const std::vector<cpp_file>& raw_files,
                        const std::string& test_name) {
  auto merged = merge_files(raw_files);
  auto tmp_dir = fs::temp_directory_path() / ("xb_fixml_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;

  for (const auto& file : merged) {
    auto path = tmp_dir / file.filename;
    std::ofstream out(path);
    out << writer.write(file);
  }

  // Write a main.cpp that includes only header files
  auto main_path = tmp_dir / "main.cpp";
  {
    std::ofstream out(main_path);
    out << "#if defined(__GNUC__) && !defined(__clang__)\n";
    out << "#pragma GCC diagnostic push\n";
    out << "#pragma GCC diagnostic ignored \"-Wmaybe-uninitialized\"\n";
    out << "#endif\n\n";
    for (const auto& file : merged) {
      if (file.kind == file_kind::header)
        out << "#include \"" << file.filename << "\"\n";
    }
    out << "int main() { return 0; }\n";
  }

  // Collect source files for compilation
  std::string source_files;
  for (const auto& file : merged) {
    if (file.kind == file_kind::source) {
      source_files += ' ';
      source_files += (tmp_dir / file.filename).string();
    }
  }

  std::string include_dir = STRINGIFY(XB_INCLUDE_DIR);
  std::string lib_file = STRINGIFY(XB_LIB_FILE);

  std::string sanitizer_flags;
  if (XB_SANITIZERS)
    sanitizer_flags = "-fsanitize=undefined -fsanitize=address ";

  std::string cmd;
  if (source_files.empty()) {
    cmd = "c++ -std=c++20 -fsyntax-only -I" + tmp_dir.string() + " -I" +
          include_dir + " " + main_path.string() + " 2>&1";
  } else {
    auto exe_path = tmp_dir / "test_exe";
    cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" + tmp_dir.string() +
          " -I" + include_dir + " -o " + exe_path.string() + " " +
          main_path.string() + source_files + " " + lib_file + " -lexpat 2>&1";
  }
  int rc = std::system(cmd.c_str());

  fs::remove_all(tmp_dir);

  return rc == 0;
}

// Build generated code + test code into an executable and run it
static bool
build_and_run(const std::vector<cpp_file>& raw_files,
              const std::string& test_name, const std::string& test_code) {
  auto merged = merge_files(raw_files);
  auto tmp_dir = fs::temp_directory_path() / ("xb_fixml_rt_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;

  for (const auto& file : merged) {
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
    for (const auto& file : merged) {
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

  // Collect generated source files
  std::string source_files;
  for (const auto& file : merged) {
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

  std::string cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" +
                    tmp_dir.string() + " -I" + include_dir + " -o " +
                    exe_path.string() + " " + main_path.string() +
                    source_files + " " + lib_file + " -lexpat 2>&1";
  int rc = std::system(cmd.c_str());

  if (rc != 0) {
    std::cerr << "Build failed for " << test_name << "\n";
    std::cerr << "Command: " << cmd << "\n";
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

static std::vector<cpp_file>
generate_fixml() {
  auto ss = parse_fixml_full();

  codegen_options opts;
  opts.namespace_map[fixml_ns] = "fixml";
  opts.mode = output_mode::split;

  auto types = type_map::defaults();
  codegen gen(ss, types, opts);
  return gen.generate();
}

TEST_CASE("FIXML full schema generates code", "[fixml][integration][codegen]") {
  auto files = generate_fixml();

  REQUIRE(!files.empty());

  bool has_header = false;
  bool has_source = false;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) has_header = true;
    if (f.kind == file_kind::source) has_source = true;
  }
  CHECK(has_header);
  CHECK(has_source);
}

TEST_CASE("FIXML generated code compiles", "[fixml][integration][compile]") {
  auto files = generate_fixml();

  REQUIRE(!files.empty());
  CHECK(compile_generated_files(files, "full_schema"));
}

// ===== Phase D: Round-trip serialization =====

TEST_CASE("FIXML round-trip: simple HopGrp_Block_t",
          "[fixml][integration][roundtrip]") {
  auto files = generate_fixml();
  REQUIRE(!files.empty());

  std::string test_code = R"(
int main() {
  using namespace fixml;

  // Construct a HopGrp_Block_t (simple type with 3 optional attributes)
  hop_grp_block_t val;
  val.id = "COMP1";
  val.ref = xb::integer("42");

  // Serialize
  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://www.fixprotocol.org/FIXML-5-0-SP2", "Hop"});
    writer.namespace_declaration("", "http://www.fixprotocol.org/FIXML-5-0-SP2");
    write_hop_grp_block_t(val, writer);
    writer.end_element();
  }

  std::cerr << "Serialized: " << os.str() << std::endl;

  // Deserialize
  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_hop_grp_block_t(reader);

  // Compare
  assert(result == val);

  std::cerr << "HopGrp round-trip OK" << std::endl;
  return 0;
}
)";

  CHECK(build_and_run(files, "hopgrp_roundtrip", test_code));
}
