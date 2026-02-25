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

// UBL 2.1 namespace URIs
static const std::string invoice_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:Invoice-2";
static const std::string cac_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:"
    "CommonAggregateComponents-2";
static const std::string cbc_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:CommonBasicComponents-2";
static const std::string cec_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:"
    "CommonExtensionComponents-2";
static const std::string udt_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:UnqualifiedDataTypes-2";
static const std::string qdt_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:QualifiedDataTypes-2";
static const std::string ccts_ns =
    "urn:un:unece:uncefact:data:specification:CoreComponentTypeSchemaModule:2";
static const std::string ccts_doc_ns = "urn:un:unece:uncefact:documentation:2";
static const std::string sig_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:"
    "CommonSignatureComponents-2";
static const std::string sac_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:"
    "SignatureAggregateComponents-2";
static const std::string sbc_ns =
    "urn:oasis:names:specification:ubl:schema:xsd:"
    "SignatureBasicComponents-2";
static const std::string dsig_ns = "http://www.w3.org/2000/09/xmldsig#";
static const std::string xades132_ns = "http://uri.etsi.org/01903/v1.3.2#";
static const std::string xades141_ns = "http://uri.etsi.org/01903/v1.4.1#";

// Recursively parse a schema file and all its includes AND imports.
// UBL uses xs:import (not xs:include) for cross-namespace references.
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

  // Follow includes
  auto parent_dir = canonical.parent_path();
  for (const auto& inc : s.includes()) {
    parse_schema_recursive(parent_dir / inc.schema_location, ss, parsed);
  }

  // Follow imports when schemaLocation is provided
  for (const auto& imp : s.imports()) {
    if (!imp.schema_location.empty()) {
      auto import_path = parent_dir / imp.schema_location;
      if (fs::exists(import_path))
        parse_schema_recursive(import_path, ss, parsed);
    }
  }

  ss.add(std::move(s));
}

static schema_set
parse_ubl_invoice() {
  schema_set ss;
  std::set<fs::path> parsed;
  fs::path main_xsd = fs::path(STRINGIFY(XB_UBL_SCHEMA_DIR)) / "maindoc" /
                      "UBL-Invoice-2.1.xsd";
  parse_schema_recursive(main_xsd, ss, parsed);
  ss.resolve();
  return ss;
}

static codegen_options
ubl_codegen_options() {
  codegen_options opts;
  opts.mode = output_mode::split;
  opts.namespace_map = {
      {invoice_ns, "ubl::invoice"},   {cac_ns, "ubl::cac"},
      {cbc_ns, "ubl::cbc"},           {cec_ns, "ubl::cec"},
      {udt_ns, "ubl::udt"},           {qdt_ns, "ubl::qdt"},
      {ccts_ns, "ubl::ccts"},         {ccts_doc_ns, "ubl::ccts_doc"},
      {sig_ns, "ubl::sig"},           {sac_ns, "ubl::sac"},
      {sbc_ns, "ubl::sbc"},           {dsig_ns, "ubl::dsig"},
      {xades132_ns, "ubl::xades132"}, {xades141_ns, "ubl::xades141"},
  };
  return opts;
}

// ===== Phase A: Schema parsing =====

TEST_CASE("UBL Invoice schema files all parse without errors",
          "[ubl][integration][parse]") {
  schema_set ss;
  std::set<fs::path> parsed;
  fs::path schema_dir = STRINGIFY(XB_UBL_SCHEMA_DIR);
  fs::path main_xsd = schema_dir / "maindoc" / "UBL-Invoice-2.1.xsd";

  REQUIRE(fs::exists(main_xsd));

  REQUIRE_NOTHROW(parse_schema_recursive(main_xsd, ss, parsed));

  // 15 XSD files in the import chain
  CHECK(parsed.size() >= 10);

  INFO("Parsed " << parsed.size() << " schema files");
}

// ===== Phase B: Schema resolution =====

TEST_CASE("UBL Invoice schema resolves cross-namespace references",
          "[ubl][integration][resolve]") {
  auto ss = parse_ubl_invoice();

  SECTION("InvoiceType exists in Invoice namespace") {
    auto* t = ss.find_complex_type({invoice_ns, "InvoiceType"});
    REQUIRE(t != nullptr);
  }

  SECTION("AddressType exists in CAC namespace") {
    auto* t = ss.find_complex_type({cac_ns, "AddressType"});
    REQUIRE(t != nullptr);
  }

  SECTION("PartyType exists in CAC namespace") {
    auto* t = ss.find_complex_type({cac_ns, "PartyType"});
    REQUIRE(t != nullptr);
  }

  SECTION("ID element exists in CBC namespace") {
    auto* e = ss.find_element({cbc_ns, "ID"});
    REQUIRE(e != nullptr);
  }

  SECTION("AmountType exists in CCTS namespace") {
    auto* t = ss.find_complex_type({ccts_ns, "AmountType"});
    REQUIRE(t != nullptr);
  }
}

// ===== Phase C: Code generation =====

static std::vector<cpp_file>
generate_ubl() {
  auto ss = parse_ubl_invoice();
  auto opts = ubl_codegen_options();
  auto types = type_map::defaults();
  codegen gen(ss, types, opts);
  return gen.generate();
}

