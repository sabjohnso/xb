#include <xb/assertion.hpp>
#include <xb/attribute_decl.hpp>
#include <xb/attribute_group_def.hpp>
#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/model_group_def.hpp>
#include <xb/occurrence.hpp>
#include <xb/simple_type.hpp>
#include <xb/wildcard.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>

static const std::string xs = "http://www.w3.org/2001/XMLSchema";
static const std::string tns = "urn:test";

// -- occurrence ---------------------------------------------------------------

TEST_CASE("occurrence default is (1,1)", "[occurrence]") {
  xb::occurrence o;
  CHECK(o.min_occurs == 1);
  CHECK(o.max_occurs == 1);
  CHECK_FALSE(o.is_unbounded());
}

TEST_CASE("occurrence custom values", "[occurrence]") {
  xb::occurrence o{0, 5};
  CHECK(o.min_occurs == 0);
  CHECK(o.max_occurs == 5);
  CHECK_FALSE(o.is_unbounded());
}

TEST_CASE("occurrence unbounded", "[occurrence]") {
  xb::occurrence o{1, xb::unbounded};
  CHECK(o.min_occurs == 1);
  CHECK(o.max_occurs == xb::unbounded);
  CHECK(o.is_unbounded());
}

TEST_CASE("occurrence equality", "[occurrence]") {
  xb::occurrence a{0, 1};
  xb::occurrence b{0, 1};
  xb::occurrence c{1, 1};
  CHECK(a == b);
  CHECK_FALSE(a == c);
}

// -- facet_set ----------------------------------------------------------------

TEST_CASE("facet_set default is empty", "[facet_set]") {
  xb::facet_set f;
  CHECK(f.enumeration.empty());
  CHECK_FALSE(f.pattern.has_value());
  CHECK_FALSE(f.min_inclusive.has_value());
  CHECK_FALSE(f.max_inclusive.has_value());
  CHECK_FALSE(f.min_exclusive.has_value());
  CHECK_FALSE(f.max_exclusive.has_value());
  CHECK_FALSE(f.length.has_value());
  CHECK_FALSE(f.min_length.has_value());
  CHECK_FALSE(f.max_length.has_value());
  CHECK_FALSE(f.total_digits.has_value());
  CHECK_FALSE(f.fraction_digits.has_value());
}

TEST_CASE("facet_set with enumeration values", "[facet_set]") {
  xb::facet_set f;
  f.enumeration = {"Buy", "Sell", "Hold"};
  CHECK(f.enumeration.size() == 3);
  CHECK(f.enumeration[0] == "Buy");
  CHECK(f.enumeration[2] == "Hold");
}

TEST_CASE("facet_set with numeric bounds", "[facet_set]") {
  xb::facet_set f;
  f.min_inclusive = "0";
  f.max_inclusive = "100";
  f.total_digits = 5;
  f.fraction_digits = 2;
  CHECK(f.min_inclusive.value() == "0");
  CHECK(f.max_inclusive.value() == "100");
  CHECK(f.total_digits.value() == 5);
  CHECK(f.fraction_digits.value() == 2);
}

TEST_CASE("facet_set equality", "[facet_set]") {
  xb::facet_set a;
  a.enumeration = {"A", "B"};
  xb::facet_set b;
  b.enumeration = {"A", "B"};
  xb::facet_set c;
  c.enumeration = {"A", "C"};
  CHECK(a == b);
  CHECK_FALSE(a == c);
}

// -- wildcard -----------------------------------------------------------------

TEST_CASE("wildcard default is any/strict", "[wildcard]") {
  xb::wildcard w;
  CHECK(w.ns_constraint == xb::wildcard_ns_constraint::any);
  CHECK(w.process == xb::process_contents::strict);
  CHECK(w.namespaces.empty());
}

TEST_CASE("wildcard with enumerated namespaces", "[wildcard]") {
  xb::wildcard w;
  w.ns_constraint = xb::wildcard_ns_constraint::enumerated;
  w.namespaces = {"urn:a", "urn:b"};
  w.process = xb::process_contents::lax;
  CHECK(w.ns_constraint == xb::wildcard_ns_constraint::enumerated);
  CHECK(w.namespaces.size() == 2);
  CHECK(w.process == xb::process_contents::lax);
}

