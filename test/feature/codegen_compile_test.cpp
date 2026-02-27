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
#include <string>

using namespace xb;

namespace fs = std::filesystem;

// Write generated files to a temp directory and compile them
static bool
compile_generated_files(const std::vector<cpp_file>& files,
                        const std::string& test_name) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_test_" + test_name);
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
    for (const auto& file : files) {
      if (file.kind == file_kind::header)
        out << "#include \"" << file.filename << "\"\n";
    }
    out << "int main() { return 0; }\n";
  }

  // Collect source files for compilation
  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      source_files += ' ';
      source_files += (tmp_dir / file.filename).string();
    }
  }

  // Compile with the system C++ compiler
  std::string include_dir = STRINGIFY(XB_INCLUDE_DIR);
  std::string lib_file = STRINGIFY(XB_LIB_FILE);

  // When the library was built with sanitizers (RelWithDebInfo), pass
  // the same flags to the subprocess so it can link the sanitizer runtime.
  std::string sanitizer_flags;
  if (XB_SANITIZERS)
    sanitizer_flags = "-fsanitize=undefined -fsanitize=address ";

  std::string cmd;
  if (source_files.empty()) {
    // Header-only: syntax check suffices
    cmd = "c++ -std=c++20 -fsyntax-only -I" + tmp_dir.string() + " -I" +
          include_dir + " " + main_path.string() + " 2>&1";
  } else {
    // Split mode: compile and link all translation units against xb runtime
    auto exe_path = tmp_dir / "test_exe";
    cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" + tmp_dir.string() +
          " -I" + include_dir + " -o " + exe_path.string() + " " +
          main_path.string() + source_files + " " + lib_file + " -lexpat 2>&1";
  }
  int rc = std::system(cmd.c_str());

  // Cleanup
  fs::remove_all(tmp_dir);

  return rc == 0;
}

TEST_CASE("generate from sequence + attributes compiles",
          "[codegen][compile]") {
  schema s;
  s.set_target_namespace("http://example.com/order");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  // Simple enum type
  facet_set side_facets;
  side_facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/order", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, side_facets));

  // Complex type with sequence + attributes
  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "symbol"}, qname{xs, "string"}));
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "quantity"}, qname{xs, "int"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/order", "price"},
                   qname{xs, "double"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs, "string"}, true, {}, {}});
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/order", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/order", "OrderType"}, false, false,
                   std::move(ct), std::move(attrs)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "sequence_attrs"));
}

TEST_CASE("generate from choice compiles", "[codegen][compile]") {
  schema s;
  s.set_target_namespace("http://example.com/msg");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/msg", "text"},
                                      qname{xs, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/msg", "code"}, qname{xs, "int"}));
  model_group choice(compositor_kind::choice, std::move(particles));

  content_type ct(content_kind::element_only,
                  complex_content(qname{}, derivation_method::restriction,
                                  std::move(choice)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/msg", "MessageType"}, false, false,
                   std::move(ct)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "choice"));
}

TEST_CASE("generate from enumeration compiles", "[codegen][compile]") {
  schema s;
  s.set_target_namespace("http://example.com/types");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set facets;
  facets.enumeration = {"Red", "Green", "Blue", "Alpha-Channel"};
  s.add_simple_type(simple_type(qname{"http://example.com/types", "ColorType"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, facets));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "enumeration"));
}

TEST_CASE("generate from two schemas with cross-reference compiles",
          "[codegen][compile]") {
  std::string xs = "http://www.w3.org/2001/XMLSchema";

  schema s1;
  s1.set_target_namespace("http://example.com/base");
  s1.add_simple_type(simple_type(qname{"http://example.com/base", "IDType"},
                                 simple_type_variety::atomic,
                                 qname{xs, "string"}));

  schema s2;
  s2.set_target_namespace("http://example.com/app");
  s2.add_import(schema_import{"http://example.com/base", ""});

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/app", "id"},
                   qname{"http://example.com/base", "IDType"}));
  particles.emplace_back(element_decl(qname{"http://example.com/app", "name"},
                                      qname{xs, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s2.add_complex_type(
      complex_type(qname{"http://example.com/app", "EntityType"}, false, false,
                   std::move(ct)));

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 2);
  CHECK(compile_generated_files(files, "cross_ref"));
}

TEST_CASE("generate from xb-typemap.xsd compiles", "[codegen][compile]") {
  // Parse the actual xb-typemap.xsd
  std::string schema_path =
      std::string(STRINGIFY(XB_SCHEMA_DIR)) + "/xb-typemap.xsd";
  std::ifstream file(schema_path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  auto s = parser.parse(reader);

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(!files.empty());
  // The generated code should at least compile syntactically
  CHECK(compile_generated_files(files, "typemap_xsd"));
}

// ===== Split mode compile tests =====

TEST_CASE("split mode: sequence + attributes compiles",
          "[codegen][compile][split]") {
  schema s;
  s.set_target_namespace("http://example.com/order");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set side_facets;
  side_facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/order", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, side_facets));

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "symbol"}, qname{xs, "string"}));
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "quantity"}, qname{xs, "int"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/order", "price"},
                   qname{xs, "double"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs, "string"}, true, {}, {}});
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/order", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/order", "OrderType"}, false, false,
                   std::move(ct), std::move(attrs)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  REQUIRE(files.size() == 2);
  CHECK(compile_generated_files(files, "split_sequence_attrs"));
}

