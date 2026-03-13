#include <xb/wire/selector_specificity.hpp>

#include <encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using xb::bes::selector_type;
using xb::wire::more_specific;
using xb::wire::selectors_conflict;
using xb::wire::specificity;

// ---------------------------------------------------------------------------
// specificity: basic ordering
// ---------------------------------------------------------------------------

TEST_CASE("empty selector has lowest specificity", "[wire][selector]") {
  selector_type empty;
  selector_type with_ns;
  with_ns.namespace_ = "http://example.com";

  CHECK(specificity(empty) < specificity(with_ns));
}

TEST_CASE("specificity ordering: qname > path > type > namespace > empty",
          "[wire][selector]") {
  selector_type s_qname;
  s_qname.qname = "{http://example.com}OrderMessage";

  selector_type s_path;
  s_path.path = "Parent > Child";

  selector_type s_type;
  s_type.type = "xs:decimal";

  selector_type s_ns;
  s_ns.namespace_ = "http://example.com";

  selector_type s_empty;

  CHECK(specificity(s_qname) > specificity(s_path));
  CHECK(specificity(s_path) > specificity(s_type));
  CHECK(specificity(s_type) > specificity(s_ns));
  CHECK(specificity(s_ns) > specificity(s_empty));
}

// ---------------------------------------------------------------------------
// specificity: attribute adds weight
// ---------------------------------------------------------------------------

TEST_CASE("attribute selector adds specificity within same tier",
          "[wire][selector]") {
  selector_type s_type;
  s_type.type = "xs:decimal";

  selector_type s_type_attr;
  s_type_attr.type = "xs:decimal";
  s_type_attr.attribute = "name=\"price\"";

  CHECK(specificity(s_type_attr) > specificity(s_type));
}

// ---------------------------------------------------------------------------
// specificity: combined selectors
// ---------------------------------------------------------------------------

TEST_CASE("qname with attribute beats plain qname", "[wire][selector]") {
  selector_type plain;
  plain.qname = "{http://example.com}Foo";

  selector_type with_attr;
  with_attr.qname = "{http://example.com}Foo";
  with_attr.attribute = "name=\"bar\"";

  CHECK(specificity(with_attr) > specificity(plain));
}

TEST_CASE("path with type beats plain path", "[wire][selector]") {
  selector_type plain;
  plain.path = "Parent > Child";

  selector_type with_type;
  with_type.path = "Parent > Child";
  with_type.type = "xs:string";

  CHECK(specificity(with_type) > specificity(plain));
}

// ---------------------------------------------------------------------------
// specificity: equality
// ---------------------------------------------------------------------------

TEST_CASE("identical selectors have equal specificity", "[wire][selector]") {
  selector_type a;
  a.qname = "{http://example.com}Foo";

  selector_type b;
  b.qname = "{http://example.com}Foo";

  CHECK(specificity(a) == specificity(b));
}

TEST_CASE("different qnames have equal specificity", "[wire][selector]") {
  selector_type a;
  a.qname = "{http://example.com}Foo";

  selector_type b;
  b.qname = "{http://example.com}Bar";

  CHECK(specificity(a) == specificity(b));
}

// ---------------------------------------------------------------------------
// specificity: sorting a vector of selectors
// ---------------------------------------------------------------------------

TEST_CASE("selectors sort by specificity", "[wire][selector]") {
  selector_type s_empty;

  selector_type s_ns;
  s_ns.namespace_ = "http://example.com";

  selector_type s_type;
  s_type.type = "xs:int";

  selector_type s_path;
  s_path.path = "A > B";

  selector_type s_qname;
  s_qname.qname = "{ns}Foo";

  std::vector<selector_type> selectors = {s_qname, s_empty, s_path, s_ns,
                                          s_type};
  std::sort(selectors.begin(), selectors.end(),
            [](const selector_type& a, const selector_type& b) {
              return specificity(a) < specificity(b);
            });

  CHECK(!selectors[0].namespace_.has_value()); // empty
  CHECK(selectors[1].namespace_.has_value());  // namespace
  CHECK(selectors[2].type.has_value());        // type
  CHECK(selectors[3].path.has_value());        // path
  CHECK(selectors[4].qname.has_value());       // qname
}

// ---------------------------------------------------------------------------
// specificity: transitivity
// ---------------------------------------------------------------------------

TEST_CASE("specificity is transitive", "[wire][selector]") {
  selector_type a;
  a.namespace_ = "http://example.com";

  selector_type b;
  b.type = "xs:int";

  selector_type c;
  c.path = "A > B";

  REQUIRE(specificity(a) < specificity(b));
  REQUIRE(specificity(b) < specificity(c));
  CHECK(specificity(a) < specificity(c));
}

// ---------------------------------------------------------------------------
// specificity: comparison operators
// ---------------------------------------------------------------------------

TEST_CASE("specificity supports all comparison operators", "[wire][selector]") {
  selector_type a;
  a.namespace_ = "http://example.com";

  selector_type b;
  b.qname = "{ns}Foo";

  auto sa = specificity(a);
  auto sb = specificity(b);

  CHECK(sa < sb);
  CHECK(sa <= sb);
  CHECK(sb > sa);
  CHECK(sb >= sa);
  CHECK(sa != sb);
  CHECK(sa == sa);
}

// ---------------------------------------------------------------------------
// more_specific: convenience function
// ---------------------------------------------------------------------------

TEST_CASE("more_specific returns the higher-specificity selector",
          "[wire][selector]") {
  selector_type a;
  a.type = "xs:int";

  selector_type b;
  b.qname = "{ns}Foo";

  CHECK(more_specific(a, b).qname.has_value());
  CHECK(more_specific(b, a).qname.has_value());
}

// ---------------------------------------------------------------------------
// selectors_conflict: equal specificity with different content
// ---------------------------------------------------------------------------

TEST_CASE("selectors_conflict detects equal-specificity different selectors",
          "[wire][selector]") {
  selector_type a;
  a.type = "xs:int";

  selector_type b;
  b.type = "xs:string";

  CHECK(specificity(a) == specificity(b));
  CHECK(selectors_conflict(a, b));
}

TEST_CASE("selectors_conflict returns false for different specificities",
          "[wire][selector]") {
  selector_type a;
  a.type = "xs:int";

  selector_type b;
  b.qname = "{ns}Foo";

  CHECK(!selectors_conflict(a, b));
}

TEST_CASE("selectors_conflict returns false for identical selectors",
          "[wire][selector]") {
  selector_type a;
  a.type = "xs:int";

  selector_type b;
  b.type = "xs:int";

  CHECK(!selectors_conflict(a, b));
}