TEST_CASE("wildcard equality", "[wildcard]") {
  xb::wildcard a;
  a.ns_constraint = xb::wildcard_ns_constraint::other;
  xb::wildcard b;
  b.ns_constraint = xb::wildcard_ns_constraint::other;
  CHECK(a == b);

  xb::wildcard c;
  CHECK_FALSE(a == c);
}

// -- simple_type --------------------------------------------------------------

TEST_CASE("simple_type atomic restriction with enumeration", "[simple_type]") {
  xb::facet_set facets;
  facets.enumeration = {"Buy", "Sell"};

  xb::simple_type st(xb::qname(tns, "SideType"),
                     xb::simple_type_variety::atomic, xb::qname(xs, "string"),
                     facets);

  CHECK(st.name() == xb::qname(tns, "SideType"));
  CHECK(st.variety() == xb::simple_type_variety::atomic);
  CHECK(st.base_type_name() == xb::qname(xs, "string"));
  CHECK(st.facets().enumeration.size() == 2);
  CHECK_FALSE(st.item_type_name().has_value());
  CHECK(st.member_type_names().empty());
}

TEST_CASE("simple_type list", "[simple_type]") {
  xb::simple_type st(xb::qname(tns, "IntListType"),
                     xb::simple_type_variety::list, xb::qname(xs, "integer"),
                     {}, xb::qname(xs, "integer"));

  CHECK(st.variety() == xb::simple_type_variety::list);
  CHECK(st.item_type_name().has_value());
  CHECK(st.item_type_name().value() == xb::qname(xs, "integer"));
}

TEST_CASE("simple_type union", "[simple_type]") {
  std::vector<xb::qname> members = {xb::qname(xs, "string"),
                                    xb::qname(xs, "int")};
  xb::simple_type st(xb::qname(tns, "StringOrInt"),
                     xb::simple_type_variety::union_type, xb::qname(), {},
                     std::nullopt, members);

  CHECK(st.variety() == xb::simple_type_variety::union_type);
  CHECK(st.member_type_names().size() == 2);
}

TEST_CASE("simple_type equality", "[simple_type]") {
  xb::simple_type a(xb::qname(tns, "A"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "string"));
  xb::simple_type b(xb::qname(tns, "A"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "string"));
  xb::simple_type c(xb::qname(tns, "B"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "string"));
  CHECK(a == b);
  CHECK_FALSE(a == c);
}

// -- element_decl -------------------------------------------------------------

TEST_CASE("element_decl construction and accessors", "[element_decl]") {
  xb::element_decl e(xb::qname(tns, "order"), xb::qname(tns, "OrderType"));

  CHECK(e.name() == xb::qname(tns, "order"));
  CHECK(e.type_name() == xb::qname(tns, "OrderType"));
  CHECK_FALSE(e.nillable());
  CHECK_FALSE(e.abstract());
  CHECK_FALSE(e.default_value().has_value());
  CHECK_FALSE(e.fixed_value().has_value());
  CHECK_FALSE(e.substitution_group().has_value());
}

TEST_CASE("element_decl nillable and abstract", "[element_decl]") {
  xb::element_decl e(xb::qname(tns, "item"), xb::qname(xs, "string"),
                     true,  // nillable
                     true); // abstract

  CHECK(e.nillable());
  CHECK(e.abstract());
}

TEST_CASE("element_decl default and fixed values", "[element_decl]") {
  xb::element_decl e(xb::qname(tns, "status"), xb::qname(xs, "string"), false,
                     false, "active", std::nullopt);

  CHECK(e.default_value().value() == "active");
  CHECK_FALSE(e.fixed_value().has_value());

  xb::element_decl f(xb::qname(tns, "version"), xb::qname(xs, "string"), false,
                     false, std::nullopt, "1.0");
  CHECK(f.fixed_value().value() == "1.0");
}

TEST_CASE("element_decl substitution group", "[element_decl]") {
  xb::element_decl e(xb::qname(tns, "special"), xb::qname(tns, "SpecialType"),
                     false, false, std::nullopt, std::nullopt,
                     xb::qname(tns, "base"));

  CHECK(e.substitution_group().has_value());
  CHECK(e.substitution_group().value() == xb::qname(tns, "base"));
}

TEST_CASE("element_ref", "[element_decl]") {
  xb::element_ref ref{xb::qname(tns, "someElement")};
  CHECK(ref.ref == xb::qname(tns, "someElement"));

  xb::element_ref ref2{xb::qname(tns, "someElement")};
  CHECK(ref == ref2);
}

