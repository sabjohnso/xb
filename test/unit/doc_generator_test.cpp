// GCC 12-13 emit a false -Wmaybe-uninitialized when constructing
// particle objects (std::variant containing std::unique_ptr) at -O3.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/doc_generator.hpp>
#include <xb/ostream_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

using namespace xb;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
static const std::string test_ns = "http://example.com/test";

static schema_set
make_schema_set(schema s) {
  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();
  return ss;
}

static std::string
generate_xml(const schema_set& schemas, const qname& element_name,
             doc_generator_options opts = {}) {
  std::ostringstream os;
  {
    ostream_writer writer(os);
    doc_generator gen(schemas, opts);
    gen.generate(element_name, writer);
  }
  return os.str();
}

// ---------------------------------------------------------------------------
// Phase A: Skeleton (tests 1-3)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: element not found throws", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);
  auto schemas = make_schema_set(std::move(s));

  REQUIRE_THROWS_AS(generate_xml(schemas, qname{test_ns, "NonExistent"}),
                    std::runtime_error);
}

TEST_CASE("doc_generator: element with xs:string type", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);
  s.add_element(element_decl{qname{test_ns, "Name"}, qname{xs_ns, "string"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Name"});
  REQUIRE(xml == "<Name xmlns=\"http://example.com/test\">string</Name>");
}

TEST_CASE("doc_generator: empty complex type", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  complex_type ct{qname{test_ns, "EmptyType"}, false, false, content_type{}};
  s.add_complex_type(std::move(ct));
  s.add_element(
      element_decl{qname{test_ns, "Empty"}, qname{test_ns, "EmptyType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Empty"});
  REQUIRE(xml == "<Empty xmlns=\"http://example.com/test\"/>");
}

// ---------------------------------------------------------------------------
// Phase B: Content models (tests 4-7)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: sequence of two simple elements", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Build sequence with two elements
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "First"}, qname{xs_ns, "string"}});
  particles.emplace_back(
      element_decl{qname{test_ns, "Second"}, qname{xs_ns, "int"}});
  model_group seq{compositor_kind::sequence, std::move(particles)};

  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};

  complex_type ctype{qname{test_ns, "SeqType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "SeqType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<First>string</First>"
                 "<Second>0</Second>"
                 "</Root>");
}

TEST_CASE("doc_generator: choice group emits first alternative",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "Alpha"}, qname{xs_ns, "string"}});
  particles.emplace_back(
      element_decl{qname{test_ns, "Beta"}, qname{xs_ns, "int"}});
  model_group choice{compositor_kind::choice, std::move(particles)};

  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(choice)};
  content_type ct{content_kind::element_only, std::move(cc)};

  complex_type ctype{qname{test_ns, "ChoiceType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "ChoiceType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Alpha>string</Alpha>"
                 "</Root>");
}

TEST_CASE("doc_generator: minOccurs=0 element skipped", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "Required"}, qname{xs_ns, "string"}});
  particles.emplace_back(
      element_decl{qname{test_ns, "Optional"}, qname{xs_ns, "string"}},
      occurrence{0, 1});
  model_group seq{compositor_kind::sequence, std::move(particles)};

  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};

  complex_type ctype{qname{test_ns, "OptType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "OptType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Required>string</Required>"
                 "</Root>");
}

TEST_CASE("doc_generator: minOccurs=2 element appears twice",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "Item"}, qname{xs_ns, "string"}},
      occurrence{2, unbounded});
  model_group seq{compositor_kind::sequence, std::move(particles)};

  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};

  complex_type ctype{qname{test_ns, "RepType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "RepType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Item>string</Item>"
                 "<Item>string</Item>"
                 "</Root>");
}

// ---------------------------------------------------------------------------
// Phase C: Attributes (tests 8-11)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: required attribute emitted", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs_ns, "string"}, true, {}, {}});

  complex_type ctype{qname{test_ns, "AttrType"}, false, false, content_type{},
                     std::move(attrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "AttrType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\" id=\"string\"/>");
}

TEST_CASE("doc_generator: optional attribute skipped", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "hint"}, qname{xs_ns, "string"}, false, {}, {}});

  complex_type ctype{qname{test_ns, "AttrType"}, false, false, content_type{},
                     std::move(attrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "AttrType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\"/>");
}

