// GCC 12-13 emit a false -Wmaybe-uninitialized when constructing
// particle objects (std::variant containing std::unique_ptr) at -O3.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

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

// Helper to find a struct declaration in a cpp_file
static const cpp_struct*
find_struct(const cpp_file& file, const std::string& name) {
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* s = std::get_if<cpp_struct>(&decl)) {
        if (s->name == name) return s;
      }
    }
  }
  return nullptr;
}

// Helper to find an enum declaration in a cpp_file
static const cpp_enum*
find_enum(const cpp_file& file, const std::string& name) {
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* e = std::get_if<cpp_enum>(&decl)) {
        if (e->name == name) return e;
      }
    }
  }
  return nullptr;
}

// Helper to find a type alias declaration
static const cpp_type_alias*
find_alias(const cpp_file& file, const std::string& name) {
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* a = std::get_if<cpp_type_alias>(&decl)) {
        if (a->name == name) return a;
      }
    }
  }
  return nullptr;
}

// Helper to find a function declaration
static const cpp_function*
find_function(const cpp_file& file, const std::string& name) {
  for (const auto& ns : file.namespaces) {
    for (const auto& decl : ns.declarations) {
      if (auto* f = std::get_if<cpp_function>(&decl)) {
        if (f->name == name) return f;
      }
    }
  }
  return nullptr;
}

// Helper to find a field in a struct
static const cpp_field*
find_field(const cpp_struct& s, const std::string& name) {
  for (const auto& f : s.fields) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

// TDD step 1: Empty schema -> empty cpp_file
TEST_CASE("empty schema produces empty file", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");
  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  CHECK(files[0].namespaces.size() == 1);
  CHECK(files[0].namespaces[0].declarations.empty());
}

// TDD step 2: Schema with target namespace -> file wrapped in C++ namespace
TEST_CASE("target namespace maps to C++ namespace", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/order");
  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.namespace_map["http://example.com/order"] = "example::order";
  codegen gen(ss, types, opts);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  REQUIRE(files[0].namespaces.size() == 1);
  CHECK(files[0].namespaces[0].name == "example::order");
}

// TDD step 3: Built-in type lookup
TEST_CASE("builtin type lookup via type_map", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // A complex type with a sequence containing an xs:string element
  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{xs_ns, "value"}, qname{xs_ns, "string"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "MyType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "my_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "value");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::string");
}

// TDD step 4: Simple type with enumeration -> cpp_enum
TEST_CASE("simple type enumeration generates enum class", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.enumeration = {"Red", "Green", "Blue"};
  s.add_simple_type(simple_type(qname{"http://example.com/test", "Color"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* e = find_enum(files[0], "color");
  REQUIRE(e != nullptr);
  REQUIRE(e->values.size() == 3);
  CHECK(e->values[0].name == "red");
  CHECK(e->values[0].xml_value == "Red");
  CHECK(e->values[1].name == "green");
  CHECK(e->values[1].xml_value == "Green");
  CHECK(e->values[2].name == "blue");
  CHECK(e->values[2].xml_value == "Blue");
}

// TDD step 5: Simple type list -> type alias to vector
TEST_CASE("simple type list generates vector alias", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.add_simple_type(simple_type(qname{"http://example.com/test", "StringList"},
                                simple_type_variety::list, qname{}, {},
                                qname{xs_ns, "string"}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* a = find_alias(files[0], "string_list");
  REQUIRE(a != nullptr);
  CHECK(a->target == "std::vector<std::string>");
}

// TDD step 6: Simple type union -> type alias to variant
TEST_CASE("simple type union generates variant alias", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.add_simple_type(simple_type(qname{"http://example.com/test", "StringOrInt"},
                                simple_type_variety::union_type, qname{}, {},
                                std::nullopt,
                                {qname{xs_ns, "string"}, qname{xs_ns, "int"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* a = find_alias(files[0], "string_or_int");
  REQUIRE(a != nullptr);
  CHECK(a->target == "std::variant<std::string, int32_t>");
}

// TDD step 7: Simple type atomic restriction (no enum) -> type alias
TEST_CASE("simple type atomic restriction generates alias", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.add_simple_type(simple_type(qname{"http://example.com/test", "MyString"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* a = find_alias(files[0], "my_string");
  REQUIRE(a != nullptr);
  CHECK(a->target == "std::string");
}

// TDD step 8: Complex type with sequence -> struct with fields in order
TEST_CASE("complex type sequence generates struct", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "age"},
                                      qname{xs_ns, "int"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "PersonType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "person_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 2);
  CHECK(st->fields[0].name == "name");
  CHECK(st->fields[0].type == "std::string");
  CHECK(st->fields[1].name == "age");
  CHECK(st->fields[1].type == "int32_t");
}

// TDD step 9: Complex type with choice -> struct with variant field
TEST_CASE("complex type choice generates variant field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "id"},
                                      qname{xs_ns, "int"}));
  model_group choice(compositor_kind::choice, std::move(particles));

  content_type ct(content_kind::element_only,
                  complex_content(qname{}, derivation_method::restriction,
                                  std::move(choice)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "IdentifierType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "identifier_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 1);
  CHECK(st->fields[0].name == "choice");
  CHECK(st->fields[0].type == "std::variant<std::string, int32_t>");
}

// TDD step 10: Complex type with all -> struct with fields
TEST_CASE("complex type all generates struct", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "y"}, qname{xs_ns, "int"}));
  model_group all(compositor_kind::all, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(all)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "PointType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "point_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 2);
  CHECK(st->fields[0].name == "x");
  CHECK(st->fields[1].name == "y");
}

// TDD step 11: Required attribute -> T field
TEST_CASE("required attribute generates plain field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ItemType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "id");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::string");
}

// TDD step 12: Optional attribute -> std::optional<T>
TEST_CASE("optional attribute generates optional field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "tag"}, qname{xs_ns, "string"}, false, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ItemType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "tag");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::optional<std::string>");
}

// TDD step 13: Optional element (0,1) -> std::optional<T>
TEST_CASE("optional element generates optional field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "note"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ItemType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "note");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::optional<std::string>");
}

// TDD step 14: Unbounded element -> std::vector<T>
TEST_CASE("unbounded element generates vector field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ListType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "list_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "item");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<std::string>");
}

// TDD step 15: Nillable element -> std::optional<T>
TEST_CASE("nillable element generates optional field", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "value"},
                                      qname{xs_ns, "int"},
                                      true)); // nillable=true
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "NillableType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "nillable_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "value");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::optional<int32_t>");
}

// TDD step 16: xs:any wildcard -> vector<any_element>
TEST_CASE("any wildcard generates any_element vector", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(wildcard{});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ExtType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "ext_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "any");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<xb::any_element>");
}

// TDD step 17: xs:anyAttribute -> vector<any_attribute>
TEST_CASE("any attribute wildcard generates any_attribute vector",
          "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ExtType"},
                                  false, false, std::move(ct), {}, // attributes
                                  {},           // attribute_group_refs
                                  wildcard{})); // attribute_wildcard

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "ext_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "any_attribute");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<xb::any_attribute>");
}

// TDD step 18: Element ref -> resolved to referenced element's type
TEST_CASE("element ref resolves to referenced type", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Global element declaration
  s.add_element(element_decl(qname{"http://example.com/test", "Name"},
                             qname{xs_ns, "string"}));

  // Complex type referencing the element
  std::vector<particle> particles;
  particles.emplace_back(element_ref{qname{"http://example.com/test", "Name"}});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "PersonType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "person_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "name");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::string");
}