// -- attribute_decl -----------------------------------------------------------

TEST_CASE("attribute_decl construction", "[attribute_decl]") {
  xb::attribute_decl a(xb::qname("", "id"), xb::qname(xs, "string"));
  CHECK(a.name() == xb::qname("", "id"));
  CHECK(a.type_name() == xb::qname(xs, "string"));
  CHECK_FALSE(a.default_value().has_value());
  CHECK_FALSE(a.fixed_value().has_value());
}

TEST_CASE("attribute_decl with default", "[attribute_decl]") {
  xb::attribute_decl a(xb::qname("", "currency"), xb::qname(xs, "string"),
                       "USD");
  CHECK(a.default_value().value() == "USD");
}

TEST_CASE("attribute_use required and optional", "[attribute_use]") {
  xb::attribute_use req;
  req.name = xb::qname("", "id");
  req.type_name = xb::qname(xs, "string");
  req.required = true;
  CHECK(req.required);

  xb::attribute_use opt;
  opt.name = xb::qname("", "lang");
  opt.type_name = xb::qname(xs, "language");
  opt.required = false;
  CHECK_FALSE(opt.required);
}

TEST_CASE("attribute_use default and fixed", "[attribute_use]") {
  xb::attribute_use au;
  au.name = xb::qname("", "currency");
  au.type_name = xb::qname(xs, "string");
  au.default_value = "USD";
  CHECK(au.default_value.value() == "USD");
  CHECK_FALSE(au.fixed_value.has_value());
}

TEST_CASE("attribute_group_ref", "[attribute_decl]") {
  xb::attribute_group_ref ref{xb::qname(tns, "commonAttrs")};
  CHECK(ref.ref == xb::qname(tns, "commonAttrs"));
}

// -- model_group + particle ---------------------------------------------------

TEST_CASE("model_group flat sequence of element_refs", "[model_group]") {
  xb::model_group seq(xb::compositor_kind::sequence);
  seq.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "a")}));
  seq.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "b")}));

  CHECK(seq.compositor() == xb::compositor_kind::sequence);
  CHECK(seq.particles().size() == 2);
}

TEST_CASE("model_group flat choice", "[model_group]") {
  xb::model_group ch(xb::compositor_kind::choice);
  ch.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "x")}));
  ch.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "y")}));

  CHECK(ch.compositor() == xb::compositor_kind::choice);
  CHECK(ch.particles().size() == 2);
}

TEST_CASE("model_group nested sequence in choice (unique_ptr)",
          "[model_group]") {
  auto inner = std::make_unique<xb::model_group>(xb::compositor_kind::sequence);
  inner->add_particle(xb::particle(xb::element_ref{xb::qname(tns, "a")}));
  inner->add_particle(xb::particle(xb::element_ref{xb::qname(tns, "b")}));

  xb::model_group outer(xb::compositor_kind::choice);
  outer.add_particle(xb::particle(std::move(inner)));
  outer.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "c")}));

  CHECK(outer.particles().size() == 2);

  const auto& first_term = outer.particles()[0].term;
  CHECK(std::holds_alternative<std::unique_ptr<xb::model_group>>(first_term));

  const auto& nested = std::get<std::unique_ptr<xb::model_group>>(first_term);
  CHECK(nested->compositor() == xb::compositor_kind::sequence);
  CHECK(nested->particles().size() == 2);
}

TEST_CASE("model_group move semantics", "[model_group]") {
  xb::model_group src(xb::compositor_kind::sequence);
  src.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "a")}));

  xb::model_group dst = std::move(src);
  CHECK(dst.compositor() == xb::compositor_kind::sequence);
  CHECK(dst.particles().size() == 1);
}

TEST_CASE("particle with occurrence", "[model_group]") {
  xb::particle p(xb::element_ref{xb::qname(tns, "item")},
                 xb::occurrence{0, xb::unbounded});
  CHECK(p.occurs.min_occurs == 0);
  CHECK(p.occurs.is_unbounded());
}

TEST_CASE("particle with wildcard", "[model_group]") {
  xb::wildcard w;
  w.process = xb::process_contents::lax;
  xb::particle p(w, xb::occurrence{0, xb::unbounded});

  CHECK(std::holds_alternative<xb::wildcard>(p.term));
}

TEST_CASE("group_ref", "[model_group]") {
  xb::group_ref ref{xb::qname(tns, "myGroup")};
  CHECK(ref.ref == xb::qname(tns, "myGroup"));
}

