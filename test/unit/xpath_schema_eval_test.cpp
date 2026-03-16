#include <xb/xpath/schema_eval.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb::xpath;

// ---------------------------------------------------------------------------
// Helpers: build XSD schemas with element structure
// ---------------------------------------------------------------------------

namespace {

  const std::string ns = "http://example.com";

  xb::complex_type
  make_simple_type(const std::string& type_name) {
    return xb::complex_type(xb::qname{ns, type_name}, false, false,
                            xb::content_type{xb::content_kind::element_only,
                                             xb::complex_content{}});
  }

  xb::schema_set
  make_test_schema() {
    xb::schema s;
    s.set_target_namespace(ns);

    s.add_complex_type(make_simple_type("OrderMessage"));
    s.add_complex_type(make_simple_type("Instrument"));
    s.add_complex_type(make_simple_type("Trade"));

    xb::schema_set ss;
    ss.add(std::move(s));
    ss.resolve();
    return ss;
  }

} // namespace

// ---------------------------------------------------------------------------
// Simple name matching
// ---------------------------------------------------------------------------

TEST_CASE("xpath_eval: match type by name", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("OrderMessage", schemas);

  REQUIRE(matches.size() == 1);
  CHECK(matches[0] == xb::qname{ns, "OrderMessage"});
}

TEST_CASE("xpath_eval: match multiple types by wildcard *", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("*", schemas);

  // Should match all complex types
  CHECK(matches.size() == 3);
}

// ---------------------------------------------------------------------------
// Descendant paths
// ---------------------------------------------------------------------------

TEST_CASE("xpath_eval: //name matches any type by name", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("//Trade", schemas);

  REQUIRE(matches.size() == 1);
  CHECK(matches[0] == xb::qname{ns, "Trade"});
}

// ---------------------------------------------------------------------------
// Predicate matching
// ---------------------------------------------------------------------------

TEST_CASE("xpath_eval: *[@name='OrderMessage'] matches by name attribute",
          "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("*[@name='OrderMessage']", schemas);

  REQUIRE(matches.size() == 1);
  CHECK(matches[0] == xb::qname{ns, "OrderMessage"});
}

TEST_CASE("xpath_eval: *[@name='Nonexistent'] returns empty", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("*[@name='Nonexistent']", schemas);

  CHECK(matches.empty());
}

// ---------------------------------------------------------------------------
// No match
// ---------------------------------------------------------------------------

TEST_CASE("xpath_eval: non-matching name returns empty", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("Nonexistent", schemas);

  CHECK(matches.empty());
}

TEST_CASE("xpath_eval: invalid xpath returns empty", "[xpath][eval]") {
  auto schemas = make_test_schema();
  auto matches = eval_schema_path("", schemas);

  CHECK(matches.empty());
}