TEST_CASE("doc_generator: optional attribute with populate_optional",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "hint"}, qname{xs_ns, "string"}, false, {}, {}});

  complex_type ctype{qname{test_ns, "AttrType"}, false, false, content_type{},
                     std::move(attrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "AttrType"}});
  auto schemas = make_schema_set(std::move(s));

  doc_generator_options opts;
  opts.populate_optional = true;
  auto xml = generate_xml(schemas, qname{test_ns, "Root"}, opts);
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\" hint=\"string\"/>");
}

TEST_CASE("doc_generator: fixed attribute value used", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "version"}, qname{xs_ns, "string"}, true,
                   std::nullopt, std::string("2.0")});

  complex_type ctype{qname{test_ns, "AttrType"}, false, false, content_type{},
                     std::move(attrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "AttrType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\" version=\"2.0\"/>");
}

// ---------------------------------------------------------------------------
// Phase D: Simple type refinements (tests 12-14)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: enumeration facet picks first value",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  facet_set facets;
  facets.enumeration = {"red", "green", "blue"};
  simple_type st{qname{test_ns, "ColorType"}, simple_type_variety::atomic,
                 qname{xs_ns, "string"}, facets};
  s.add_simple_type(std::move(st));
  s.add_element(
      element_decl{qname{test_ns, "Color"}, qname{test_ns, "ColorType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Color"});
  REQUIRE(xml == "<Color xmlns=\"http://example.com/test\">red</Color>");
}

TEST_CASE("doc_generator: min_inclusive facet used", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  facet_set facets;
  facets.min_inclusive = "5";
  simple_type st{qname{test_ns, "MinType"}, simple_type_variety::atomic,
                 qname{xs_ns, "integer"}, facets};
  s.add_simple_type(std::move(st));
  s.add_element(
      element_decl{qname{test_ns, "Value"}, qname{test_ns, "MinType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Value"});
  REQUIRE(xml == "<Value xmlns=\"http://example.com/test\">5</Value>");
}

TEST_CASE("doc_generator: user-defined simple type follows base chain",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // NameType -> xs:string  (no facets — should follow chain to built-in)
  simple_type st{qname{test_ns, "NameType"}, simple_type_variety::atomic,
                 qname{xs_ns, "string"}};
  s.add_simple_type(std::move(st));
  s.add_element(
      element_decl{qname{test_ns, "Item"}, qname{test_ns, "NameType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Item"});
  REQUIRE(xml == "<Item xmlns=\"http://example.com/test\">string</Item>");
}

// ---------------------------------------------------------------------------
// Phase E: Complex content (tests 15-18)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: simpleContent emits text value", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  simple_content sc{qname{xs_ns, "string"}, derivation_method::restriction, {}};
  content_type ct{content_kind::simple, std::move(sc)};

  complex_type ctype{qname{test_ns, "TextType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Text"}, qname{test_ns, "TextType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Text"});
  REQUIRE(xml == "<Text xmlns=\"http://example.com/test\">string</Text>");
}

TEST_CASE("doc_generator: extension inherits base particles",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Base type with one element
  std::vector<particle> base_particles;
  base_particles.emplace_back(
      element_decl{qname{test_ns, "Base"}, qname{xs_ns, "string"}});
  model_group base_seq{compositor_kind::sequence, std::move(base_particles)};
  complex_content base_cc{qname{xs_ns, "anyType"},
                          derivation_method::restriction, std::move(base_seq)};
  content_type base_ct{content_kind::element_only, std::move(base_cc)};
  complex_type base_type{qname{test_ns, "BaseType"}, false, false,
                         std::move(base_ct)};
  s.add_complex_type(std::move(base_type));

  // Derived type extending base with one more element
  std::vector<particle> ext_particles;
  ext_particles.emplace_back(
      element_decl{qname{test_ns, "Extra"}, qname{xs_ns, "int"}});
  model_group ext_seq{compositor_kind::sequence, std::move(ext_particles)};
  complex_content ext_cc{qname{test_ns, "BaseType"},
                         derivation_method::extension, std::move(ext_seq)};
  content_type ext_ct{content_kind::element_only, std::move(ext_cc)};
  complex_type ext_type{qname{test_ns, "ExtType"}, false, false,
                        std::move(ext_ct)};
  s.add_complex_type(std::move(ext_type));

  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "ExtType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Base>string</Base>"
                 "<Extra>0</Extra>"
                 "</Root>");
}

TEST_CASE("doc_generator: element_ref resolves", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Global element
  s.add_element(element_decl{qname{test_ns, "Shared"}, qname{xs_ns, "string"}});

  // Complex type with element_ref
  std::vector<particle> particles;
  particles.emplace_back(element_ref{qname{test_ns, "Shared"}});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "RefType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));

  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "RefType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Shared>string</Shared>"
                 "</Root>");
}

