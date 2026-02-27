#include <xb/rng_simplify.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
using namespace xb::rng;

// -- 4.13: mixed → interleave(p, text) ----------------------------------------

TEST_CASE("simplify: mixed desugars to interleave with text",
          "[rng_simplify]") {
  pattern p(mixed_pattern{make_pattern(empty_pattern{})});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<interleave_pattern>());
  auto& il = result.get<interleave_pattern>();
  CHECK(il.left->holds<empty_pattern>());
  CHECK(il.right->holds<text_pattern>());
}

// -- 4.14: optional → choice(p, empty) ----------------------------------------

TEST_CASE("simplify: optional desugars to choice with empty",
          "[rng_simplify]") {
  pattern p(optional_pattern{make_pattern(text_pattern{})});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<choice_pattern>());
  auto& ch = result.get<choice_pattern>();
  CHECK(ch.left->holds<text_pattern>());
  CHECK(ch.right->holds<empty_pattern>());
}

// -- 4.15: zeroOrMore → choice(oneOrMore(p), empty) ---------------------------

TEST_CASE("simplify: zeroOrMore desugars to choice of oneOrMore and empty",
          "[rng_simplify]") {
  pattern p(zero_or_more_pattern{make_pattern(text_pattern{})});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<choice_pattern>());
  auto& ch = result.get<choice_pattern>();
  REQUIRE(ch.left->holds<one_or_more_pattern>());
  CHECK(ch.left->get<one_or_more_pattern>().content->holds<text_pattern>());
  CHECK(ch.right->holds<empty_pattern>());
}

// -- 4.20: notAllowed propagation ---------------------------------------------

TEST_CASE("simplify: group with notAllowed propagates", "[rng_simplify]") {
  pattern p(group_pattern{make_pattern(not_allowed_pattern{}),
                          make_pattern(text_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<not_allowed_pattern>());
}

TEST_CASE("simplify: interleave with notAllowed propagates", "[rng_simplify]") {
  pattern p(interleave_pattern{make_pattern(text_pattern{}),
                               make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<not_allowed_pattern>());
}

TEST_CASE("simplify: choice with notAllowed simplifies to other",
          "[rng_simplify]") {
  pattern p(choice_pattern{make_pattern(not_allowed_pattern{}),
                           make_pattern(text_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<text_pattern>());
}

TEST_CASE("simplify: choice with notAllowed on right", "[rng_simplify]") {
  pattern p(choice_pattern{make_pattern(text_pattern{}),
                           make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<text_pattern>());
}

TEST_CASE("simplify: oneOrMore of notAllowed", "[rng_simplify]") {
  pattern p(one_or_more_pattern{make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<not_allowed_pattern>());
}

TEST_CASE("simplify: attribute with notAllowed body", "[rng_simplify]") {
  pattern p(attribute_pattern{name_class(specific_name{"", "x"}),
                              make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<not_allowed_pattern>());
}

TEST_CASE("simplify: list of notAllowed", "[rng_simplify]") {
  pattern p(list_pattern{make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<not_allowed_pattern>());
}

TEST_CASE("simplify: element with notAllowed remains", "[rng_simplify]") {
  pattern p(element_pattern{name_class(specific_name{"", "x"}),
                            make_pattern(not_allowed_pattern{})});
  auto result = rng_simplify(std::move(p));
  // element(nc, notAllowed) is kept as-is (simply never matches)
  CHECK(result.holds<element_pattern>());
}

// -- 4.17: combine merging ----------------------------------------------------

TEST_CASE("simplify: combine=choice merges defines", "[rng_simplify]") {
  std::vector<define> defs;
  defs.push_back(
      define{"inline", combine_method::none, make_pattern(text_pattern{})});
  defs.push_back(
      define{"inline", combine_method::choice,
             make_pattern(element_pattern{name_class(specific_name{"", "code"}),
                                          make_pattern(text_pattern{})})});

  pattern p(grammar_pattern{
      make_pattern(ref_pattern{"inline"}), std::move(defs), {}});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  // The two "inline" defines should be merged into one
  int count = 0;
  for (const auto& d : g.defines) {
    if (d.name == "inline") count++;
  }
  CHECK(count == 1);
  // The merged body should be a choice
  for (const auto& d : g.defines) {
    if (d.name == "inline") { CHECK(d.body->holds<choice_pattern>()); }
  }
}

TEST_CASE("simplify: combine=interleave merges defines", "[rng_simplify]") {
  std::vector<define> defs;
  defs.push_back(
      define{"attrs", combine_method::none, make_pattern(text_pattern{})});
  defs.push_back(define{"attrs", combine_method::interleave,
                        make_pattern(empty_pattern{})});

  pattern p(
      grammar_pattern{make_pattern(ref_pattern{"attrs"}), std::move(defs), {}});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<grammar_pattern>());
  for (const auto& d : result.get<grammar_pattern>().defines) {
    if (d.name == "attrs") { CHECK(d.body->holds<interleave_pattern>()); }
  }
}

// -- 4.19: unreachable definitions removed ------------------------------------

TEST_CASE("simplify: unreachable defines removed", "[rng_simplify]") {
  std::vector<define> defs;
  defs.push_back(
      define{"used", combine_method::none, make_pattern(text_pattern{})});
  defs.push_back(
      define{"unused", combine_method::none, make_pattern(empty_pattern{})});

  pattern p(
      grammar_pattern{make_pattern(ref_pattern{"used"}), std::move(defs), {}});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<grammar_pattern>());
  auto& g = result.get<grammar_pattern>();
  CHECK(g.defines.size() == 1);
  CHECK(g.defines[0].name == "used");
}

// -- recursive simplification ------------------------------------------------

TEST_CASE("simplify: nested optional inside element", "[rng_simplify]") {
  pattern p(element_pattern{
      name_class(specific_name{"", "root"}),
      make_pattern(optional_pattern{make_pattern(text_pattern{})})});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<element_pattern>());
  auto& elem = result.get<element_pattern>();
  // optional should be desugared to choice
  REQUIRE(elem.content->holds<choice_pattern>());
}

TEST_CASE("simplify: nested zeroOrMore inside group", "[rng_simplify]") {
  pattern p(group_pattern{
      make_pattern(zero_or_more_pattern{make_pattern(text_pattern{})}),
      make_pattern(empty_pattern{})});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<group_pattern>());
  auto& g = result.get<group_pattern>();
  // zeroOrMore should be desugared to choice(oneOrMore, empty)
  CHECK(g.left->holds<choice_pattern>());
}

// -- leaf patterns pass through -----------------------------------------------

TEST_CASE("simplify: text passes through", "[rng_simplify]") {
  pattern p(text_pattern{});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<text_pattern>());
}

TEST_CASE("simplify: empty passes through", "[rng_simplify]") {
  pattern p(empty_pattern{});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<empty_pattern>());
}

TEST_CASE("simplify: ref passes through", "[rng_simplify]") {
  pattern p(ref_pattern{"foo"});
  auto result = rng_simplify(std::move(p));
  REQUIRE(result.holds<ref_pattern>());
  CHECK(result.get<ref_pattern>().name == "foo");
}

TEST_CASE("simplify: data passes through", "[rng_simplify]") {
  pattern p(data_pattern{
      "http://www.w3.org/2001/XMLSchema-datatypes", "string", {}, nullptr});
  auto result = rng_simplify(std::move(p));
  CHECK(result.holds<data_pattern>());
}