// TDD step 19: Group ref -> particles inlined
TEST_CASE("group ref inlines particles", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Model group definition
  std::vector<particle> group_particles;
  group_particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  group_particles.emplace_back(
      element_decl(qname{"http://example.com/test", "y"}, qname{xs_ns, "int"}));
  s.add_model_group_def(model_group_def(
      qname{"http://example.com/test", "CoordGroup"},
      model_group(compositor_kind::sequence, std::move(group_particles))));

  // Complex type referencing the group
  std::vector<particle> particles;
  particles.emplace_back(
      group_ref{qname{"http://example.com/test", "CoordGroup"}});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "PointType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "point_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 2);
  CHECK(st->fields[0].name == "x");
  CHECK(st->fields[1].name == "y");
}

// TDD step 20: Attribute group ref -> attributes inlined
TEST_CASE("attribute group ref inlines attributes", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Attribute group definition
  std::vector<attribute_use> group_attrs;
  group_attrs.push_back(
      {qname{"", "id"}, qname{xs_ns, "string"}, true, {}, {}});
  group_attrs.push_back(
      {qname{"", "name"}, qname{xs_ns, "string"}, false, {}, {}});
  s.add_attribute_group_def(attribute_group_def(
      qname{"http://example.com/test", "CommonAttrs"}, std::move(group_attrs)));

  // Complex type referencing the attribute group
  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_group_ref> attr_group_refs;
  attr_group_refs.push_back({qname{"http://example.com/test", "CommonAttrs"}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ItemType"},
                                  false, false, std::move(ct),
                                  {}, // direct attributes
                                  std::move(attr_group_refs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 2);
  CHECK(st->fields[0].name == "id");
  CHECK(st->fields[0].type == "std::string");
  CHECK(st->fields[1].name == "name");
  CHECK(st->fields[1].type == "std::optional<std::string>");
}

// TDD step 21: Multi-schema namespace mapping
TEST_CASE("multi-schema generates multiple files", "[codegen]") {
  schema s1;
  s1.set_target_namespace("http://example.com/types");
  s1.add_simple_type(simple_type(qname{"http://example.com/types", "ID"},
                                 simple_type_variety::atomic,
                                 qname{xs_ns, "string"}));

  schema s2;
  s2.set_target_namespace("http://example.com/order");
  s2.add_simple_type(simple_type(qname{"http://example.com/order", "OrderID"},
                                 simple_type_variety::atomic,
                                 qname{xs_ns, "string"}));

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  ss.resolve();

  auto types = default_types();
  codegen gen(ss, types);
  auto files = gen.generate();
  CHECK(files.size() == 2);
}

// TDD step 22: Include dependencies
TEST_CASE("cross-namespace type reference generates include", "[codegen]") {
  schema s1;
  s1.set_target_namespace("http://example.com/types");
  s1.add_simple_type(simple_type(qname{"http://example.com/types", "Amount"},
                                 simple_type_variety::atomic,
                                 qname{xs_ns, "decimal"}));

  schema s2;
  s2.set_target_namespace("http://example.com/order");
  s2.add_import(schema_import{"http://example.com/types", ""});

  // Complex type in schema 2 using type from schema 1
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/order", "total"},
                   qname{"http://example.com/types", "Amount"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s2.add_complex_type(
      complex_type(qname{"http://example.com/order", "OrderType"}, false, false,
                   std::move(ct)));

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  ss.resolve();

  auto types = default_types();
  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 2);

  // Find the order file and check for include of types file
  const cpp_file* order_file = nullptr;
  for (const auto& f : files) {
    for (const auto& ns : f.namespaces) {
      for (const auto& decl : ns.declarations) {
        if (auto* st = std::get_if<cpp_struct>(&decl)) {
          if (st->name == "order_type") {
            order_file = &f;
            break;
          }
        }
      }
    }
  }
  REQUIRE(order_file != nullptr);

  bool has_types_include = false;
  for (const auto& inc : order_file->includes) {
    if (inc.path.find("types") != std::string::npos) has_types_include = true;
  }
  CHECK(has_types_include);
}

// ===== Group 3: Advanced Translation =====

// TDD step 1: complexContent extension -> flattened struct
TEST_CASE("complex content extension flattens base fields", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Base type: has a 'name' field
  std::vector<particle> base_particles;
  base_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "name"}, qname{xs_ns, "string"}));
  model_group base_seq(compositor_kind::sequence, std::move(base_particles));

  content_type base_ct(content_kind::element_only,
                       complex_content(qname{}, derivation_method::restriction,
                                       std::move(base_seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, std::move(base_ct)));

  // Derived type: extends BaseType, adds 'age' field
  std::vector<particle> derived_particles;
  derived_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "age"}, qname{xs_ns, "int"}));
  model_group derived_seq(compositor_kind::sequence,
                          std::move(derived_particles));

  content_type derived_ct(
      content_kind::element_only,
      complex_content(qname{"http://example.com/test", "BaseType"},
                      derivation_method::extension, std::move(derived_seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "DerivedType"}, false,
                   false, std::move(derived_ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "derived_type");
  REQUIRE(st != nullptr);
  // Should have both base and derived fields (flattened)
  REQUIRE(st->fields.size() == 2);
  CHECK(st->fields[0].name == "name");
  CHECK(st->fields[0].type == "std::string");
  CHECK(st->fields[1].name == "age");
  CHECK(st->fields[1].type == "int32_t");
}

// TDD step 2: complexContent restriction -> struct with restricted fields
TEST_CASE("complex content restriction generates struct", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Base type with two fields
  std::vector<particle> base_particles;
  base_particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  base_particles.emplace_back(
      element_decl(qname{"http://example.com/test", "y"}, qname{xs_ns, "int"}));
  model_group base_seq(compositor_kind::sequence, std::move(base_particles));
  content_type base_ct(content_kind::element_only,
                       complex_content(qname{}, derivation_method::restriction,
                                       std::move(base_seq)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, std::move(base_ct)));

  // Restricted type: only has 'x' field
  std::vector<particle> rest_particles;
  rest_particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  model_group rest_seq(compositor_kind::sequence, std::move(rest_particles));
  content_type rest_ct(
      content_kind::element_only,
      complex_content(qname{"http://example.com/test", "BaseType"},
                      derivation_method::restriction, std::move(rest_seq)));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "RestrictedType"}, false,
                   false, std::move(rest_ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "restricted_type");
  REQUIRE(st != nullptr);
  // Should have only the restricted fields
  REQUIRE(st->fields.size() == 1);
  CHECK(st->fields[0].name == "x");
}