TEST_CASE("doc_generator: group_ref resolves", "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Named model group
  std::vector<particle> group_particles;
  group_particles.emplace_back(
      element_decl{qname{test_ns, "A"}, qname{xs_ns, "string"}});
  model_group grp{compositor_kind::sequence, std::move(group_particles)};
  model_group_def gdef{qname{test_ns, "MyGroup"}, std::move(grp)};
  s.add_model_group_def(std::move(gdef));

  // Complex type referencing the group
  std::vector<particle> particles;
  particles.emplace_back(group_ref{qname{test_ns, "MyGroup"}});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "GrpRefType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));

  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "GrpRefType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<A>string</A>"
                 "</Root>");
}

// ---------------------------------------------------------------------------
// Phase F: Namespaces (tests 19-20)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: single namespace gets xmlns declaration",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);
  s.add_element(element_decl{qname{test_ns, "Item"}, qname{xs_ns, "string"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Item"});
  // Should contain xmlns="..." for the default namespace
  REQUIRE(xml.find("xmlns=\"http://example.com/test\"") != std::string::npos);
}

TEST_CASE("doc_generator: multiple namespaces get distinct prefixes",
          "[doc_generator]") {
  static const std::string ns_a = "http://example.com/a";
  static const std::string ns_b = "http://example.com/b";

  schema sa;
  sa.set_target_namespace(ns_a);
  // Child element in namespace b
  sa.add_element(element_decl{qname{ns_b, "Child"}, qname{xs_ns, "string"}});

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{ns_b, "Child"}, qname{xs_ns, "string"}});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{ns_a, "ParentType"}, false, false, std::move(ct)};
  sa.add_complex_type(std::move(ctype));
  sa.add_element(
      element_decl{qname{ns_a, "Parent"}, qname{ns_a, "ParentType"}});

  auto schemas = make_schema_set(std::move(sa));

  auto xml = generate_xml(schemas, qname{ns_a, "Parent"});
  // Root element gets default xmlns, child gets ns0 prefix
  REQUIRE(xml.find("xmlns=\"http://example.com/a\"") != std::string::npos);
  REQUIRE(xml.find("xmlns:ns0=\"http://example.com/b\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Phase G: Recursion safety (tests 21-22)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: self-referencing type respects max_depth",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // RecType has a sequence containing an optional element of its own type
  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "Value"}, qname{xs_ns, "string"}});
  particles.emplace_back(
      element_decl{qname{test_ns, "Child"}, qname{test_ns, "RecType"}},
      occurrence{0, 1});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "RecType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "RecType"}});
  auto schemas = make_schema_set(std::move(s));

  // With populate_optional, it would try to recurse. max_depth=2 limits it.
  doc_generator_options opts;
  opts.populate_optional = true;
  opts.max_depth = 2;
  auto xml = generate_xml(schemas, qname{test_ns, "Root"}, opts);

  // Should terminate without infinite recursion. The exact nesting depends
  // on depth, but it must not hang or crash.
  REQUIRE(!xml.empty());
  // At depth 0 we expand RecType, at depth 1 we expand RecType again in
  // Child, at depth 2 we stop — Child is empty.
  REQUIRE(xml.find("<Root") != std::string::npos);
  REQUIRE(xml.find("</Root>") != std::string::npos);
}

TEST_CASE("doc_generator: mutually recursive types terminate",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // TypeA contains optional TypeB, TypeB contains optional TypeA
  std::vector<particle> a_particles;
  a_particles.emplace_back(
      element_decl{qname{test_ns, "B"}, qname{test_ns, "TypeB"}},
      occurrence{0, 1});
  model_group a_seq{compositor_kind::sequence, std::move(a_particles)};
  complex_content a_cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                       std::move(a_seq)};
  content_type a_ct{content_kind::element_only, std::move(a_cc)};
  complex_type ta{qname{test_ns, "TypeA"}, false, false, std::move(a_ct)};
  s.add_complex_type(std::move(ta));

  std::vector<particle> b_particles;
  b_particles.emplace_back(
      element_decl{qname{test_ns, "A"}, qname{test_ns, "TypeA"}},
      occurrence{0, 1});
  model_group b_seq{compositor_kind::sequence, std::move(b_particles)};
  complex_content b_cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                       std::move(b_seq)};
  content_type b_ct{content_kind::element_only, std::move(b_cc)};
  complex_type tb{qname{test_ns, "TypeB"}, false, false, std::move(b_ct)};
  s.add_complex_type(std::move(tb));

  s.add_element(element_decl{qname{test_ns, "Root"}, qname{test_ns, "TypeA"}});
  auto schemas = make_schema_set(std::move(s));

  doc_generator_options opts;
  opts.populate_optional = true;
  opts.max_depth = 3;
  auto xml = generate_xml(schemas, qname{test_ns, "Root"}, opts);

  REQUIRE(!xml.empty());
  REQUIRE(xml.find("<Root") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Phase H: Options (tests 23-24)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: populate_optional emits optional elements",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "Required"}, qname{xs_ns, "string"}});
  particles.emplace_back(
      element_decl{qname{test_ns, "Optional"}, qname{xs_ns, "int"}},
      occurrence{0, 1});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "OptType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "OptType"}});
  auto schemas = make_schema_set(std::move(s));

  doc_generator_options opts;
  opts.populate_optional = true;
  auto xml = generate_xml(schemas, qname{test_ns, "Root"}, opts);
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\">"
                 "<Required>string</Required>"
                 "<Optional>0</Optional>"
                 "</Root>");
}

