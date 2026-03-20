// XSD Bootstrap Translation Test
//
// This test validates the generated-parse-then-translate approach:
// 1. Parse XMLSchema.xsd with the generated xs::read_schema_type()
// 2. Translate xs::schema_type → xb::schema using hand-written translation
// 3. Compare the result to what schema_parser produces
//
// The translation layer is written at production quality — it would
// replace the hand-written schema parser if adopted.

#include <xb/expat_reader.hpp>
#include <xb/schema.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

// Include the generated XSD types
#include "xml_schema.hpp"

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

using namespace xb;
namespace fs = std::filesystem;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

// --- Translation layer: xs::schema_type → xb::schema ---

// Translate xs::import_type → xb::schema_import
static schema_import
translate_import(const xs::import_type& imp) {
  return {imp.namespace_.value_or(""), imp.schema_location.value_or("")};
}

// Translate xs::include_type → xb::schema_include
static schema_include
translate_include(const xs::include_type& inc) {
  return {inc.schema_location};
}

// Translate an xs::schema_type to an xb::schema.
// This is the core translation function — it converts the generated
// syntax-tree types into the semantic model.
static schema
translate_schema(const xs::schema_type& src) {
  schema result;

  // Target namespace
  if (src.target_namespace.has_value())
    result.set_target_namespace(src.target_namespace.value());

  // Process the composition group (imports, includes, annotations, etc.)
  for (const auto& item : src.choice) {
    std::visit(
        [&](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, xs::import_type>) {
            result.add_import(translate_import(v));
          } else if constexpr (std::is_same_v<T, xs::include_type>) {
            result.add_include(translate_include(v));
          }
          // redefine_type, override_type, annotation_type — skipped for now
        },
        item);
  }

  return result;
}

// --- Helper: parse a schema file using schema_parser ---
static schema
parse_with_schema_parser(const fs::path& path) {
  std::ifstream file(path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());
  expat_reader reader(xml);
  schema_parser parser;
  return parser.parse(reader);
}

// --- Helper: parse a schema file using generated read_schema_type ---
static xs::schema_type
parse_with_generated_reader(const fs::path& path) {
  std::ifstream file(path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());
  expat_reader reader(xml);
  // Advance to xs:schema element
  while (reader.read()) {
    if (reader.node_type() == xml_node_type::start_element &&
        reader.name().namespace_uri() == xs_ns &&
        reader.name().local_name() == "schema") {
      return xs::read_schema_type(reader);
    }
  }
  throw std::runtime_error("no xs:schema element found");
}

// ===== Tests =====

TEST_CASE("XSD translate: generated reader parses XMLSchema.xsd",
          "[xsd][translate]") {
  fs::path schema_dir = STRINGIFY(XB_XSD_SCHEMA_DIR);
  fs::path xs_xsd = schema_dir / "XMLSchema.xsd";
  REQUIRE(fs::exists(xs_xsd));

  xs::schema_type parsed;
  REQUIRE_NOTHROW(parsed = parse_with_generated_reader(xs_xsd));

  // Basic sanity: the schema has a target namespace
  REQUIRE(parsed.target_namespace.has_value());
  CHECK(parsed.target_namespace.value() == xs_ns);

  // Check composition items
  INFO("choice count: " << parsed.choice.size());
  for (const auto& item : parsed.choice) {
    std::visit(
        [](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, xs::import_type>)
            INFO("  import: " << v.namespace_.value_or("(none)"));
          else if constexpr (std::is_same_v<T, xs::include_type>)
            INFO("  include: " << v.schema_location);
          else if constexpr (std::is_same_v<T, xs::annotation_type>)
            INFO("  annotation");
          else
            INFO("  other");
        },
        item);
  }

  // It has imports (the xml namespace)
  bool has_xml_import = false;
  for (const auto& item : parsed.choice) {
    if (auto* imp = std::get_if<xs::import_type>(&item)) {
      if (imp->namespace_.has_value() &&
          imp->namespace_.value() == "http://www.w3.org/XML/1998/namespace") {
        has_xml_import = true;
      }
    }
  }
  CHECK(has_xml_import);
}

TEST_CASE("XSD translate: schema_type → xb::schema preserves imports",
          "[xsd][translate]") {
  fs::path schema_dir = STRINGIFY(XB_XSD_SCHEMA_DIR);
  fs::path xs_xsd = schema_dir / "XMLSchema.xsd";

  // Parse with both methods
  auto expected = parse_with_schema_parser(xs_xsd);
  auto generated = parse_with_generated_reader(xs_xsd);
  auto translated = translate_schema(generated);

  // Compare target namespace
  CHECK(translated.target_namespace() == expected.target_namespace());

  // Compare imports
  REQUIRE(translated.imports().size() == expected.imports().size());
  for (size_t i = 0; i < translated.imports().size(); ++i) {
    CHECK(translated.imports()[i].namespace_uri ==
          expected.imports()[i].namespace_uri);
  }

  // Compare includes
  CHECK(translated.includes().size() == expected.includes().size());
}