// TDD step 3: simpleContent extension -> struct with value + attrs
TEST_CASE("simple content extension generates value struct", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct(
      content_kind::simple,
      simple_content{qname{xs_ns, "string"}, derivation_method::extension, {}});

  std::vector<attribute_use> attrs;
  attrs.push_back(
      {qname{"", "currency"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "MoneyType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "money_type");
  REQUIRE(st != nullptr);
  // Should have 'value' field + attribute fields
  auto* vf = find_field(*st, "value");
  REQUIRE(vf != nullptr);
  CHECK(vf->type == "std::string");
  auto* cf = find_field(*st, "currency");
  REQUIRE(cf != nullptr);
  CHECK(cf->type == "std::string");
}

// TDD step 4: Anonymous types -> synthetic name
TEST_CASE("anonymous complex type gets synthetic name", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Element with anonymous complex type (name has _type suffix convention)
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  // Anonymous type named after the element: "item_type"
  s.add_complex_type(complex_type(qname{"http://example.com/test", "item_type"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
}

// TDD step 5: Default values -> field initializer
TEST_CASE("attribute default value becomes field initializer", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "count"}, qname{xs_ns, "int"}, false, "10", {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ItemType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "item_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "count");
  REQUIRE(f != nullptr);
  CHECK(f->default_value == "10");
}

// TDD step 6: Fixed values -> field initializer
TEST_CASE("attribute fixed value becomes field initializer", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "version"},
                   qname{xs_ns, "string"},
                   true,
                   {},
                   std::string("2.0")});

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "HeaderType"}, false, false,
                   std::move(ct), std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "header_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "version");
  REQUIRE(f != nullptr);
  CHECK(f->default_value == "\"2.0\"");
}

// TDD step 7: Abstract type + substitution group -> variant
TEST_CASE("substitution group generates variant", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Abstract base element
  s.add_element(element_decl(qname{"http://example.com/test", "Shape"},
                             qname{"http://example.com/test", "ShapeType"},
                             false, true)); // abstract=true

  // Concrete substitution members
  s.add_element(element_decl(qname{"http://example.com/test", "Circle"},
                             qname{"http://example.com/test", "CircleType"},
                             false, false, {}, {},
                             qname{"http://example.com/test", "Shape"}));

  s.add_element(element_decl(qname{"http://example.com/test", "Square"},
                             qname{"http://example.com/test", "SquareType"},
                             false, false, {}, {},
                             qname{"http://example.com/test", "Shape"}));

  // Types for the substitution members
  content_type empty_ct;
  empty_ct.kind = content_kind::empty;

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ShapeType"},
                                  true, false, content_type{}));

  std::vector<particle> circle_particles;
  circle_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "radius"}, qname{xs_ns, "double"}));
  model_group circle_seq(compositor_kind::sequence,
                         std::move(circle_particles));
  content_type circle_ct(content_kind::element_only,
                         complex_content(qname{},
                                         derivation_method::restriction,
                                         std::move(circle_seq)));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "CircleType"}, false, false,
                   std::move(circle_ct)));

  std::vector<particle> square_particles;
  square_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "side"}, qname{xs_ns, "double"}));
  model_group square_seq(compositor_kind::sequence,
                         std::move(square_particles));
  content_type square_ct(content_kind::element_only,
                         complex_content(qname{},
                                         derivation_method::restriction,
                                         std::move(square_seq)));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "SquareType"}, false, false,
                   std::move(square_ct)));

  // Complex type using the abstract element ref
  std::vector<particle> container_particles;
  container_particles.emplace_back(
      element_ref{qname{"http://example.com/test", "Shape"}},
      occurrence{1, unbounded});
  model_group container_seq(compositor_kind::sequence,
                            std::move(container_particles));
  content_type container_ct(content_kind::element_only,
                            complex_content(qname{},
                                            derivation_method::restriction,
                                            std::move(container_seq)));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "DrawingType"}, false,
                   false, std::move(container_ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "drawing_type");
  REQUIRE(st != nullptr);
  REQUIRE(st->fields.size() == 1);
  // The field should be a vector of variant of concrete types
  CHECK(st->fields[0].type.find("std::vector<std::variant<") !=
        std::string::npos);
  CHECK(st->fields[0].type.find("circle_type") != std::string::npos);
  CHECK(st->fields[0].type.find("square_type") != std::string::npos);
}

// TDD step 8: Recursive type (self-referencing) -> unique_ptr
TEST_CASE("recursive self-reference uses unique_ptr", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // A tree node that references itself
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "value"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "left"},
                   qname{"http://example.com/test", "TreeNode"}),
      occurrence{0, 1});
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "right"},
                   qname{"http://example.com/test", "TreeNode"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "TreeNode"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "tree_node");
  REQUIRE(st != nullptr);
  auto* left = find_field(*st, "left");
  REQUIRE(left != nullptr);
  CHECK(left->type == "std::unique_ptr<tree_node>");
  auto* right = find_field(*st, "right");
  REQUIRE(right != nullptr);
  CHECK(right->type == "std::unique_ptr<tree_node>");
}

// TDD step 9: Recursive via vector -> no unique_ptr needed
TEST_CASE("recursive via vector uses plain vector", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "children"},
                   qname{"http://example.com/test", "FolderType"}),
      occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "FolderType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "folder_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "children");
  REQUIRE(f != nullptr);
  // Vector provides indirection, no unique_ptr needed
  CHECK(f->type == "std::vector<folder_type>");
}

// TDD step 10: Mixed content
TEST_CASE("mixed content generates variant vector", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "bold"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(
      qname{"http://example.com/test", "italic"}, qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::mixed,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "RichTextType"}, false,
                   true, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "rich_text_type");
  REQUIRE(st != nullptr);
  // Mixed content should have a single content field
  auto* f = find_field(*st, "content");
  REQUIRE(f != nullptr);
  CHECK(f->type.find("std::vector<std::variant<std::string") !=
        std::string::npos);
}

// TDD step 11: Element default value -> field initializer
TEST_CASE("element default value becomes field initializer", "[codegen]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "priority"},
                   qname{xs_ns, "int"}, false, false,
                   std::string("5"))); // default_value = "5"
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "TaskType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "task_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "priority");
  REQUIRE(f != nullptr);
  CHECK(f->default_value == "5");
}

// ===== Group 2: Serialization Codegen =====

// Serialization TDD step 1: Complex type with sequence -> write_ function
TEST_CASE("codegen generates write function for sequence type",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "age"},
                                      qname{xs_ns, "int"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "PersonType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);

  auto* fn = find_function(files[0], "write_person_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "void");
  CHECK(fn->parameters.find("const person_type&") != std::string::npos);
  CHECK(fn->parameters.find("xb::xml_writer&") != std::string::npos);
  // Body should contain write_simple calls for each element
  CHECK(fn->body.find("write_simple") != std::string::npos);
  CHECK(fn->body.find("\"name\"") != std::string::npos);
  CHECK(fn->body.find("\"age\"") != std::string::npos);
}

// Serialization TDD step 2: Required element -> unconditional write_simple
TEST_CASE("write function required element is unconditional",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_simple");
  REQUIRE(fn != nullptr);
  // Required element: no "if" guard
  CHECK(fn->body.find("xb::write_simple(writer") != std::string::npos);
  CHECK(fn->body.find("value.name") != std::string::npos);
}

// Serialization TDD step 3: Optional element -> conditional write
TEST_CASE("write function optional element is conditional",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "note"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "WithOpt"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_with_opt");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("if (value.note)") != std::string::npos);
}

// Serialization TDD step 4: Unbounded element -> for loop
TEST_CASE("write function unbounded element uses for loop",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ListType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_list_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("for (") != std::string::npos);
  CHECK(fn->body.find("value.item") != std::string::npos);
}

// Serialization TDD step 5: Required attribute -> writer.attribute()
TEST_CASE("write function required attribute", "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "WithAttr"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_with_attr");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("writer.attribute(") != std::string::npos);
  CHECK(fn->body.find("xb::format(value.id)") != std::string::npos);
}

// Serialization TDD step 6: Optional attribute -> conditional
TEST_CASE("write function optional attribute is conditional",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "tag"}, qname{xs_ns, "string"}, false, {}, {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "WithOptAttr"}, false,
                   false, std::move(ct), std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_with_opt_attr");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("if (value.tag)") != std::string::npos);
}