TEST_CASE("model_group equality", "[model_group]") {
  xb::model_group a(xb::compositor_kind::sequence);
  a.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "x")}));

  xb::model_group b(xb::compositor_kind::sequence);
  b.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "x")}));

  CHECK(a == b);

  xb::model_group c(xb::compositor_kind::choice);
  c.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "x")}));

  CHECK_FALSE(a == c);
}

// -- content_type -------------------------------------------------------------

TEST_CASE("content_type empty", "[content_type]") {
  xb::content_type ct;
  CHECK(ct.kind == xb::content_kind::empty);
  CHECK(std::holds_alternative<std::monostate>(ct.detail));
}

TEST_CASE("content_type with simple content", "[content_type]") {
  xb::simple_content sc;
  sc.base_type_name = xb::qname(xs, "string");
  sc.derivation = xb::derivation_method::extension;

  xb::content_type ct(xb::content_kind::simple, std::move(sc));
  CHECK(ct.kind == xb::content_kind::simple);
  CHECK(std::holds_alternative<xb::simple_content>(ct.detail));

  const auto& detail = std::get<xb::simple_content>(ct.detail);
  CHECK(detail.base_type_name == xb::qname(xs, "string"));
}

TEST_CASE("content_type with complex content and model_group",
          "[content_type]") {
  xb::model_group mg(xb::compositor_kind::sequence);
  mg.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "a")}));

  xb::complex_content cc(xb::qname(tns, "BaseType"),
                         xb::derivation_method::extension, std::move(mg));

  xb::content_type ct(xb::content_kind::element_only, std::move(cc));
  CHECK(ct.kind == xb::content_kind::element_only);
  CHECK(std::holds_alternative<xb::complex_content>(ct.detail));
}

// -- complex_type -------------------------------------------------------------

TEST_CASE("complex_type with content and attributes", "[complex_type]") {
  xb::model_group mg(xb::compositor_kind::sequence);
  mg.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "name")}));

  xb::complex_content cc(xb::qname(), xb::derivation_method::restriction,
                         std::move(mg));
  xb::content_type ct(xb::content_kind::element_only, std::move(cc));

  xb::attribute_use attr;
  attr.name = xb::qname("", "id");
  attr.type_name = xb::qname(xs, "string");
  attr.required = true;

  xb::complex_type ctype(xb::qname(tns, "PersonType"), false, false,
                         std::move(ct), {attr});

  CHECK(ctype.name() == xb::qname(tns, "PersonType"));
  CHECK_FALSE(ctype.abstract());
  CHECK_FALSE(ctype.mixed());
  CHECK(ctype.content().kind == xb::content_kind::element_only);
  CHECK(ctype.attributes().size() == 1);
  CHECK(ctype.attributes()[0].name == xb::qname("", "id"));
}

TEST_CASE("complex_type with attribute wildcard", "[complex_type]") {
  xb::wildcard w;
  w.ns_constraint = xb::wildcard_ns_constraint::any;
  w.process = xb::process_contents::lax;

  xb::content_type ct;
  xb::complex_type ctype(xb::qname(tns, "OpenType"), false, false,
                         std::move(ct), {}, {}, w);

  CHECK(ctype.attribute_wildcard().has_value());
  CHECK(ctype.attribute_wildcard()->process == xb::process_contents::lax);
}

TEST_CASE("complex_type abstract and mixed", "[complex_type]") {
  xb::content_type ct(xb::content_kind::mixed, std::monostate{});
  xb::complex_type ctype(xb::qname(tns, "AbstractMixed"), true, true,
                         std::move(ct));
  CHECK(ctype.abstract());
  CHECK(ctype.mixed());
  CHECK(ctype.content().kind == xb::content_kind::mixed);
}

// -- model_group_def ----------------------------------------------------------

TEST_CASE("model_group_def construction and accessors", "[model_group_def]") {
  xb::model_group mg(xb::compositor_kind::sequence);
  mg.add_particle(xb::particle(xb::element_ref{xb::qname(tns, "a")}));

  xb::model_group_def def(xb::qname(tns, "myGroup"), std::move(mg));

  CHECK(def.name() == xb::qname(tns, "myGroup"));
  CHECK(def.group().compositor() == xb::compositor_kind::sequence);
  CHECK(def.group().particles().size() == 1);
}