TEST_CASE("doc_generator: populate_optional emits optional attrs and elements",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  std::vector<attribute_use> attrs;
  attrs.push_back(
      {qname{"", "opt_attr"}, qname{xs_ns, "string"}, false, {}, {}});

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl{qname{test_ns, "OptChild"}, qname{xs_ns, "string"}},
      occurrence{0, 1});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "BothType"}, false, false, std::move(ct),
                     std::move(attrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Root"}, qname{test_ns, "BothType"}});
  auto schemas = make_schema_set(std::move(s));

  doc_generator_options opts;
  opts.populate_optional = true;
  auto xml = generate_xml(schemas, qname{test_ns, "Root"}, opts);
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\" "
                 "opt_attr=\"string\">"
                 "<OptChild>string</OptChild>"
                 "</Root>");
}

// ---------------------------------------------------------------------------
// Phase I: Abstract elements (test 25)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: abstract element uses first substitution member",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Abstract element
  s.add_element(element_decl{qname{test_ns, "Shape"}, qname{xs_ns, "string"},
                             false, true});
  // Concrete substitution member
  s.add_element(element_decl{qname{test_ns, "Circle"},
                             qname{xs_ns, "string"},
                             false,
                             false,
                             {},
                             {},
                             qname{test_ns, "Shape"}});

  // Type that uses the abstract element in a sequence
  std::vector<particle> particles;
  particles.emplace_back(element_decl{qname{test_ns, "Shape"},
                                      qname{xs_ns, "string"}, false, true});
  model_group seq{compositor_kind::sequence, std::move(particles)};
  complex_content cc{qname{xs_ns, "anyType"}, derivation_method::restriction,
                     std::move(seq)};
  content_type ct{content_kind::element_only, std::move(cc)};
  complex_type ctype{qname{test_ns, "DrawType"}, false, false, std::move(ct)};
  s.add_complex_type(std::move(ctype));
  s.add_element(
      element_decl{qname{test_ns, "Drawing"}, qname{test_ns, "DrawType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Drawing"});
  REQUIRE(xml.find("<Circle") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Phase J: attribute_group_ref (test 26)
// ---------------------------------------------------------------------------

TEST_CASE("doc_generator: attribute_group_ref resolved and attrs emitted",
          "[doc_generator]") {
  schema s;
  s.set_target_namespace(test_ns);

  // Attribute group with a required attribute
  std::vector<attribute_use> group_attrs;
  group_attrs.push_back(
      {qname{"", "lang"}, qname{xs_ns, "string"}, true, {}, {}});
  attribute_group_def agd{qname{test_ns, "CommonAttrs"},
                          std::move(group_attrs)};
  s.add_attribute_group_def(std::move(agd));

  // Complex type referencing the attribute group
  std::vector<attribute_group_ref> agrs;
  agrs.push_back({qname{test_ns, "CommonAttrs"}});

  complex_type ctype{qname{test_ns, "AGType"}, false, false,
                     content_type{},           {},    std::move(agrs)};
  s.add_complex_type(std::move(ctype));
  s.add_element(element_decl{qname{test_ns, "Root"}, qname{test_ns, "AGType"}});
  auto schemas = make_schema_set(std::move(s));

  auto xml = generate_xml(schemas, qname{test_ns, "Root"});
  REQUIRE(xml == "<Root xmlns=\"http://example.com/test\" lang=\"string\"/>");
}
