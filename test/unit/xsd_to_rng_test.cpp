#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/xsd_to_rng.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::rng;

static const std::string xs = "http://www.w3.org/2001/XMLSchema";
static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";
static const std::string tns = "urn:test";

// Helper: build a schema_set from a single schema
static schema_set
make_ss(schema s) {
  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();
  return ss;
}

// ---------------------------------------------------------------------------
// Step 1: Empty schema → grammar with empty start
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: empty schema → grammar with empty start",
          "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.start);
  CHECK(g.start->holds<empty_pattern>());
  CHECK(g.defines.empty());
}

// ---------------------------------------------------------------------------
// Step 2: Single global element with xs:string → element define + data_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: single element with xs:string", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);
  s.add_element(element_decl(qname(tns, "greeting"), qname(xs, "string")));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();

  // start = ref to "greeting"
  REQUIRE(g.start);
  REQUIRE(g.start->holds<ref_pattern>());
  CHECK(g.start->get<ref_pattern>().name == "greeting");

  // One define for the element
  REQUIRE(g.defines.size() == 1);
  CHECK(g.defines[0].name == "greeting");
  REQUIRE(g.defines[0].body);
  REQUIRE(g.defines[0].body->holds<element_pattern>());
  auto& ep = g.defines[0].body->get<element_pattern>();
  REQUIRE(ep.name.holds<specific_name>());
  CHECK(ep.name.get<specific_name>().ns == tns);
  CHECK(ep.name.get<specific_name>().local_name == "greeting");
  // Content is data_pattern for xs:string
  REQUIRE(ep.content);
  REQUIRE(ep.content->holds<data_pattern>());
  CHECK(ep.content->get<data_pattern>().datatype_library == xsd_dt);
  CHECK(ep.content->get<data_pattern>().type == "string");
}

// ---------------------------------------------------------------------------
// Step 3: Multiple global elements → start = choice of refs
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: multiple elements → choice start", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);
  s.add_element(element_decl(qname(tns, "a"), qname(xs, "string")));
  s.add_element(element_decl(qname(tns, "b"), qname(xs, "int")));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.start);
  REQUIRE(g.start->holds<choice_pattern>());
  CHECK(g.defines.size() == 2);
}

// ---------------------------------------------------------------------------
// Step 4: Named complex type with sequence → group_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: complex type with sequence", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "name"), qname(xs, "string")));
  parts.emplace_back(element_decl(qname(tns, "age"), qname(xs, "int")));
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "personType"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "person"), qname(tns, "personType")));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  // Should have defines for "person" and "personType"
  CHECK(g.defines.size() == 2);
  // Find personType define
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "personType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body);
  // Content should be a group (sequence of two elements)
  REQUIRE(type_def->body->holds<group_pattern>());
}

// ---------------------------------------------------------------------------
// Step 5: Choice compositor → choice_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: choice compositor", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "cat"), qname(xs, "string")));
  parts.emplace_back(element_decl(qname(tns, "dog"), qname(xs, "string")));
  model_group mg(compositor_kind::choice, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "petType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "petType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<choice_pattern>());
}

// ---------------------------------------------------------------------------
// Step 6: All compositor → interleave_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: all compositor → interleave", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "x"), qname(xs, "string")));
  parts.emplace_back(element_decl(qname(tns, "y"), qname(xs, "string")));
  model_group mg(compositor_kind::all, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "pointType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "pointType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<interleave_pattern>());
}

// ---------------------------------------------------------------------------
// Step 7: Occurrence wrappers
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: occurrence {0,1} → optional", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "opt"), qname(xs, "string")),
                     occurrence{0, 1});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "optType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "optType") { type_def = &d; }
  }
  REQUIRE(type_def);
  // Single particle with optional → optional_pattern
  REQUIRE(type_def->body->holds<optional_pattern>());
}

TEST_CASE("xsd_to_rng: occurrence {1,∞} → oneOrMore", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "item"), qname(xs, "string")),
                     occurrence{1, unbounded});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "listType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "listType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<one_or_more_pattern>());
}