// -- attribute_group_def ------------------------------------------------------

TEST_CASE("attribute_group_def construction and accessors",
          "[attribute_group_def]") {
  xb::attribute_use au;
  au.name = xb::qname("", "lang");
  au.type_name = xb::qname(xs, "language");

  xb::attribute_group_def def(xb::qname(tns, "i18nAttrs"), {au});

  CHECK(def.name() == xb::qname(tns, "i18nAttrs"));
  CHECK(def.attributes().size() == 1);
  CHECK(def.attributes()[0].name == xb::qname("", "lang"));
}

TEST_CASE("attribute_group_def with wildcard", "[attribute_group_def]") {
  xb::wildcard w;
  w.process = xb::process_contents::skip;

  xb::attribute_group_def def(xb::qname(tns, "openAttrs"), {}, {}, w);

  CHECK(def.attribute_wildcard().has_value());
  CHECK(def.attribute_wildcard()->process == xb::process_contents::skip);
}

// -- assertion ----------------------------------------------------------------

TEST_CASE("assertion construction and equality", "[assertion]") {
  xb::assertion a{"end >= start"};
  CHECK(a.test == "end >= start");

  xb::assertion b{"end >= start"};
  CHECK(a == b);

  xb::assertion c{"$value > 0"};
  CHECK_FALSE(a == c);
}

// -- complex_type with assertions ---------------------------------------------

TEST_CASE("complex_type with assertions", "[complex_type][assertion]") {
  xb::content_type ct;
  std::vector<xb::assertion> asserts = {{"end >= start"}, {"start > 0"}};

  xb::complex_type ctype(xb::qname(tns, "DateRange"), false, false,
                         std::move(ct), {}, {}, std::nullopt, std::nullopt,
                         std::move(asserts));

  REQUIRE(ctype.assertions().size() == 2);
  CHECK(ctype.assertions()[0].test == "end >= start");
  CHECK(ctype.assertions()[1].test == "start > 0");
}

TEST_CASE("complex_type assertions default to empty",
          "[complex_type][assertion]") {
  xb::content_type ct;
  xb::complex_type ctype(xb::qname(tns, "NoAssert"), false, false,
                         std::move(ct));

  CHECK(ctype.assertions().empty());
}

TEST_CASE("complex_type equality includes assertions",
          "[complex_type][assertion]") {
  xb::content_type ct1;
  xb::complex_type a(xb::qname(tns, "T"), false, false, std::move(ct1), {}, {},
                     std::nullopt, std::nullopt, {{"x > 0"}});

  xb::content_type ct2;
  xb::complex_type b(xb::qname(tns, "T"), false, false, std::move(ct2), {}, {},
                     std::nullopt, std::nullopt, {{"x > 0"}});

  CHECK(a == b);

  xb::content_type ct3;
  xb::complex_type c(xb::qname(tns, "T"), false, false, std::move(ct3), {}, {},
                     std::nullopt, std::nullopt, {{"y > 0"}});

  CHECK_FALSE(a == c);
}

// -- simple_type with assertions ----------------------------------------------

TEST_CASE("simple_type with assertions", "[simple_type][assertion]") {
  xb::simple_type st(xb::qname(tns, "PositiveInt"),
                     xb::simple_type_variety::atomic, xb::qname(xs, "integer"),
                     {}, std::nullopt, {}, {{"$value > 0"}});

  REQUIRE(st.assertions().size() == 1);
  CHECK(st.assertions()[0].test == "$value > 0");
}

TEST_CASE("simple_type assertions default to empty",
          "[simple_type][assertion]") {
  xb::simple_type st(xb::qname(tns, "Plain"), xb::simple_type_variety::atomic,
                     xb::qname(xs, "string"));

  CHECK(st.assertions().empty());
}

TEST_CASE("simple_type equality includes assertions",
          "[simple_type][assertion]") {
  xb::simple_type a(xb::qname(tns, "T"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "int"), {}, std::nullopt, {},
                    {{"$value > 0"}});

  xb::simple_type b(xb::qname(tns, "T"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "int"), {}, std::nullopt, {},
                    {{"$value > 0"}});

  CHECK(a == b);

  xb::simple_type c(xb::qname(tns, "T"), xb::simple_type_variety::atomic,
                    xb::qname(xs, "int"), {}, std::nullopt, {},
                    {{"$value < 0"}});

  CHECK_FALSE(a == c);
}
