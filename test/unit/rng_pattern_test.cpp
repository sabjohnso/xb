#include <xb/rng_pattern.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>

using namespace xb::rng;

// -- name_class construction --------------------------------------------------

TEST_CASE("specific_name construction", "[rng_pattern]") {
  name_class nc(specific_name{"urn:test", "foo"});
  REQUIRE(nc.holds<specific_name>());
  CHECK(nc.get<specific_name>().ns == "urn:test");
  CHECK(nc.get<specific_name>().local_name == "foo");
}

TEST_CASE("any_name without except", "[rng_pattern]") {
  name_class nc(any_name_nc{});
  REQUIRE(nc.holds<any_name_nc>());
  CHECK(nc.get<any_name_nc>().except == nullptr);
}

TEST_CASE("any_name with except", "[rng_pattern]") {
  auto except = make_name_class(specific_name{"urn:test", "bar"});
  name_class nc(any_name_nc{std::move(except)});
  REQUIRE(nc.holds<any_name_nc>());
  REQUIRE(nc.get<any_name_nc>().except != nullptr);
  CHECK(nc.get<any_name_nc>().except->holds<specific_name>());
}

TEST_CASE("ns_name with except", "[rng_pattern]") {
  auto except = make_name_class(specific_name{"urn:test", "skip"});
  name_class nc(ns_name_nc{"urn:test", std::move(except)});
  REQUIRE(nc.holds<ns_name_nc>());
  CHECK(nc.get<ns_name_nc>().ns == "urn:test");
  REQUIRE(nc.get<ns_name_nc>().except != nullptr);
}

TEST_CASE("name_class choice", "[rng_pattern]") {
  auto left = make_name_class(specific_name{"", "a"});
  auto right = make_name_class(specific_name{"", "b"});
  name_class nc(choice_name_class{std::move(left), std::move(right)});
  REQUIRE(nc.holds<choice_name_class>());
}

// -- leaf pattern construction ------------------------------------------------

TEST_CASE("empty pattern", "[rng_pattern]") {
  pattern p(empty_pattern{});
  CHECK(p.holds<empty_pattern>());
}

TEST_CASE("text pattern", "[rng_pattern]") {
  pattern p(text_pattern{});
  CHECK(p.holds<text_pattern>());
}

TEST_CASE("not_allowed pattern", "[rng_pattern]") {
  pattern p(not_allowed_pattern{});
  CHECK(p.holds<not_allowed_pattern>());
}

// -- ref patterns -------------------------------------------------------------

TEST_CASE("ref pattern", "[rng_pattern]") {
  pattern p(ref_pattern{"cardContent"});
  REQUIRE(p.holds<ref_pattern>());
  CHECK(p.get<ref_pattern>().name == "cardContent");
}

TEST_CASE("parent_ref pattern", "[rng_pattern]") {
  pattern p(parent_ref_pattern{"outer"});
  REQUIRE(p.holds<parent_ref_pattern>());
  CHECK(p.get<parent_ref_pattern>().name == "outer");
}

// -- element and attribute patterns -------------------------------------------

TEST_CASE("element pattern with text content", "[rng_pattern]") {
  pattern p(element_pattern{name_class(specific_name{"", "name"}),
                            make_pattern(text_pattern{})});
  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  CHECK(elem.name.holds<specific_name>());
  CHECK(elem.name.get<specific_name>().local_name == "name");
  REQUIRE(elem.content != nullptr);
  CHECK(elem.content->holds<text_pattern>());
}

TEST_CASE("attribute pattern", "[rng_pattern]") {
  pattern p(attribute_pattern{name_class(specific_name{"", "type"}),
                              make_pattern(text_pattern{})});
  REQUIRE(p.holds<attribute_pattern>());
  auto& attr = p.get<attribute_pattern>();
  CHECK(attr.name.get<specific_name>().local_name == "type");
}

// -- combinator patterns ------------------------------------------------------

TEST_CASE("group pattern", "[rng_pattern]") {
  pattern p(group_pattern{make_pattern(text_pattern{}),
                          make_pattern(empty_pattern{})});
  REQUIRE(p.holds<group_pattern>());
  CHECK(p.get<group_pattern>().left->holds<text_pattern>());
  CHECK(p.get<group_pattern>().right->holds<empty_pattern>());
}

TEST_CASE("interleave pattern", "[rng_pattern]") {
  pattern p(interleave_pattern{make_pattern(text_pattern{}),
                               make_pattern(empty_pattern{})});
  REQUIRE(p.holds<interleave_pattern>());
}

TEST_CASE("choice pattern", "[rng_pattern]") {
  pattern p(choice_pattern{make_pattern(text_pattern{}),
                           make_pattern(empty_pattern{})});
  REQUIRE(p.holds<choice_pattern>());
}

// -- occurrence patterns ------------------------------------------------------

TEST_CASE("one_or_more pattern", "[rng_pattern]") {
  pattern p(one_or_more_pattern{make_pattern(text_pattern{})});
  REQUIRE(p.holds<one_or_more_pattern>());
  CHECK(p.get<one_or_more_pattern>().content->holds<text_pattern>());
}

TEST_CASE("zero_or_more pattern", "[rng_pattern]") {
  pattern p(zero_or_more_pattern{make_pattern(text_pattern{})});
  REQUIRE(p.holds<zero_or_more_pattern>());
}

TEST_CASE("optional pattern", "[rng_pattern]") {
  pattern p(optional_pattern{make_pattern(text_pattern{})});
  REQUIRE(p.holds<optional_pattern>());
}