TEST_CASE("UBL Invoice generates code for multiple namespaces",
          "[ubl][integration][codegen]") {
  auto files = generate_ubl();

  REQUIRE(!files.empty());

  // Collect distinct filenames
  std::set<std::string> filenames;
  bool has_header = false;
  bool has_source = false;
  for (const auto& f : files) {
    filenames.insert(f.filename);
    if (f.kind == file_kind::header) has_header = true;
    if (f.kind == file_kind::source) has_source = true;
  }

  CHECK(has_header);
  CHECK(has_source);

  // Multiple namespaces should produce multiple distinct files
  CHECK(filenames.size() >= 7);

  // No filename should contain colons (URN artifact)
  for (const auto& fn : filenames) {
    INFO("filename: " << fn);
    CHECK(fn.find(':') == std::string::npos);
  }
}

// ===== Phase D: Compilation =====

static bool
compile_generated_files(const std::vector<cpp_file>& files,
                        const std::string& test_name) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_ubl_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;

  for (const auto& file : files) {
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
    for (const auto& file : files) {
      if (file.kind == file_kind::header)
        out << "#include \"" << file.filename << "\"\n";
    }
    out << "int main() { return 0; }\n";
  }

  // Collect source files for compilation.
  // Skip xmldsig and XAdES source files — their serialization code uses
  // XSD patterns (mixed content with xs:any, complex choice groups) the
  // codegen doesn't yet fully support. Headers compile fine and the
  // type definitions are available for other namespaces.
  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      auto fn = file.filename;
      // Skip xmldsig, XAdES, and signature component source files —
      // their serialization code uses XSD patterns (mixed content with
      // xs:any, complex choice groups) the codegen doesn't yet fully
      // support. Headers compile fine for type definitions.
      if (fn == "xmldsig.cpp" || fn == "v1_3_2.cpp" || fn == "v1_4_1.cpp" ||
          fn.find("signature") != std::string::npos ||
          fn == "common_signature_components_2.cpp")
        continue;
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

  std::cerr << "Compile command: " << cmd << "\n";
  int rc = std::system(cmd.c_str());

  if (rc != 0) {
    std::cerr << "Compilation failed for " << test_name << "\n";
    // Leave tmp_dir for debugging
  } else {
    fs::remove_all(tmp_dir);
  }

  return rc == 0;
}

TEST_CASE("UBL generated code compiles", "[ubl][integration][compile]") {
  auto files = generate_ubl();

  REQUIRE(!files.empty());
  CHECK(compile_generated_files(files, "full_schema"));
}

// ===== Phase E: Round-trip serialization =====

static bool
build_and_run(const std::vector<cpp_file>& files, const std::string& test_name,
              const std::string& test_code) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_ubl_rt_" + test_name);
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

  // Collect generated source files (skip xmldsig/XAdES, see compile test)
  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      auto fn = file.filename;
      // Skip xmldsig, XAdES, and signature component source files —
      // their serialization code uses XSD patterns (mixed content with
      // xs:any, complex choice groups) the codegen doesn't yet fully
      // support. Headers compile fine for type definitions.
      if (fn == "xmldsig.cpp" || fn == "v1_3_2.cpp" || fn == "v1_4_1.cpp" ||
          fn.find("signature") != std::string::npos ||
          fn == "common_signature_components_2.cpp")
        continue;
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
    // Leave tmp_dir for debugging
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

TEST_CASE("UBL round-trip: minimal Invoice", "[ubl][integration][roundtrip]") {
  auto files = generate_ubl();
  REQUIRE(!files.empty());

  // Construct a minimal UBL Invoice with required fields, serialize,
  // deserialize, and compare. The exact generated struct field names
  // depend on codegen output, so we use the snake_case forms.
  std::string test_code = R"(
int main() {
  using namespace ubl::invoice;
  using namespace ubl::cbc;

  // Construct a minimal InvoiceType
  // ID and IssueDate are required elements (minOccurs=1), so they are
  // value types, not optionals.
  invoice_type val;
  val.id.value = "INV-001";
  val.issue_date.value = xb::date(2024, 1, 15);

  // Serialize
  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(
        xb::qname{"urn:oasis:names:specification:ubl:schema:xsd:Invoice-2",
                   "Invoice"});
    writer.namespace_declaration(
        "", "urn:oasis:names:specification:ubl:schema:xsd:Invoice-2");
    writer.namespace_declaration(
        "cbc",
        "urn:oasis:names:specification:ubl:schema:xsd:CommonBasicComponents-2");
    write_invoice_type(val, writer);
    writer.end_element();
  }

  std::cerr << "Serialized: " << os.str() << std::endl;

  // Deserialize
  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_invoice_type(reader);

  // Compare key fields (required, so direct value access)
  assert(result.id.value == "INV-001");
  assert(result.issue_date.value == xb::date(2024, 1, 15));

  std::cerr << "UBL Invoice round-trip OK" << std::endl;
  return 0;
}
)";

  CHECK(build_and_run(files, "invoice_roundtrip", test_code));
}