// Serialization TDD step 7: Enum attribute -> to_string
TEST_CASE("write function enum attribute uses to_string",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/test", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/test", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "WithEnum"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_with_enum");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("to_string(value.side)") != std::string::npos);
}

// Serialization TDD step 8: Choice (variant) -> std::visit
TEST_CASE("write function choice uses std::visit", "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "text"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "code"},
                                      qname{xs_ns, "int"}));
  model_group choice(compositor_kind::choice, std::move(particles));

  content_type ct(content_kind::element_only,
                  complex_content(qname{}, derivation_method::restriction,
                                  std::move(choice)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ChoiceType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_choice_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("std::visit") != std::string::npos);
}

// Serialization TDD step 9: Simple content -> writer.characters()
TEST_CASE("write function simple content uses characters",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct(
      content_kind::simple,
      simple_content{qname{xs_ns, "string"}, derivation_method::extension, {}});

  std::vector<attribute_use> attrs;
  attrs.push_back(
      {qname{"", "currency"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "MoneyType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_money_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("writer.characters(") != std::string::npos);
  CHECK(fn->body.find("value.value") != std::string::npos);
}

// Serialization TDD step 10: Extension -> writes base + derived fields
TEST_CASE("write function extension writes base and derived fields",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Base type
  std::vector<particle> base_particles;
  base_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "name"}, qname{xs_ns, "string"}));
  model_group base_seq(compositor_kind::sequence, std::move(base_particles));
  content_type base_ct(content_kind::element_only,
                       complex_content(qname{}, derivation_method::restriction,
                                       std::move(base_seq)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, std::move(base_ct)));

  // Derived type
  std::vector<particle> derived_particles;
  derived_particles.emplace_back(element_decl(
      qname{"http://example.com/test", "age"}, qname{xs_ns, "int"}));
  model_group derived_seq(compositor_kind::sequence,
                          std::move(derived_particles));
  content_type derived_ct(
      content_kind::element_only,
      complex_content(qname{"http://example.com/test", "BaseType"},
                      derivation_method::extension, std::move(derived_seq)));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "DerivedType"}, false,
                   false, std::move(derived_ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_derived_type");
  REQUIRE(fn != nullptr);
  // Should write both base field 'name' and derived field 'age'
  CHECK(fn->body.find("value.name") != std::string::npos);
  CHECK(fn->body.find("value.age") != std::string::npos);
}

// Serialization TDD step 11: Wildcard (xs:any) -> any_element write
TEST_CASE("write function wildcard delegates to any_element write",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(wildcard{});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ExtType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_ext_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find(".write(writer)") != std::string::npos);
}

// Serialization TDD step 12: Recursive type -> null check + dereference
TEST_CASE("write function recursive type checks null",
          "[codegen][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "value"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "left"},
                   qname{"http://example.com/test", "TreeNode"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "TreeNode"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_tree_node");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("if (value.left)") != std::string::npos);
  CHECK(fn->body.find("write_tree_node(*value.left") != std::string::npos);
}

// ===== Group 3: Deserialization Codegen =====

// Deserialization TDD step 1: Complex type with sequence -> read_ function
TEST_CASE("codegen generates read function for sequence type",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "age"},
                                      qname{xs_ns, "int"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "PersonType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);

  auto* fn = find_function(files[0], "read_person_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "person_type");
  CHECK(fn->parameters.find("xb::xml_reader&") != std::string::npos);
  // Body should dispatch by element name
  CHECK(fn->body.find("reader.name()") != std::string::npos);
  CHECK(fn->body.find("\"name\"") != std::string::npos);
  CHECK(fn->body.find("\"age\"") != std::string::npos);
  CHECK(fn->body.find("read_simple") != std::string::npos);
}

// Deserialization TDD step 2: Required element
TEST_CASE("read function required element assigns field",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_simple");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("result.name = xb::read_simple<std::string>(reader)") !=
        std::string::npos);
}

// Deserialization TDD step 3: Unbounded element -> push_back
TEST_CASE("read function unbounded element uses push_back",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ListType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_list_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("result.item.push_back(") != std::string::npos);
}

// Deserialization TDD step 4: Required attribute
TEST_CASE("read function required attribute parses from attr",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "WithAttr"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_with_attr");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("result.id = xb::parse<std::string>") !=
        std::string::npos);
  CHECK(fn->body.find("attribute_value") != std::string::npos);
}

// Deserialization TDD step 5: Optional attribute
TEST_CASE("read function optional attribute checks empty",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "tag"}, qname{xs_ns, "string"}, false, {}, {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "WithOptAttr"}, false,
                   false, std::move(ct), std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_with_opt_attr");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("if (!") != std::string::npos);
  CHECK(fn->body.find(".empty()") != std::string::npos);
}

// Deserialization TDD step 6: Enum attribute -> from_string
TEST_CASE("read function enum attribute uses from_string",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/test", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  content_type ct;
  ct.kind = content_kind::empty;

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/test", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "WithEnum"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_with_enum");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("_from_string(") != std::string::npos);
}

// Deserialization TDD step 7: Choice -> element name selects variant
TEST_CASE("read function choice dispatches by element name",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "text"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "code"},
                                      qname{xs_ns, "int"}));
  model_group choice(compositor_kind::choice, std::move(particles));

  content_type ct(content_kind::element_only,
                  complex_content(qname{}, derivation_method::restriction,
                                  std::move(choice)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ChoiceType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_choice_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("result.choice =") != std::string::npos);
  CHECK(fn->body.find("\"text\"") != std::string::npos);
  CHECK(fn->body.find("\"code\"") != std::string::npos);
}

// Deserialization TDD step 8: Simple content -> parse from text
TEST_CASE("read function simple content parses text",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  content_type ct(
      content_kind::simple,
      simple_content{qname{xs_ns, "string"}, derivation_method::extension, {}});

  std::vector<attribute_use> attrs;
  attrs.push_back(
      {qname{"", "currency"}, qname{xs_ns, "string"}, true, {}, {}});

  s.add_complex_type(complex_type(qname{"http://example.com/test", "MoneyType"},
                                  false, false, std::move(ct),
                                  std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_money_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("result.value = xb::parse<") != std::string::npos);
  CHECK(fn->body.find("xb::read_text(reader)") != std::string::npos);
}

// Deserialization TDD step 9: Unknown elements -> skip_element
TEST_CASE("read function skips unknown elements",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_simple");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::skip_element(reader)") != std::string::npos);
}

// Deserialization TDD step 10: Recursive type -> make_unique
TEST_CASE("read function recursive type uses make_unique",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "value"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "left"},
                   qname{"http://example.com/test", "TreeNode"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "TreeNode"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_tree_node");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("std::make_unique<tree_node>") != std::string::npos);
  CHECK(fn->body.find("read_tree_node(reader)") != std::string::npos);
}

// Deserialization TDD step 11: Wildcard -> any_element(reader)
TEST_CASE("read function wildcard uses any_element",
          "[codegen][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(wildcard{});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ExtType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_ext_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::any_element(reader)") != std::string::npos);
}

// ===== Group: Split Mode =====

TEST_CASE("split mode produces two files per namespace", "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();
  REQUIRE(files.size() == 2);

  // One header, one source
  bool found_header = false;
  bool found_source = false;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) found_header = true;
    if (f.kind == file_kind::source) found_source = true;
  }
  CHECK(found_header);
  CHECK(found_source);
}

