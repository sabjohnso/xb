#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_code.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using namespace xb;
using Catch::Matchers::ContainsSubstring;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
static const std::string test_ns = "http://example.com/test";

static schema_set
make_schema_set(schema s) {
  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();
  return ss;
}

static type_map
default_types() {
  return type_map::defaults();
}

static std::vector<const cpp_function*>
find_all_functions(const cpp_file& file, const std::string& name) {
  std::vector<const cpp_function*> result;
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* f = std::get_if<cpp_function>(&decl)) {
        if (f->name == name) result.push_back(f);
      }
    }
  }
  return result;
}

static const cpp_function*
find_function_with_param(const cpp_file& file, const std::string& fn_name,
                         const std::string& param_substr) {
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* f = std::get_if<cpp_function>(&decl)) {
        if (f->name == fn_name &&
            f->parameters.find(param_substr) != std::string::npos)
          return f;
      }
    }
  }
  return nullptr;
}

// ===== Enum JSON codegen =====

TEST_CASE("json codegen generates to_json for enum", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);
  s.add_simple_type(simple_type(qname{test_ns, "color"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, [] {
                                  facet_set f;
                                  f.enumeration = {"red", "green", "blue"};
                                  return f;
                                }()));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "to_json", "color");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("to_string"));
}

TEST_CASE("json codegen generates from_json for enum", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);
  s.add_simple_type(simple_type(qname{test_ns, "color"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, [] {
                                  facet_set f;
                                  f.enumeration = {"red", "green", "blue"};
                                  return f;
                                }()));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "from_json", "color");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("color_from_string"));
}

// ===== Complex type JSON codegen =====

TEST_CASE("json codegen generates to_json for simple struct",
          "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "to_json", "person_type");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("j[\"name\"]"));
  CHECK_THAT(f->body, ContainsSubstring("value.name"));
}

TEST_CASE("json codegen generates from_json for simple struct",
          "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "from_json", "person_type");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("\"name\""));
  CHECK_THAT(f->body, ContainsSubstring("value.name"));
}

TEST_CASE("json codegen handles optional fields", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  particle p(element_decl(qname{test_ns, "age"}, qname{xs_ns, "int"}));
  p.occurs.min_occurs = 0;

  std::vector<particle> particles;
  particles.push_back(std::move(p));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "to_json", "person_type");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("if (value.age)"));
}

TEST_CASE("json codegen handles vector fields", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  particle p(element_decl(qname{test_ns, "item"}, qname{xs_ns, "string"}));
  p.occurs.max_occurs = unbounded;

  std::vector<particle> particles;
  particles.push_back(std::move(p));

  s.add_complex_type(complex_type(
      qname{test_ns, "list_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "to_json", "list_type");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("j[\"item\"]"));
  CHECK_THAT(f->body, ContainsSubstring("value.item"));
}

TEST_CASE("json codegen not generated when json_mode::none", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::none;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  CHECK(find_all_functions(files[0], "to_json").empty());
  CHECK(find_all_functions(files[0], "from_json").empty());
}

TEST_CASE("json codegen handles choice variant", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> choice_particles;
  choice_particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));
  choice_particles.emplace_back(
      element_decl(qname{test_ns, "id"}, qname{xs_ns, "int"}));

  std::vector<particle> seq_particles;
  seq_particles.emplace_back(std::make_unique<model_group>(
      compositor_kind::choice, std::move(choice_particles)));

  s.add_complex_type(complex_type(
      qname{test_ns, "choice_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(seq_particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  auto* f = find_function_with_param(files[0], "to_json", "choice_type");
  REQUIRE(f != nullptr);
  CHECK_THAT(f->body, ContainsSubstring("std::visit"));
  CHECK_THAT(f->body, ContainsSubstring("choice_type"));
}

TEST_CASE("json codegen adds nlohmann/json.hpp include", "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::enabled;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  bool has_json_include = false;
  bool has_json_value_include = false;
  for (const auto& inc : files[0].includes) {
    if (inc.path == "<nlohmann/json.hpp>") has_json_include = true;
    if (inc.path == "\"xb/json_value.hpp\"") has_json_value_include = true;
  }
  CHECK(has_json_include);
  CHECK(has_json_value_include);
}

TEST_CASE("json codegen no nlohmann include when json disabled",
          "[json_codegen]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{test_ns, "name"}, qname{xs_ns, "string"}));

  s.add_complex_type(complex_type(
      qname{test_ns, "person_type"}, false, false,
      content_type(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   model_group(compositor_kind::sequence,
                                               std::move(particles))))));

  auto ss = make_schema_set(std::move(s));
  codegen_options opts;
  opts.json = json_mode::none;
  auto files = codegen(ss, default_types(), opts).generate();
  REQUIRE(!files.empty());

  for (const auto& inc : files[0].includes)
    CHECK(inc.path != "<nlohmann/json.hpp>");
}