TEST_CASE("xsd_to_rng: occurrence {0,∞} → zeroOrMore", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "item"), qname(xs, "string")),
                     occurrence{0, unbounded});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "zType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "zType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<zero_or_more_pattern>());
}

// ---------------------------------------------------------------------------
// Step 8: Required + optional attributes
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: required + optional attributes", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  content_type ct;
  std::vector<attribute_use> attrs;
  attrs.push_back(
      attribute_use{qname("", "id"), qname(xs, "string"), true, {}, {}});
  attrs.push_back(
      attribute_use{qname("", "lang"), qname(xs, "string"), false, {}, {}});
  s.add_complex_type(complex_type(qname(tns, "attrType"), false, false,
                                  std::move(ct), std::move(attrs)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "attrType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body);
  // Should be group(attribute, optional(attribute))
  REQUIRE(type_def->body->holds<group_pattern>());
  auto& gp = type_def->body->get<group_pattern>();
  CHECK(gp.left->holds<attribute_pattern>());
  CHECK(gp.right->holds<optional_pattern>());
}

// ---------------------------------------------------------------------------
// Step 9: Mixed content → mixed_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: mixed content → mixed_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(element_decl(qname(tns, "b"), qname(xs, "string")));
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::mixed, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "mixedType"), false, true, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "mixedType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<mixed_pattern>());
}

// ---------------------------------------------------------------------------
// Step 10: Simple content on complex type → data_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: simple content → data_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  simple_content sc{qname(xs, "decimal"), derivation_method::restriction, {}};
  content_type ct(content_kind::simple, std::move(sc));
  s.add_complex_type(
      complex_type(qname(tns, "priceType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "priceType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<data_pattern>());
  CHECK(type_def->body->get<data_pattern>().type == "decimal");
}

// ---------------------------------------------------------------------------
// Step 11: Named simple type (atomic) → data_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: named simple type → data_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  facet_set fs;
  fs.min_inclusive = "0";
  fs.max_inclusive = "100";
  s.add_simple_type(simple_type(qname(tns, "percentType"),
                                simple_type_variety::atomic,
                                qname(xs, "integer"), std::move(fs)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.defines.size() == 1);
  CHECK(g.defines[0].name == "percentType");
  REQUIRE(g.defines[0].body->holds<data_pattern>());
  auto& dp = g.defines[0].body->get<data_pattern>();
  CHECK(dp.type == "integer");
  CHECK(dp.params.size() == 2);
}

// ---------------------------------------------------------------------------
// Step 12: Enumeration facets → choice of value_patterns
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: enumeration → choice of values", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  facet_set fs;
  fs.enumeration = {"red", "green", "blue"};
  s.add_simple_type(simple_type(qname(tns, "colorType"),
                                simple_type_variety::atomic,
                                qname(xs, "string"), std::move(fs)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.defines.size() == 1);
  REQUIRE(g.defines[0].body->holds<choice_pattern>());
  // The choice should contain value_patterns for red, green, blue
  // Structure: choice(red, choice(green, blue))
  auto& ch = g.defines[0].body->get<choice_pattern>();
  REQUIRE(ch.left->holds<value_pattern>());
  CHECK(ch.left->get<value_pattern>().value == "red");
}

// ---------------------------------------------------------------------------
// Step 13: List → list_pattern; union → choice_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: list type → list_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  s.add_simple_type(
      simple_type(qname(tns, "intList"), simple_type_variety::list,
                  qname(xs, "anySimpleType"), {}, qname(xs, "int")));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.defines.size() == 1);
  REQUIRE(g.defines[0].body->holds<list_pattern>());
  auto& lp = g.defines[0].body->get<list_pattern>();
  REQUIRE(lp.content->holds<data_pattern>());
  CHECK(lp.content->get<data_pattern>().type == "int");
}