TEST_CASE("split mode header has file_kind::header", "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  const cpp_file* hpp = nullptr;
  const cpp_file* cpp = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) hpp = &f;
    if (f.kind == file_kind::source) cpp = &f;
  }
  REQUIRE(hpp != nullptr);
  REQUIRE(cpp != nullptr);
  CHECK(hpp->filename.find(".hpp") != std::string::npos);
  CHECK(cpp->filename.find(".cpp") != std::string::npos);
}

TEST_CASE("split mode read/write functions are not inline",
          "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  // Find the header file and check functions are non-inline
  const cpp_file* hpp = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) hpp = &f;
  }
  REQUIRE(hpp != nullptr);

  auto* read_fn = find_function(*hpp, "read_simple");
  auto* write_fn = find_function(*hpp, "write_simple");
  REQUIRE(read_fn != nullptr);
  REQUIRE(write_fn != nullptr);
  CHECK_FALSE(read_fn->is_inline);
  CHECK_FALSE(write_fn->is_inline);
}

TEST_CASE("split mode header omits runtime includes", "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  const cpp_file* hpp = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) hpp = &f;
  }
  REQUIRE(hpp != nullptr);

  // Header includes xml_reader/xml_writer (needed for declaration parameter
  // types) but NOT xml_value/xml_io (only needed for function bodies)
  bool found_reader = false;
  bool found_writer = false;
  for (const auto& inc : hpp->includes) {
    if (inc.path.find("xml_reader") != std::string::npos) found_reader = true;
    if (inc.path.find("xml_writer") != std::string::npos) found_writer = true;
    CHECK(inc.path.find("xml_value") == std::string::npos);
    CHECK(inc.path.find("xml_io") == std::string::npos);
  }
  CHECK(found_reader);
  CHECK(found_writer);
}

TEST_CASE("split mode source includes self header and runtime",
          "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  const cpp_file* cpp_f = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::source) cpp_f = &f;
  }
  REQUIRE(cpp_f != nullptr);

  // Source should include the self header
  bool has_self = false;
  bool has_runtime = false;
  for (const auto& inc : cpp_f->includes) {
    if (inc.path.find("test.hpp") != std::string::npos) has_self = true;
    if (inc.path.find("xml_reader") != std::string::npos ||
        inc.path.find("xml_io") != std::string::npos)
      has_runtime = true;
  }
  CHECK(has_self);
  CHECK(has_runtime);
}

TEST_CASE("header_only mode produces one file unchanged", "[codegen][split]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::header_only;
  codegen gen(ss, types, opts);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  CHECK(files[0].kind == file_kind::header);

  // Functions should be inline
  auto* fn = find_function(files[0], "read_simple");
  REQUIRE(fn != nullptr);
  CHECK(fn->is_inline);
}

// ===== Group: File-per-type Mode =====

TEST_CASE("file_per_type produces per-type headers + umbrella + source",
          "[codegen][file_per_type]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  // Enum type
  facet_set facets;
  facets.enumeration = {"Red", "Green", "Blue"};
  s.add_simple_type(simple_type(qname{"http://example.com/test", "Color"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, facets));

  // Two complex types
  std::vector<particle> p1;
  p1.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs, "int"}));
  model_group seq1(compositor_kind::sequence, std::move(p1));
  content_type ct1(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   std::move(seq1)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "PointType"},
                                  false, false, std::move(ct1)));

  std::vector<particle> p2;
  p2.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                               qname{xs, "string"}));
  model_group seq2(compositor_kind::sequence, std::move(p2));
  content_type ct2(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   std::move(seq2)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "LabelType"},
                                  false, false, std::move(ct2)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::file_per_type;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  // Should have: color.hpp, point_type.hpp, label_type.hpp, test.hpp
  // (umbrella), test.cpp
  int header_count = 0;
  int source_count = 0;
  for (const auto& f : files) {
    if (f.kind == file_kind::header) ++header_count;
    if (f.kind == file_kind::source) ++source_count;
  }
  // 3 per-type headers + 1 umbrella = 4 headers, 1 source
  CHECK(header_count == 4);
  CHECK(source_count == 1);
}

TEST_CASE("file_per_type umbrella includes all per-type headers",
          "[codegen][file_per_type]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> p1;
  p1.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs, "int"}));
  model_group seq1(compositor_kind::sequence, std::move(p1));
  content_type ct1(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   std::move(seq1)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "PointType"},
                                  false, false, std::move(ct1)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::file_per_type;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  // Find umbrella (stem.hpp with no type-specific declarations)
  const cpp_file* umbrella = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::header && f.filename == "test.hpp") umbrella = &f;
  }
  REQUIRE(umbrella != nullptr);

  // Umbrella should include the per-type header
  bool has_point = false;
  for (const auto& inc : umbrella->includes) {
    if (inc.path.find("test_point_type.hpp") != std::string::npos)
      has_point = true;
  }
  CHECK(has_point);
}

TEST_CASE("file_per_type source includes umbrella + runtime",
          "[codegen][file_per_type]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> p1;
  p1.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs, "int"}));
  model_group seq1(compositor_kind::sequence, std::move(p1));
  content_type ct1(content_kind::element_only,
                   complex_content(qname{}, derivation_method::restriction,
                                   std::move(seq1)));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "PointType"},
                                  false, false, std::move(ct1)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.mode = output_mode::file_per_type;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  const cpp_file* src = nullptr;
  for (const auto& f : files) {
    if (f.kind == file_kind::source) src = &f;
  }
  REQUIRE(src != nullptr);

  bool has_umbrella = false;
  bool has_runtime = false;
  for (const auto& inc : src->includes) {
    if (inc.path.find("test.hpp") != std::string::npos) has_umbrella = true;
    if (inc.path.find("xml_reader") != std::string::npos) has_runtime = true;
  }
  CHECK(has_umbrella);
  CHECK(has_runtime);
}

// ===== Group: Open Content =====

