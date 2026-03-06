#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace xb;

namespace fs = std::filesystem;

static bool
build_and_run_json(const std::vector<cpp_file>& files,
                   const std::string& test_name, const std::string& test_code) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_json_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;
  for (const auto& file : files) {
    auto path = tmp_dir / file.filename;
    std::ofstream out(path);
    out << writer.write(file);
  }

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
    out << "#include <nlohmann/json.hpp>\n";
    out << "#include <cassert>\n";
    out << "#include <iostream>\n";
    out << "#include <string>\n";
    out << "\n";
    out << test_code;
  }

  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      source_files += ' ';
      source_files += (tmp_dir / file.filename).string();
    }
  }

  std::string include_dir = STRINGIFY(XB_INCLUDE_DIR);
  std::string lib_file = STRINGIFY(XB_LIB_FILE);
  std::string json_include_dir = STRINGIFY(XB_JSON_INCLUDE_DIR);
  auto exe_path = tmp_dir / "test_exe";

  std::string sanitizer_flags;
  if (XB_SANITIZERS)
    sanitizer_flags = "-fsanitize=undefined -fsanitize=address ";

  std::string cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" +
                    tmp_dir.string() + " -I" + include_dir + " -I" +
                    json_include_dir + " -o " + exe_path.string() + " " +
                    main_path.string() + source_files + " " + lib_file +
                    " -lexpat 2>&1";
  int rc = std::system(cmd.c_str());

  if (rc != 0) {
    std::cerr << "Build failed for " << test_name << "\n";
    std::cerr << "Command: " << cmd << "\n";
    fs::remove_all(tmp_dir);
    return false;
  }

  rc = std::system(exe_path.string().c_str());

  if (rc != 0)
    std::cerr << "Run failed for " << test_name << " (exit code " << rc
              << ")\n";

  fs::remove_all(tmp_dir);
  return rc == 0;
}

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

TEST_CASE("json round-trip: struct with string and int fields",
          "[json][round-trip]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "age"},
                                      qname{xs_ns, "int"}));

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "person"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, type_map::defaults(), opts).generate();

  std::string test_code = R"(
using namespace example::com::test;

int main() {
  person p;
  p.name = "Alice";
  p.age = 30;

  nlohmann::json j;
  to_json(j, p);

  assert(j["name"] == "Alice");
  assert(j["age"] == 30);

  person p2;
  from_json(j, p2);

  assert(p2.name == p.name);
  assert(p2.age == p.age);

  std::cout << "JSON round-trip passed\n";
  return 0;
}
)";

  REQUIRE(build_and_run_json(files, "person", test_code));
}

TEST_CASE("json round-trip: enum type", "[json][round-trip]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.enumeration = {"red", "green", "blue"};
  s.add_simple_type(simple_type(qname{"http://example.com/test", "color"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "favorite"},
                   qname{"http://example.com/test", "color"}));

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "preference"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, type_map::defaults(), opts).generate();

  std::string test_code = R"(
using namespace example::com::test;

int main() {
  preference p;
  p.favorite = color::green;

  nlohmann::json j;
  to_json(j, p);

  // The enum field should round-trip through JSON
  preference p2;
  from_json(j, p2);

  assert(p2.favorite == color::green);

  std::cout << "JSON enum round-trip passed\n";
  return 0;
}
)";

  REQUIRE(build_and_run_json(files, "enum_roundtrip", test_code));
}

TEST_CASE("json round-trip: optional field", "[json][round-trip]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  particle name_p(element_decl(qname{"http://example.com/test", "name"},
                               qname{xs_ns, "string"}));
  particle age_p(element_decl(qname{"http://example.com/test", "age"},
                              qname{xs_ns, "int"}));
  age_p.occurs.min_occurs = 0;

  std::vector<particle> particles;
  particles.push_back(std::move(name_p));
  particles.push_back(std::move(age_p));

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "person"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, type_map::defaults(), opts).generate();

  std::string test_code = R"(
using namespace example::com::test;

int main() {
  // Test with value present
  person p1;
  p1.name = "Bob";
  p1.age = 25;

  nlohmann::json j1;
  to_json(j1, p1);
  assert(j1.contains("age"));

  person p1r;
  from_json(j1, p1r);
  assert(p1r.age.has_value());
  assert(*p1r.age == 25);

  // Test with value absent
  person p2;
  p2.name = "Charlie";

  nlohmann::json j2;
  to_json(j2, p2);
  assert(!j2.contains("age"));

  person p2r;
  from_json(j2, p2r);
  assert(!p2r.age.has_value());

  std::cout << "JSON optional round-trip passed\n";
  return 0;
}
)";

  REQUIRE(build_and_run_json(files, "optional_roundtrip", test_code));
}