TEST_CASE("compile: complex type with open content",
          "[codegen][compile][open_content]") {
  std::string xs = "http://www.w3.org/2001/XMLSchema";

  schema s;
  s.set_target_namespace("http://example.com/oc");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/oc", "data"},
                                      qname{xs, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{
      open_content_mode::interleave,
      wildcard{wildcard_ns_constraint::any, {}, process_contents::lax, {}, {}}};

  s.add_complex_type(complex_type(qname{"http://example.com/oc", "FlexType"},
                                  false, false, std::move(ct), {}, {}, {},
                                  std::move(oc)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "open_content"));
}

TEST_CASE("generate from CTA element compiles", "[codegen][compile][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/cta");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  // Alternative types (each with a "kind" attribute for CTA dispatch)
  std::vector<attribute_use> car_attrs;
  car_attrs.push_back({qname{"", "kind"}, qname{xs, "string"}, true, {}, {}});
  std::vector<particle> car_particles;
  car_particles.emplace_back(
      element_decl(qname{"http://example.com/cta", "doors"}, qname{xs, "int"}));
  model_group car_seq(compositor_kind::sequence, std::move(car_particles));
  content_type car_ct(content_kind::element_only,
                      complex_content(qname{}, derivation_method::restriction,
                                      std::move(car_seq)));
  s.add_complex_type(complex_type(qname{"http://example.com/cta", "CarType"},
                                  false, false, std::move(car_ct),
                                  std::move(car_attrs)));

  std::vector<attribute_use> truck_attrs;
  truck_attrs.push_back({qname{"", "kind"}, qname{xs, "string"}, true, {}, {}});
  std::vector<particle> truck_particles;
  truck_particles.emplace_back(element_decl(
      qname{"http://example.com/cta", "payload"}, qname{xs, "double"}));
  model_group truck_seq(compositor_kind::sequence, std::move(truck_particles));
  content_type truck_ct(content_kind::element_only,
                        complex_content(qname{}, derivation_method::restriction,
                                        std::move(truck_seq)));
  s.add_complex_type(complex_type(qname{"http://example.com/cta", "TruckType"},
                                  false, false, std::move(truck_ct),
                                  std::move(truck_attrs)));

  // Container type with CTA element
  std::vector<type_alternative> alts = {
      {std::string("@kind = 'car'"),
       qname{"http://example.com/cta", "CarType"}},
      {std::string("@kind = 'truck'"),
       qname{"http://example.com/cta", "TruckType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/cta", "vehicle"},
                   qname{"http://example.com/cta", "CarType"}, false, false,
                   std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/cta", "GarageType"},
                                  false, false, std::move(ct)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "cta_element"));
}

TEST_CASE("split mode: xb-typemap.xsd compiles", "[codegen][compile][split]") {
  std::string schema_path =
      std::string(STRINGIFY(XB_SCHEMA_DIR)) + "/xb-typemap.xsd";
  std::ifstream file(schema_path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  auto s = parser.parse(reader);

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  REQUIRE(files.size() == 2);
  CHECK(compile_generated_files(files, "split_typemap_xsd"));
}

// ===== XSD 1.1: Assertion compile tests =====

TEST_CASE("generate from complex type with assertion compiles",
          "[codegen][compile][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/assert");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/assert", "start"}, qname{xs, "int"}));
  particles.emplace_back(element_decl(qname{"http://example.com/assert", "end"},
                                      qname{xs, "int"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(
      qname{"http://example.com/assert", "DateRange"}, false, false,
      std::move(ct), {}, {}, std::nullopt, std::nullopt, {{"end >= start"}}));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "assertion_compile"));
}

TEST_CASE("generate unsupported assertion compiles",
          "[codegen][compile][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/assert");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/assert", "x"},
                                      qname{xs, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/assert", "FancyType"}, false,
                   false, std::move(ct), {}, {}, std::nullopt, std::nullopt,
                   {{"fn:string-length($value) > 5"}}));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "assertion_unsupported_compile"));
}

// ===== Facet & cardinality compile tests =====

TEST_CASE("generate from simple type with range facets compiles",
          "[codegen][compile][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/facet");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set facets;
  facets.min_inclusive = "0";
  facets.max_exclusive = "100";

  s.add_simple_type(simple_type(qname{"http://example.com/facet", "Percent"},
                                simple_type_variety::atomic, qname{xs, "int"},
                                facets));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "facet_range_compile"));
}

TEST_CASE("generate from simple type with pattern facet compiles",
          "[codegen][compile][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/facet");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set facets;
  facets.pattern = "[A-Z]{3}";

  s.add_simple_type(simple_type(qname{"http://example.com/facet", "CurrCode"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, facets));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "facet_pattern_compile"));
}

TEST_CASE("generate from complex type with cardinality constraints compiles",
          "[codegen][compile][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/card");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/card", "item"},
                                      qname{xs, "string"}),
                         occurrence{1, 10});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/card", "BoundedList"}, false,
                   false, std::move(ct)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "cardinality_compile"));
}

TEST_CASE("generate from complex type with simple content facets compiles",
          "[codegen][compile][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/facet");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set facets;
  facets.min_inclusive = "0";
  facets.max_inclusive = "999";

  content_type ct(
      content_kind::simple,
      simple_content{qname{xs, "int"}, derivation_method::restriction, facets});

  s.add_complex_type(
      complex_type(qname{"http://example.com/facet", "PriceType"}, false, false,
                   std::move(ct),
                   {attribute_use{qname{"", "currency"}, qname{xs, "string"},
                                  true, std::nullopt, std::nullopt}}));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();

  REQUIRE(files.size() == 1);
  CHECK(compile_generated_files(files, "facet_simple_content_compile"));
}