TEST_CASE("open content: type with open content gets open_content field",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{
      open_content_mode::interleave,
      wildcard{wildcard_ns_constraint::other, {}, process_contents::lax}};

  s.add_complex_type(complex_type(qname{"http://example.com/test", "FlexType"},
                                  false, false, std::move(ct), {}, {}, {},
                                  std::move(oc)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "flex_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "open_content");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<xb::any_element>");
}

TEST_CASE("open content: type with explicit wildcard + open content: no "
          "duplicate open_content field",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  particles.emplace_back(wildcard{});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{open_content_mode::interleave, wildcard{}};

  s.add_complex_type(complex_type(qname{"http://example.com/test", "DupType"},
                                  false, false, std::move(ct), {}, {}, {},
                                  std::move(oc)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "dup_type");
  REQUIRE(st != nullptr);
  // Should have 'any' field from wildcard, but no 'open_content' field
  auto* any_f = find_field(*st, "any");
  CHECK(any_f != nullptr);
  auto* oc_f = find_field(*st, "open_content");
  CHECK(oc_f == nullptr);
}

TEST_CASE("open content: defaultOpenContent adds field to type without own "
          "openContent",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.set_default_open_content(
      open_content{open_content_mode::interleave, wildcard{}});

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "DefType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "def_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "open_content");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<xb::any_element>");
}

TEST_CASE("open content: mode=none opts out of schema default",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.set_default_open_content(
      open_content{open_content_mode::interleave, wildcard{}});

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{open_content_mode::none, wildcard{}};

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ClosedType"}, false, false,
                   std::move(ct), {}, {}, {}, std::move(oc)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "closed_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "open_content");
  CHECK(f == nullptr);
}

TEST_CASE("open content: appliesToEmpty=false, empty type: no open_content "
          "field",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.set_default_open_content(
      open_content{open_content_mode::interleave, wildcard{}}, false);

  content_type ct;
  ct.kind = content_kind::empty;

  s.add_complex_type(complex_type(qname{"http://example.com/test", "EmptyType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "empty_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "open_content");
  CHECK(f == nullptr);
}

TEST_CASE("open content: appliesToEmpty=true, empty type: gets open_content "
          "field",
          "[codegen][open_content]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.set_default_open_content(
      open_content{open_content_mode::interleave, wildcard{}}, true);

  content_type ct;
  ct.kind = content_kind::empty;

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "EmptyOpenType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "empty_open_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "open_content");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<xb::any_element>");
}

// ===== Open Content: Serialization & Deserialization =====

TEST_CASE("open content: read function captures into open_content field",
          "[codegen][open_content][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{
      open_content_mode::interleave,
      wildcard{wildcard_ns_constraint::other, {}, process_contents::lax}};

  s.add_complex_type(complex_type(qname{"http://example.com/test", "FlexType"},
                                  false, false, std::move(ct), {}, {}, {},
                                  std::move(oc)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_flex_type");
  REQUIRE(fn != nullptr);
  // Should capture unknown elements into open_content, not skip_element
  CHECK(fn->body.find(
            "result.open_content.emplace_back(xb::any_element(reader))") !=
        std::string::npos);
  CHECK(fn->body.find("xb::skip_element(reader)") == std::string::npos);
}

TEST_CASE("open content: read function without open content still skips",
          "[codegen][open_content][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "PlainType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_plain_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::skip_element(reader)") != std::string::npos);
  CHECK(fn->body.find("open_content") == std::string::npos);
}

TEST_CASE("open content: write function writes open_content elements",
          "[codegen][open_content][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "data"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  open_content oc{
      open_content_mode::suffix,
      wildcard{wildcard_ns_constraint::any, {}, process_contents::lax}};

  s.add_complex_type(complex_type(qname{"http://example.com/test", "FlexType"},
                                  false, false, std::move(ct), {}, {}, {},
                                  std::move(oc)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_flex_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("for (const auto& e : value.open_content)") !=
        std::string::npos);
  CHECK(fn->body.find("e.write(writer)") != std::string::npos);
}

TEST_CASE("open content: empty type with open content gets read loop",
          "[codegen][open_content][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.set_default_open_content(
      open_content{open_content_mode::interleave, wildcard{}}, true);

  content_type ct;
  ct.kind = content_kind::empty;

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "EmptyOpenType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_empty_open_type");
  REQUIRE(fn != nullptr);
  // Should have a read loop that captures unknown elements
  CHECK(fn->body.find("reader.read()") != std::string::npos);
  CHECK(fn->body.find(
            "result.open_content.emplace_back(xb::any_element(reader))") !=
        std::string::npos);
}

// ===== Conditional Type Assignment: Type Generation =====

TEST_CASE("CTA element with 2 alternatives generates variant field",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'car'"),
       qname{"http://example.com/test", "CarType"}},
      {std::string("@kind = 'truck'"),
       qname{"http://example.com/test", "TruckType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/test", "vehicle"},
      qname{"http://example.com/test", "VehicleType"}, false, false,
      std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  // Add the complex types so resolver can resolve them
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "VehicleType"}, false,
                   false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "CarType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "TruckType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "container_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "vehicle");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::variant<car_type, truck_type>");
}

TEST_CASE("CTA element with alternatives + default: all types in variant",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'a'"), qname{"http://example.com/test", "AType"}},
      {std::string("@kind = 'b'"), qname{"http://example.com/test", "BType"}},
      {std::nullopt, qname{"http://example.com/test", "BaseType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "item"},
                   qname{"http://example.com/test", "BaseType"}, false, false,
                   std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "container_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "item");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::variant<a_type, b_type, base_type>");
}

TEST_CASE("CTA element optional occurrence wraps variant in optional",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@k = '1'"), qname{"http://example.com/test", "AType"}},
      {std::nullopt, qname{"http://example.com/test", "BType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{"http://example.com/test", "BType"},
                                      false, false, std::nullopt, std::nullopt,
                                      std::nullopt, std::move(alts)),
                         occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "container_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "item");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::optional<std::variant<a_type, b_type>>");
}

TEST_CASE("CTA element unbounded occurrence wraps variant in vector",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@k = '1'"), qname{"http://example.com/test", "AType"}},
      {std::nullopt, qname{"http://example.com/test", "BType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{"http://example.com/test", "BType"},
                                      false, false, std::nullopt, std::nullopt,
                                      std::nullopt, std::move(alts)),
                         occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "container_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "item");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::vector<std::variant<a_type, b_type>>");
}

TEST_CASE("element without CTA alternatives: unchanged single type field",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "name"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Simple"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "simple");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "name");
  REQUIRE(f != nullptr);
  CHECK(f->type == "std::string");
}

// --- Group 4: CTA serialization / deserialization ---

TEST_CASE("write function for CTA element uses std::visit dispatch",
          "[codegen][cta][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'car'"),
       qname{"http://example.com/test", "CarType"}},
      {std::string("@kind = 'truck'"),
       qname{"http://example.com/test", "TruckType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/test", "vehicle"},
      qname{"http://example.com/test", "VehicleType"}, false, false,
      std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "VehicleType"}, false,
                   false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "CarType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "TruckType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_container_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("std::visit") != std::string::npos);
  CHECK(fn->body.find("car_type") != std::string::npos);
  CHECK(fn->body.find("truck_type") != std::string::npos);
  CHECK(fn->body.find("\"vehicle\"") != std::string::npos);
}

TEST_CASE("read function for CTA element dispatches on attribute value",
          "[codegen][cta][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'car'"),
       qname{"http://example.com/test", "CarType"}},
      {std::string("@kind = 'truck'"),
       qname{"http://example.com/test", "TruckType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/test", "vehicle"},
      qname{"http://example.com/test", "VehicleType"}, false, false,
      std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "VehicleType"}, false,
                   false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "CarType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "TruckType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_container_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("attribute_value") != std::string::npos);
  CHECK(fn->body.find("\"kind\"") != std::string::npos);
  CHECK(fn->body.find("\"car\"") != std::string::npos);
  CHECK(fn->body.find("\"truck\"") != std::string::npos);
  CHECK(fn->body.find("read_car_type") != std::string::npos);
  CHECK(fn->body.find("read_truck_type") != std::string::npos);
}

