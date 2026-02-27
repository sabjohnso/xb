// GCC 12-13 emit a false -Wmaybe-uninitialized when constructing
// particle objects (std::variant containing std::unique_ptr) at -O3.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/rng_translator.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::rng;

static const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

// Helper: build a simple grammar with a start element pointing to a define
static pattern
make_grammar(const std::string& start_name, std::vector<define> defs) {
  return pattern(grammar_pattern{
      make_pattern(ref_pattern{start_name}), std::move(defs), {}});
}

// -- element with text content → complex type with simple content -------------

TEST_CASE("translate: element with text → element decl + complex type",
          "[rng_translator]") {
  std::vector<define> defs;
  defs.push_back(define{
      "doc", combine_method::none,
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "doc"}),
                                   make_pattern(text_pattern{})})});

  auto p = make_grammar("doc", std::move(defs));
  auto ss = rng_translate(p);

  CHECK(ss.schemas().size() == 1);
  const auto& s = ss.schemas()[0];
  CHECK(s.elements().size() >= 1);
  CHECK(s.elements()[0].name() == qname("urn:test", "doc"));
}

// -- element with child elements → complex type with sequence -----------------

TEST_CASE("translate: element with children → complex type with sequence",
          "[rng_translator]") {
  // doc { name: text, email: text }
  auto content = make_pattern(group_pattern{
      make_pattern(
          element_pattern{name_class(specific_name{"urn:test", "name"}),
                          make_pattern(text_pattern{})}),
      make_pattern(
          element_pattern{name_class(specific_name{"urn:test", "email"}),
                          make_pattern(text_pattern{})})});

  std::vector<define> defs;
  defs.push_back(define{
      "doc", combine_method::none,
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "doc"}),
                                   std::move(content)})});

  auto p = make_grammar("doc", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  // Should have a complex type for doc
  CHECK(!s.complex_types().empty());
  // Should have element declarations
  CHECK(s.elements().size() >= 1);
}

// -- element with attribute ---------------------------------------------------

TEST_CASE("translate: element with attribute → attribute_use",
          "[rng_translator]") {
  auto content = make_pattern(group_pattern{
      make_pattern(attribute_pattern{
          name_class(specific_name{"", "type"}),
          make_pattern(data_pattern{xsd_dt, "string", {}, nullptr})}),
      make_pattern(text_pattern{})});

  std::vector<define> defs;
  defs.push_back(define{
      "card", combine_method::none,
      make_pattern(element_pattern{
          name_class(specific_name{"urn:test", "card"}), std::move(content)})});

  auto p = make_grammar("card", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  REQUIRE(!s.complex_types().empty());
  // Find the complex type for card
  const complex_type* ct = nullptr;
  for (const auto& t : s.complex_types()) {
    if (t.name().local_name() == "card") {
      ct = &t;
      break;
    }
  }
  REQUIRE(ct != nullptr);
  CHECK(ct->attributes().size() >= 1);
  CHECK(ct->attributes()[0].name.local_name() == "type");
  CHECK(ct->attributes()[0].required == true);
}

// -- data type → simple type --------------------------------------------------

TEST_CASE("translate: data type maps to XSD type", "[rng_translator]") {
  std::vector<define> defs;
  defs.push_back(
      define{"qty", combine_method::none,
             make_pattern(element_pattern{
                 name_class(specific_name{"urn:test", "qty"}),
                 make_pattern(data_pattern{xsd_dt, "integer", {}, nullptr})})});

  auto p = make_grammar("qty", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  REQUIRE(s.elements().size() >= 1);
  // The element should reference the XSD integer type
  CHECK(s.elements()[0].type_name().local_name() == "integer");
}

// -- choice → choice compositor -----------------------------------------------

TEST_CASE("translate: choice → model_group with choice compositor",
          "[rng_translator]") {
  auto choice_content = make_pattern(choice_pattern{
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "a"}),
                                   make_pattern(text_pattern{})}),
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "b"}),
                                   make_pattern(text_pattern{})})});

  std::vector<define> defs;
  defs.push_back(define{"root", combine_method::none,
                        make_pattern(element_pattern{
                            name_class(specific_name{"urn:test", "root"}),
                            std::move(choice_content)})});

  auto p = make_grammar("root", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  REQUIRE(!s.complex_types().empty());
  // The root type should have a choice content model
  for (const auto& ct : s.complex_types()) {
    if (ct.name().local_name() == "root") {
      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      REQUIRE(cc != nullptr);
      REQUIRE(cc->content_model.has_value());
      CHECK(cc->content_model->compositor() == compositor_kind::choice);
    }
  }
}

