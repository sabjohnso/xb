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