TEST_CASE("read function for CTA with default alternative has else branch",
          "[codegen][cta][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'a'"), qname{"http://example.com/test", "AType"}},
      {std::nullopt, qname{"http://example.com/test", "BaseType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "item"},
                   qname{"http://example.com/test", "BaseType"}, false, false,
                   std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_container_type");
  REQUIRE(fn != nullptr);
  // CTA dispatch on attribute value
  CHECK(fn->body.find("attribute_value") != std::string::npos);
  CHECK(fn->body.find("\"kind\"") != std::string::npos);
  CHECK(fn->body.find("read_a_type") != std::string::npos);
  // Default alternative generates else branch
  CHECK(fn->body.find("else {") != std::string::npos);
  CHECK(fn->body.find("read_base_type") != std::string::npos);
}

// --- Fix 2: Single-type deduplication ---

TEST_CASE("CTA all alternatives same type: unwrapped single type field",
          "[codegen][cta]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Two alternatives that both resolve to the same type
  std::vector<type_alternative> alts = {
      {std::string("@kind = 'a'"), qname{"http://example.com/test", "AType"}},
      {std::string("@kind = 'b'"), qname{"http://example.com/test", "AType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{"http://example.com/test", "AType"},
                                      false, false, std::nullopt, std::nullopt,
                                      std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);
  auto* st = find_struct(files[0], "container_type");
  REQUIRE(st != nullptr);
  auto* f = find_field(*st, "item");
  REQUIRE(f != nullptr);
  // Single unique type should NOT be wrapped in variant
  CHECK(f->type == "a_type");
}

TEST_CASE("CTA all alternatives same type: write uses normal path",
          "[codegen][cta][serialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'a'"), qname{"http://example.com/test", "AType"}},
      {std::string("@kind = 'b'"), qname{"http://example.com/test", "AType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{"http://example.com/test", "AType"},
                                      false, false, std::nullopt, std::nullopt,
                                      std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "write_container_type");
  REQUIRE(fn != nullptr);
  // Should NOT use std::visit — single type uses normal write path
  CHECK(fn->body.find("std::visit") == std::string::npos);
  CHECK(fn->body.find("write_a_type") != std::string::npos);
}

TEST_CASE("CTA all alternatives same type: read uses normal path",
          "[codegen][cta][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<type_alternative> alts = {
      {std::string("@kind = 'a'"), qname{"http://example.com/test", "AType"}},
      {std::string("@kind = 'b'"), qname{"http://example.com/test", "AType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{"http://example.com/test", "AType"},
                                      false, false, std::nullopt, std::nullopt,
                                      std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "AType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_container_type");
  REQUIRE(fn != nullptr);
  // Should NOT dispatch on attribute — single type uses normal read path
  CHECK(fn->body.find("attribute_value") == std::string::npos);
  CHECK(fn->body.find("read_a_type") != std::string::npos);
}

TEST_CASE("CTA unsupported XPath emits warning comment in read function",
          "[codegen][cta][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // One supported, one unsupported XPath expression
  std::vector<type_alternative> alts = {
      {std::string("@kind = 'car'"),
       qname{"http://example.com/test", "CarType"}},
      {std::string("@a and @b"), qname{"http://example.com/test", "TruckType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/test", "vehicle"},
      qname{"http://example.com/test", "VehicleType"}, false, false,
      std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "VehicleType"}, false,
                   false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "CarType"},
                                  false, false, content_type{}));
  s.add_complex_type(complex_type(qname{"http://example.com/test", "TruckType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_container_type");
  REQUIRE(fn != nullptr);
  // Warning comment about unsupported XPath
  CHECK(fn->body.find("WARNING") != std::string::npos);
  CHECK(fn->body.find("@a and @b") != std::string::npos);
}

TEST_CASE("CTA default-only alternative: unconditional read",
          "[codegen][cta][deserialization]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Only a default alternative (no test expression)
  std::vector<type_alternative> alts = {
      {std::nullopt, qname{"http://example.com/test", "DefaultType"}},
  };

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "item"},
                   qname{"http://example.com/test", "BaseType"}, false, false,
                   std::nullopt, std::nullopt, std::nullopt, std::move(alts)));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "BaseType"},
                                  false, false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "DefaultType"}, false,
                   false, content_type{}));
  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "ContainerType"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "read_container_type");
  REQUIRE(fn != nullptr);
  // Should read the default type unconditionally (else branch without if)
  CHECK(fn->body.find("read_default_type") != std::string::npos);
  // Should NOT have attribute_value dispatch since there are no test
  // expressions
  CHECK(fn->body.find("attribute_value") == std::string::npos);
}

// ===== Assertion validation function generation =====

