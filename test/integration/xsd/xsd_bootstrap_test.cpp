#include <xb/codegen.hpp>
#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <variant>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

using namespace xb;
namespace fs = std::filesystem;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

static schema
parse_xsd_file(const fs::path& path) {
  std::ifstream file(path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  return parser.parse(reader);
}

// ===== Phase 1: Parse the schema-for-schemas =====

TEST_CASE("XSD schema-for-schemas parses without errors",
          "[xsd][bootstrap][parse]") {
  fs::path schema_dir = STRINGIFY(XB_XSD_SCHEMA_DIR);
  fs::path xml_xsd = schema_dir / "xml.xsd";
  fs::path xs_xsd = schema_dir / "XMLSchema.xsd";

  REQUIRE(fs::exists(xml_xsd));
  REQUIRE(fs::exists(xs_xsd));

  schema_set ss;

  // Parse xml.xsd first (imported dependency)
  REQUIRE_NOTHROW([&] {
    auto s = parse_xsd_file(xml_xsd);
    ss.add(std::move(s));
  }());

  // Parse XMLSchema.xsd
  REQUIRE_NOTHROW([&] {
    auto s = parse_xsd_file(xs_xsd);
    ss.add(std::move(s));
  }());

  REQUIRE_NOTHROW(ss.resolve());

  // Verify key types from the schema-for-schemas were parsed
  SECTION("Named complex types exist") {
    CHECK(ss.find_complex_type({xs_ns, "openAttrs"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "annotated"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "complexType"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "simpleType"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "element"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "attribute"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "group"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "wildcard"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "restrictionType"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "extensionType"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "facet"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "keybase"}) != nullptr);
    CHECK(ss.find_complex_type({xs_ns, "assertion"}) != nullptr);
  }

  SECTION("Named simple types exist") {
    CHECK(ss.find_simple_type({xs_ns, "formChoice"}) != nullptr);
    CHECK(ss.find_simple_type({xs_ns, "derivationSet"}) != nullptr);
    CHECK(ss.find_simple_type({xs_ns, "blockSet"}) != nullptr);
    CHECK(ss.find_simple_type({xs_ns, "namespaceList"}) != nullptr);
    CHECK(ss.find_simple_type({xs_ns, "allNNI"}) != nullptr);
    CHECK(ss.find_simple_type({xs_ns, "derivationControl"}) != nullptr);
  }

  SECTION("Named model groups exist") {
    CHECK(ss.find_model_group_def({xs_ns, "schemaTop"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "redefinable"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "typeDefParticle"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "nestedParticle"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "particle"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "attrDecls"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "complexTypeModel"}) != nullptr);
    CHECK(ss.find_model_group_def({xs_ns, "assertions"}) != nullptr);
  }

  SECTION("Named attribute groups exist") {
    CHECK(ss.find_attribute_group_def({xs_ns, "occurs"}) != nullptr);
    CHECK(ss.find_attribute_group_def({xs_ns, "defRef"}) != nullptr);
  }

  SECTION("Global elements exist") {
    CHECK(ss.find_element({xs_ns, "schema"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "element"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "attribute"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "complexType"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "simpleType"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "group"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "attributeGroup"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "annotation"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "include"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "import"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "sequence"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "choice"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "all"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "any"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "anyAttribute"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "restriction"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "list"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "union"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "simpleContent"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "complexContent"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "openContent"}) != nullptr);
    CHECK(ss.find_element({xs_ns, "defaultOpenContent"}) != nullptr);
  }
}

// ===== Phase 2: Generate C++ IR from the schema-for-schemas =====

static schema_set
parse_xsd_full() {
  schema_set ss;
  fs::path schema_dir = STRINGIFY(XB_XSD_SCHEMA_DIR);

  auto xml_schema = parse_xsd_file(schema_dir / "xml.xsd");
  ss.add(std::move(xml_schema));

  auto xs_schema = parse_xsd_file(schema_dir / "XMLSchema.xsd");
  ss.add(std::move(xs_schema));

  ss.resolve();
  return ss;
}

static std::vector<cpp_file>
generate_xsd_ir() {
  auto ss = parse_xsd_full();

  codegen_options opts;
  opts.namespace_map[xs_ns] = "xs";
  opts.namespace_map["http://www.w3.org/XML/1998/namespace"] = "xml_ns";
  opts.mode = output_mode::split;
  opts.generate_docs = true;

  auto types = type_map::defaults();
  codegen gen(ss, types, opts);
  return gen.generate();
}

TEST_CASE("XSD schema-for-schemas generates C++ IR",
          "[xsd][bootstrap][codegen]") {
  auto files = generate_xsd_ir();
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

// ===== Phase 3: Dump IR for analysis =====

// Helper to collect all struct/class/enum declarations from the IR
struct ir_type_info {
  std::string name;
  std::string kind; // "struct", "class", "enum", "alias"
  std::vector<std::pair<std::string, std::string>> fields; // {type, name}
};

static std::vector<ir_type_info>
collect_ir_types(const std::vector<cpp_file>& files) {
  std::vector<ir_type_info> result;

  for (const auto& file : files) {
    if (file.kind != file_kind::header) continue;

    for (const auto& ns : file.namespaces) {
      for (const auto& decl : ns.declarations) {
        std::visit(
            [&](const auto& d) {
              using T = std::decay_t<decltype(d)>;
              if constexpr (std::is_same_v<T, cpp_struct>) {
                ir_type_info info;
                info.name = d.name;
                info.kind = "struct";
                for (const auto& f : d.fields)
                  info.fields.push_back({f.type, f.name});
                result.push_back(std::move(info));
              } else if constexpr (std::is_same_v<T, cpp_class>) {
                ir_type_info info;
                info.name = d.name;
                info.kind = "class";
                for (const auto& f : d.fields)
                  info.fields.push_back({f.type, f.name});
                result.push_back(std::move(info));
              } else if constexpr (std::is_same_v<T, cpp_enum>) {
                ir_type_info info;
                info.name = d.name;
                info.kind = "enum";
                for (const auto& v : d.values)
                  info.fields.push_back({"enumerator", v.name});
                result.push_back(std::move(info));
              } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
                ir_type_info info;
                info.name = d.name;
                info.kind = "alias";
                info.fields.push_back({"target", d.target});
                result.push_back(std::move(info));
              }
            },
            decl);
      }
    }
  }
  return result;
}

TEST_CASE("XSD IR dump for comparison", "[xsd][bootstrap][dump]") {
  auto files = generate_xsd_ir();
  REQUIRE(!files.empty());

  auto types = collect_ir_types(files);

  // Print a structured dump for analysis
  std::cerr << "\n===== XSD Bootstrap IR Dump =====\n";
  std::cerr << "Total types generated: " << types.size() << "\n\n";

  for (const auto& t : types) {
    std::cerr << t.kind << " " << t.name << " {\n";
    for (const auto& [ftype, fname] : t.fields)
      std::cerr << "  " << ftype << " " << fname << "\n";
    std::cerr << "}\n\n";
  }

  // Write generated code into the build tree (config-specific)
  cpp_writer writer;
  fs::path out_dir = STRINGIFY(XB_XSD_OUTPUT_DIR);
  fs::create_directories(out_dir);

  for (const auto& file : files) {
    auto path = out_dir / file.filename;
    std::ofstream out(path);
    out << writer.write(file);
  }

  std::cerr << "Generated files written to: " << out_dir << "\n";
  std::cerr << "===== End IR Dump =====\n";

  // Basic sanity: we should have generated types for the core XSD constructs
  std::set<std::string> type_names;
  for (const auto& t : types)
    type_names.insert(t.name);

  // These should exist as generated types (snake_case versions of XSD type
  // names) The exact names depend on the naming convention; check some likely
  // candidates
  CHECK(!types.empty());

  // Print all type names for the comparison document
  std::cerr << "\n===== All Generated Type Names =====\n";
  for (const auto& name : type_names)
    std::cerr << "  " << name << "\n";
  std::cerr << "===== End Type Names =====\n";
}