TEST_CASE("xsd_to_rng: union type → choice_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  s.add_simple_type(simple_type(qname(tns, "strOrInt"),
                                simple_type_variety::union_type,
                                qname(xs, "anySimpleType"), {}, std::nullopt,
                                {qname(xs, "string"), qname(xs, "int")}));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  REQUIRE(g.defines.size() == 1);
  REQUIRE(g.defines[0].body->holds<choice_pattern>());
}

// ---------------------------------------------------------------------------
// Step 14: Element refs → ref_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: element ref in complex type", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  s.add_element(element_decl(qname(tns, "child"), qname(xs, "string")));

  std::vector<particle> parts;
  parts.emplace_back(element_ref{qname(tns, "child")});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "parentType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  // Should have defines for "child" element and "parentType"
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "parentType") { type_def = &d; }
  }
  REQUIRE(type_def);
  // Content should be a ref to "child"
  REQUIRE(type_def->body->holds<ref_pattern>());
  CHECK(type_def->body->get<ref_pattern>().name == "child");
}

// ---------------------------------------------------------------------------
// Step 15: Model group defs + group_ref → define + ref
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: group ref → ref to group define", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> gparts;
  gparts.emplace_back(element_decl(qname(tns, "a"), qname(xs, "string")));
  gparts.emplace_back(element_decl(qname(tns, "b"), qname(xs, "string")));
  model_group inner_mg(compositor_kind::sequence, std::move(gparts));
  s.add_model_group_def(
      model_group_def(qname(tns, "nameGroup"), std::move(inner_mg)));

  std::vector<particle> parts;
  parts.emplace_back(group_ref{qname(tns, "nameGroup")});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "useGroupType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  // Should have defines including "group.nameGroup"
  const define* group_def = nullptr;
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "group.nameGroup") { group_def = &d; }
    if (d.name == "useGroupType") { type_def = &d; }
  }
  REQUIRE(group_def);
  REQUIRE(group_def->body->holds<group_pattern>());
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<ref_pattern>());
  CHECK(type_def->body->get<ref_pattern>().name == "group.nameGroup");
}

// ---------------------------------------------------------------------------
// Step 16: Attribute group defs → define + ref
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: attribute group ref → ref to attrgroup define",
          "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<attribute_use> ag_attrs;
  ag_attrs.push_back(
      attribute_use{qname("", "x"), qname(xs, "string"), true, {}, {}});
  s.add_attribute_group_def(
      attribute_group_def(qname(tns, "coords"), std::move(ag_attrs)));

  content_type ct;
  s.add_complex_type(complex_type(qname(tns, "usesAttrGroup"), false, false,
                                  std::move(ct), {},
                                  {attribute_group_ref{qname(tns, "coords")}}));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* ag_def = nullptr;
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "attrgroup.coords") { ag_def = &d; }
    if (d.name == "usesAttrGroup") { type_def = &d; }
  }
  REQUIRE(ag_def);
  REQUIRE(ag_def->body->holds<attribute_pattern>());
  REQUIRE(type_def);
  // Content should reference the attribute group
  REQUIRE(type_def->body->holds<ref_pattern>());
  CHECK(type_def->body->get<ref_pattern>().name == "attrgroup.coords");
}