TEST_CASE("complex type with assertion generates validate function",
          "[codegen][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "start"},
                                      qname{xs_ns, "int"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "end"},
                                      qname{xs_ns, "int"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "DateRange"}, false, false,
      std::move(ct), {}, {}, std::nullopt, std::nullopt, {{"end >= start"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_date_range");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "bool");
  CHECK(fn->parameters.find("const date_range&") != std::string::npos);
  CHECK(fn->body.find("value.end >= value.start") != std::string::npos);
}

TEST_CASE("complex type without assertions: no validate function",
          "[codegen][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "x"},
                                      qname{xs_ns, "string"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "PlainType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_plain_type");
  CHECK(fn == nullptr);
}

TEST_CASE("complex type with multiple assertions: &&-chained",
          "[codegen][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "min"},
                                      qname{xs_ns, "int"}));
  particles.emplace_back(element_decl(qname{"http://example.com/test", "max"},
                                      qname{xs_ns, "int"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "RangeType"},
                                  false, false, std::move(ct), {}, {},
                                  std::nullopt, std::nullopt,
                                  {{"max >= min"}, {"min >= 0"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_range_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value.max >= value.min") != std::string::npos);
  CHECK(fn->body.find("value.min >= 0") != std::string::npos);
  CHECK(fn->body.find("&&") != std::string::npos);
}

TEST_CASE("simple type with assertion generates validate function",
          "[codegen][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  s.add_simple_type(simple_type(qname{"http://example.com/test", "PositiveInt"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, {}, std::nullopt, {},
                                {{"$value > 0"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_positive_int");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "bool");
  CHECK(fn->body.find("value > 0") != std::string::npos);
}

TEST_CASE("unsupported XPath in assertion: WARNING comment, returns true",
          "[codegen][assertion]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "x"},
                                      qname{xs_ns, "string"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "FancyType"},
                                  false, false, std::move(ct), {}, {},
                                  std::nullopt, std::nullopt,
                                  {{"fn:string-length($value) > 5"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_fancy_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("WARNING") != std::string::npos);
  CHECK(fn->body.find("return true") != std::string::npos);
}

// ===== Facet validation: range facets =====

TEST_CASE("simple type with min_inclusive facet generates validate function",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_inclusive = "0";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "NonNegInt"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_non_neg_int");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "bool");
  CHECK(fn->body.find("value >= xb::parse<xb::integer>(\"0\")") !=
        std::string::npos);
}

TEST_CASE("simple type with max_exclusive facet generates validate function",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.max_exclusive = "100";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "Under100"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "int"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_under100");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value < xb::parse<int32_t>(\"100\")") !=
        std::string::npos);
}

TEST_CASE("simple type with both min and max inclusive facets",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_inclusive = "1";
  facets.max_inclusive = "10";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "OneToTen"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "int"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_one_to_ten");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value >= xb::parse<int32_t>(\"1\")") !=
        std::string::npos);
  CHECK(fn->body.find("value <= xb::parse<int32_t>(\"10\")") !=
        std::string::npos);
  CHECK(fn->body.find("&&") != std::string::npos);
}

TEST_CASE("simple type with range facet AND assertion: both present",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_inclusive = "0";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "PosChecked"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets, std::nullopt,
                                {}, {{"$value > 0"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_pos_checked");
  REQUIRE(fn != nullptr);
  // Both assertion and facet should be present
  CHECK(fn->body.find("value > 0") != std::string::npos);
  CHECK(fn->body.find("value >= xb::parse<xb::integer>(\"0\")") !=
        std::string::npos);
  CHECK(fn->body.find("&&") != std::string::npos);
}

// ===== Facet validation: length facets =====

TEST_CASE("simple type with length facet generates validate function",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.length = 5;

  s.add_simple_type(simple_type(qname{"http://example.com/test", "Code5"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_code5");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::format(value).size() == 5") != std::string::npos);
}

TEST_CASE("simple type with min_length facet generates validate function",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_length = 2;

  s.add_simple_type(simple_type(qname{"http://example.com/test", "AtLeast2"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_at_least2");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::format(value).size() >= 2") != std::string::npos);
}

TEST_CASE("simple type with max_length facet generates validate function",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.max_length = 10;

  s.add_simple_type(simple_type(qname{"http://example.com/test", "AtMost10"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_at_most10");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("xb::format(value).size() <= 10") != std::string::npos);
}

TEST_CASE("string type with length facet uses value.size() optimization",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.length = 3;

  s.add_simple_type(simple_type(qname{"http://example.com/test", "StrCode"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_str_code");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value.size() == 3") != std::string::npos);
  // Should NOT use xb::format for string types
  CHECK(fn->body.find("xb::format") == std::string::npos);
}

// ===== Facet validation: pattern facet =====

TEST_CASE("simple type with pattern facet generates regex_match check",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.pattern = "[A-Z]{3}";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "CurrCode"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_curr_code");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("std::regex_match(value, std::regex(\"^[A-Z]{3}$\"))") !=
        std::string::npos);
}

TEST_CASE("non-string type with pattern uses xb::format", "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.pattern = "[0-9]+";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "DigitsOnly"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_digits_only");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find(
            "std::regex_match(xb::format(value), std::regex(\"^[0-9]+$\"))") !=
        std::string::npos);
}

TEST_CASE("pattern facet adds regex include to generated file",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.pattern = "[A-Z]+";

  s.add_simple_type(simple_type(qname{"http://example.com/test", "UpperOnly"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "string"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  bool has_regex_include = false;
  for (const auto& inc : files[0].includes) {
    if (inc.path == "<regex>") has_regex_include = true;
  }
  CHECK(has_regex_include);
}

// ===== Facet validation: complex type with simple content =====

TEST_CASE("complex type with simple content and min_inclusive facet",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_inclusive = "0";

  content_type ct(content_kind::simple,
                  simple_content{qname{xs_ns, "int"},
                                 derivation_method::restriction, facets});

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "PriceType"}, false, false,
                   std::move(ct),
                   {attribute_use{qname{"", "currency"}, qname{xs_ns, "string"},
                                  true, std::nullopt, std::nullopt}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_price_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->return_type == "bool");
  CHECK(fn->parameters.find("const price_type&") != std::string::npos);
  CHECK(fn->body.find("value.value >= xb::parse<int32_t>(\"0\")") !=
        std::string::npos);
}

TEST_CASE("complex type with simple content facets AND assertions",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.max_inclusive = "100";

  content_type ct(content_kind::simple,
                  simple_content{qname{xs_ns, "int"},
                                 derivation_method::restriction, facets});

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "ScoreType"}, false, false,
      std::move(ct), {}, {}, std::nullopt, std::nullopt, {{"value >= 0"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_score_type");
  REQUIRE(fn != nullptr);
  // Both assertion and facet check should be present
  CHECK(fn->body.find("value.value >= 0") != std::string::npos);
  CHECK(fn->body.find("value.value <= xb::parse<int32_t>(\"100\")") !=
        std::string::npos);
  CHECK(fn->body.find("&&") != std::string::npos);
}

TEST_CASE("complex type without simple content: no facet checks",
          "[codegen][facet]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "x"},
                                      qname{xs_ns, "string"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "PlainCt"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_plain_ct");
  CHECK(fn == nullptr);
}

// ===== Cardinality validation =====

TEST_CASE("complex type with minOccurs=2 maxOccurs=5 generates size checks",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{xs_ns, "string"}),
                         occurrence{2, 5});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "ListType"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_list_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value.item.size() >= 2") != std::string::npos);
  CHECK(fn->body.find("value.item.size() <= 5") != std::string::npos);
}

TEST_CASE("complex type with minOccurs=1 maxOccurs=unbounded: only min check",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "entry"},
                                      qname{xs_ns, "int"}),
                         occurrence{1, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "NonEmptyList"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_non_empty_list");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value.entry.size() >= 1") != std::string::npos);
  // No max check for unbounded
  CHECK(fn->body.find("value.entry.size() <=") == std::string::npos);
}

TEST_CASE("complex type with minOccurs=0 maxOccurs=3: only max check",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "tag"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, 3});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "TaggedType"}, false, false,
                   std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_tagged_type");
  REQUIRE(fn != nullptr);
  CHECK(fn->body.find("value.tag.size() <= 3") != std::string::npos);
  // No min check for 0
  CHECK(fn->body.find("value.tag.size() >=") == std::string::npos);
}

TEST_CASE("complex type with default cardinality (1,1): no validate function",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "x"},
                                      qname{xs_ns, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/test", "DefaultCard"}, false,
                   false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_default_card");
  CHECK(fn == nullptr);
}

TEST_CASE("complex type with minOccurs=0 maxOccurs=unbounded: no validate",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "item"},
                                      qname{xs_ns, "string"}),
                         occurrence{0, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "FreeList"},
                                  false, false, std::move(ct)));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_free_list");
  CHECK(fn == nullptr);
}

TEST_CASE("cardinality checks combined with assertions",
          "[codegen][cardinality]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/test", "value"},
                                      qname{xs_ns, "int"}),
                         occurrence{1, unbounded});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(
      qname{"http://example.com/test", "CheckedList"}, false, false,
      std::move(ct), {}, {}, std::nullopt, std::nullopt, {{"value >= 0"}}));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen gen(ss, types);
  auto files = gen.generate();

  auto* fn = find_function(files[0], "validate_checked_list");
  REQUIRE(fn != nullptr);
  // Both assertion and cardinality check
  CHECK(fn->body.find("value.value >= 0") != std::string::npos);
  CHECK(fn->body.find("value.value.size() >= 1") != std::string::npos);
  CHECK(fn->body.find("&&") != std::string::npos);
}

// ===== Validation mode configuration =====

TEST_CASE("validation_mode::none suppresses validate functions",
          "[codegen][validation_mode]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  // Type with assertion — would normally generate a validate function
  model_group seq(compositor_kind::sequence);
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/test", "x"}, qname{xs_ns, "int"}));
  seq = model_group(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s.add_complex_type(complex_type(qname{"http://example.com/test", "Checked"},
                                  false, false, std::move(ct), {}, {},
                                  std::nullopt, std::nullopt, {{"x > 0"}}));

  // Type with facets — would also normally generate validate
  facet_set facets;
  facets.min_inclusive = "0";
  s.add_simple_type(simple_type(qname{"http://example.com/test", "NonNeg"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  codegen_options opts;
  opts.validation = validation_mode::none;
  codegen gen(ss, types, opts);
  auto files = gen.generate();

  CHECK(find_function(files[0], "validate_checked") == nullptr);
  CHECK(find_function(files[0], "validate_non_neg") == nullptr);
}

TEST_CASE("validation_mode::on_demand generates validate functions (default)",
          "[codegen][validation_mode]") {
  schema s;
  s.set_target_namespace("http://example.com/test");

  facet_set facets;
  facets.min_inclusive = "0";
  s.add_simple_type(simple_type(qname{"http://example.com/test", "NonNeg"},
                                simple_type_variety::atomic,
                                qname{xs_ns, "integer"}, facets));

  auto ss = make_schema_set(std::move(s));
  auto types = default_types();

  // Default options (on_demand is default)
  codegen gen(ss, types);
  auto files = gen.generate();

  CHECK(find_function(files[0], "validate_non_neg") != nullptr);
}