TEST_CASE("mixed pattern", "[rng_pattern]") {
  pattern p(mixed_pattern{make_pattern(empty_pattern{})});
  REQUIRE(p.holds<mixed_pattern>());
}

// -- data patterns ------------------------------------------------------------

TEST_CASE("data pattern with params", "[rng_pattern]") {
  pattern p(data_pattern{"http://www.w3.org/2001/XMLSchema-datatypes",
                         "string",
                         {{"minLength", "1"}, {"maxLength", "100"}},
                         nullptr});
  REQUIRE(p.holds<data_pattern>());
  auto& d = p.get<data_pattern>();
  CHECK(d.datatype_library == "http://www.w3.org/2001/XMLSchema-datatypes");
  CHECK(d.type == "string");
  CHECK(d.params.size() == 2);
  CHECK(d.params[0].name == "minLength");
  CHECK(d.except == nullptr);
}

TEST_CASE("data pattern with except", "[rng_pattern]") {
  pattern p(data_pattern{
      "http://www.w3.org/2001/XMLSchema-datatypes",
      "token",
      {},
      make_pattern(value_pattern{"http://www.w3.org/2001/XMLSchema-datatypes",
                                 "token", "forbidden", ""})});
  REQUIRE(p.holds<data_pattern>());
  CHECK(p.get<data_pattern>().except != nullptr);
}

TEST_CASE("value pattern", "[rng_pattern]") {
  pattern p(value_pattern{"http://www.w3.org/2001/XMLSchema-datatypes", "token",
                          "personal", ""});
  REQUIRE(p.holds<value_pattern>());
  CHECK(p.get<value_pattern>().value == "personal");
}

TEST_CASE("list pattern", "[rng_pattern]") {
  pattern p(list_pattern{make_pattern(one_or_more_pattern{
      make_pattern(data_pattern{"http://www.w3.org/2001/XMLSchema-datatypes",
                                "double",
                                {},
                                nullptr})})});
  REQUIRE(p.holds<list_pattern>());
}

// -- modularity patterns ------------------------------------------------------

TEST_CASE("external_ref pattern", "[rng_pattern]") {
  pattern p(external_ref_pattern{"other.rng", "urn:other"});
  REQUIRE(p.holds<external_ref_pattern>());
  CHECK(p.get<external_ref_pattern>().href == "other.rng");
}

// -- grammar pattern ----------------------------------------------------------

TEST_CASE("grammar with start and defines", "[rng_pattern]") {
  std::vector<define> defs;
  defs.push_back(
      define{"card", combine_method::none,
             make_pattern(element_pattern{name_class(specific_name{"", "card"}),
                                          make_pattern(text_pattern{})})});

  pattern p(
      grammar_pattern{make_pattern(ref_pattern{"card"}), std::move(defs), {}});
  REQUIRE(p.holds<grammar_pattern>());
  auto& g = p.get<grammar_pattern>();
  REQUIRE(g.start != nullptr);
  CHECK(g.start->holds<ref_pattern>());
  CHECK(g.defines.size() == 1);
  CHECK(g.defines[0].name == "card");
}

TEST_CASE("grammar with include", "[rng_pattern]") {
  std::vector<define> overrides;
  overrides.push_back(
      define{"inline", combine_method::none, make_pattern(text_pattern{})});

  std::vector<include_directive> includes;
  includes.push_back(
      include_directive{"base.rng", "", std::move(overrides), nullptr});

  pattern p(grammar_pattern{
      make_pattern(ref_pattern{"doc"}), {}, std::move(includes)});
  REQUIRE(p.holds<grammar_pattern>());
  CHECK(p.get<grammar_pattern>().includes.size() == 1);
  CHECK(p.get<grammar_pattern>().includes[0].href == "base.rng");
  CHECK(p.get<grammar_pattern>().includes[0].overrides.size() == 1);
}

// -- move semantics -----------------------------------------------------------

TEST_CASE("pattern is movable", "[rng_pattern]") {
  pattern p1(text_pattern{});
  pattern p2(std::move(p1));
  CHECK(p2.holds<text_pattern>());
}

TEST_CASE("name_class is movable", "[rng_pattern]") {
  name_class nc1(specific_name{"", "x"});
  name_class nc2(std::move(nc1));
  CHECK(nc2.holds<specific_name>());
}

// -- recursive nesting --------------------------------------------------------

TEST_CASE("deeply nested patterns", "[rng_pattern]") {
  // element { name = group(text, choice(empty, ref)) }
  auto ref = make_pattern(ref_pattern{"other"});
  auto empty = make_pattern(empty_pattern{});
  auto ch = make_pattern(choice_pattern{std::move(empty), std::move(ref)});
  auto txt = make_pattern(text_pattern{});
  auto grp = make_pattern(group_pattern{std::move(txt), std::move(ch)});

  pattern p(element_pattern{name_class(specific_name{"urn:test", "deep"}),
                            std::move(grp)});

  REQUIRE(p.holds<element_pattern>());
  auto& elem = p.get<element_pattern>();
  REQUIRE(elem.content->holds<group_pattern>());
  auto& g = elem.content->get<group_pattern>();
  CHECK(g.left->holds<text_pattern>());
  REQUIRE(g.right->holds<choice_pattern>());
  auto& c = g.right->get<choice_pattern>();
  CHECK(c.left->holds<empty_pattern>());
  CHECK(c.right->holds<ref_pattern>());
}

TEST_CASE("define with combine method", "[rng_pattern]") {
  define d{"inline", combine_method::choice, make_pattern(text_pattern{})};
  CHECK(d.name == "inline");
  CHECK(d.combine == combine_method::choice);
  REQUIRE(d.body != nullptr);
  CHECK(d.body->holds<text_pattern>());
}