// ---------------------------------------------------------------------------
// Step 17: Wildcards → element/attribute with any_name_nc
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: xs:any wildcard", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  std::vector<particle> parts;
  parts.emplace_back(wildcard{}, occurrence{0, unbounded});
  model_group mg(compositor_kind::sequence, std::move(parts));
  complex_content cc(qname{}, derivation_method::restriction, std::move(mg));
  content_type ct(content_kind::element_only, std::move(cc));
  s.add_complex_type(
      complex_type(qname(tns, "anyType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "anyType") { type_def = &d; }
  }
  REQUIRE(type_def);
  // zeroOrMore(element { anyName } { text })
  REQUIRE(type_def->body->holds<zero_or_more_pattern>());
  auto& zm = type_def->body->get<zero_or_more_pattern>();
  REQUIRE(zm.content->holds<element_pattern>());
  CHECK(zm.content->get<element_pattern>().name.holds<any_name_nc>());
}

TEST_CASE("xsd_to_rng: xs:anyAttribute wildcard", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  content_type ct;
  s.add_complex_type(complex_type(qname(tns, "anyAttrType"), false, false,
                                  std::move(ct), {}, {}, wildcard{}));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "anyAttrType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<attribute_pattern>());
  CHECK(type_def->body->get<attribute_pattern>().name.holds<any_name_nc>());
}

// ---------------------------------------------------------------------------
// Step 18: Empty content → empty_pattern
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: empty content → empty_pattern", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  content_type ct;
  s.add_complex_type(
      complex_type(qname(tns, "emptyType"), false, false, std::move(ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* type_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "emptyType") { type_def = &d; }
  }
  REQUIRE(type_def);
  REQUIRE(type_def->body->holds<empty_pattern>());
}

// ---------------------------------------------------------------------------
// Step 19: Multi-schema → merged grammar
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: multi-schema merges into one grammar", "[xsd_to_rng]") {
  schema s1;
  s1.set_target_namespace("urn:ns1");
  s1.add_element(element_decl(qname("urn:ns1", "a"), qname(xs, "string")));

  schema s2;
  s2.set_target_namespace("urn:ns2");
  s2.add_element(element_decl(qname("urn:ns2", "b"), qname(xs, "int")));

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  ss.resolve();
  auto result = xsd_to_rng(ss);

  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  CHECK(g.defines.size() == 2);
  REQUIRE(g.start->holds<choice_pattern>());
}

// ---------------------------------------------------------------------------
// Step 20: Type extension → group(base_ref, local)
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: type extension → group(base, local)", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  // Base type: sequence(name)
  std::vector<particle> base_parts;
  base_parts.emplace_back(
      element_decl(qname(tns, "name"), qname(xs, "string")));
  model_group base_mg(compositor_kind::sequence, std::move(base_parts));
  complex_content base_cc(qname{}, derivation_method::restriction,
                          std::move(base_mg));
  content_type base_ct(content_kind::element_only, std::move(base_cc));
  s.add_complex_type(
      complex_type(qname(tns, "baseType"), false, false, std::move(base_ct)));

  // Derived type: extends baseType with sequence(email)
  std::vector<particle> ext_parts;
  ext_parts.emplace_back(
      element_decl(qname(tns, "email"), qname(xs, "string")));
  model_group ext_mg(compositor_kind::sequence, std::move(ext_parts));
  complex_content ext_cc(qname(tns, "baseType"), derivation_method::extension,
                         std::move(ext_mg));
  content_type ext_ct(content_kind::element_only, std::move(ext_cc));
  s.add_complex_type(
      complex_type(qname(tns, "derivedType"), false, false, std::move(ext_ct)));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  const define* derived_def = nullptr;
  for (const auto& d : g.defines) {
    if (d.name == "derivedType") { derived_def = &d; }
  }
  REQUIRE(derived_def);
  // Should be group(ref(baseType), element(email))
  REQUIRE(derived_def->body->holds<group_pattern>());
  auto& gp = derived_def->body->get<group_pattern>();
  REQUIRE(gp.left->holds<ref_pattern>());
  CHECK(gp.left->get<ref_pattern>().name == "baseType");
}

// ---------------------------------------------------------------------------
// Step: Name collision → element and type with same name
// ---------------------------------------------------------------------------

TEST_CASE("xsd_to_rng: name collision adds .type suffix", "[xsd_to_rng]") {
  schema s;
  s.set_target_namespace(tns);

  content_type ct;
  s.add_complex_type(
      complex_type(qname(tns, "item"), false, false, std::move(ct)));
  s.add_element(element_decl(qname(tns, "item"), qname(tns, "item")));
  auto ss = make_ss(std::move(s));
  auto result = xsd_to_rng(ss);

  auto& g = result.get<grammar_pattern>();
  bool found_element = false;
  bool found_type = false;
  for (const auto& d : g.defines) {
    if (d.name == "item") { found_element = true; }
    if (d.name == "item.type") { found_type = true; }
  }
  CHECK(found_element);
  CHECK(found_type);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