// -- interleave → interleave compositor ---------------------------------------

TEST_CASE("translate: interleave → model_group with interleave compositor",
          "[rng_translator]") {
  auto il_content = make_pattern(interleave_pattern{
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "x"}),
                                   make_pattern(text_pattern{})}),
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "y"}),
                                   make_pattern(text_pattern{})})});

  std::vector<define> defs;
  defs.push_back(define{"unord", combine_method::none,
                        make_pattern(element_pattern{
                            name_class(specific_name{"urn:test", "unord"}),
                            std::move(il_content)})});

  auto p = make_grammar("unord", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  for (const auto& ct : s.complex_types()) {
    if (ct.name().local_name() == "unord") {
      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      REQUIRE(cc != nullptr);
      REQUIRE(cc->content_model.has_value());
      CHECK(cc->content_model->compositor() == compositor_kind::interleave);
    }
  }
}

// -- oneOrMore → occurrence {1, unbounded} ------------------------------------

TEST_CASE("translate: oneOrMore → particle with unbounded occurrence",
          "[rng_translator]") {
  auto content = make_pattern(one_or_more_pattern{make_pattern(
      element_pattern{name_class(specific_name{"urn:test", "item"}),
                      make_pattern(text_pattern{})})});

  std::vector<define> defs;
  defs.push_back(define{
      "list", combine_method::none,
      make_pattern(element_pattern{
          name_class(specific_name{"urn:test", "list"}), std::move(content)})});

  auto p = make_grammar("list", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  for (const auto& ct : s.complex_types()) {
    if (ct.name().local_name() == "list") {
      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      REQUIRE(cc != nullptr);
      REQUIRE(cc->content_model.has_value());
      // Check that at least one particle has unbounded occurrence
      bool found_unbounded = false;
      for (const auto& part : cc->content_model->particles()) {
        if (part.occurs.is_unbounded()) found_unbounded = true;
      }
      CHECK(found_unbounded);
    }
  }
}

// -- optional → particle with {0, 1} occurrence ------------------------------

TEST_CASE("translate: choice(p, empty) → particle with optional occurrence",
          "[rng_translator]") {
  // After simplification, optional becomes choice(p, empty)
  auto content = make_pattern(choice_pattern{
      make_pattern(element_pattern{name_class(specific_name{"urn:test", "opt"}),
                                   make_pattern(text_pattern{})}),
      make_pattern(empty_pattern{})});

  std::vector<define> defs;
  defs.push_back(define{
      "root", combine_method::none,
      make_pattern(element_pattern{
          name_class(specific_name{"urn:test", "root"}), std::move(content)})});

  auto p = make_grammar("root", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  for (const auto& ct : s.complex_types()) {
    if (ct.name().local_name() == "root") {
      auto* cc = std::get_if<complex_content>(&ct.content().detail);
      REQUIRE(cc != nullptr);
      REQUIRE(cc->content_model.has_value());
      // Should have a particle with {0,1} for the optional element
      bool found_optional = false;
      for (const auto& part : cc->content_model->particles()) {
        if (part.occurs.min_occurs == 0 && part.occurs.max_occurs == 1)
          found_optional = true;
      }
      CHECK(found_optional);
    }
  }
}

// -- ref linking: define with ref → proper type linking -----------------------

TEST_CASE("translate: ref in body links to correct type", "[rng_translator]") {
  std::vector<define> defs;
  // Define "item" as element
  defs.push_back(define{"item", combine_method::none,
                        make_pattern(element_pattern{
                            name_class(specific_name{"urn:test", "item"}),
                            make_pattern(text_pattern{})})});
  // Define "container" referencing "item"
  defs.push_back(define{"container", combine_method::none,
                        make_pattern(element_pattern{
                            name_class(specific_name{"urn:test", "container"}),
                            make_pattern(ref_pattern{"item"})})});

  auto p = make_grammar("container", std::move(defs));
  auto ss = rng_translate(p);

  const auto& s = ss.schemas()[0];
  // Both "container" and "item" should be element declarations
  CHECK(s.elements().size() >= 2);
}
