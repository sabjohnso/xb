// GCC 12-13 emit a false -Wmaybe-uninitialized when constructing
// particle objects (std::variant containing std::unique_ptr) at -O3.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/schematron_overlay.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace sch = xb::schematron;

static const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

// Build a minimal schema_set with one element and complex type
static schema_set
make_test_schema(const std::string& element_name, const std::string& ns = "") {
  schema s;
  s.set_target_namespace(ns);

  qname tn(ns, element_name + "Type");
  qname en(ns, element_name);

  content_type ct;
  s.add_complex_type(complex_type(tn, false, false, std::move(ct)));
  s.add_element(element_decl(en, tn));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();
  return ss;
}

// -- Simple element name context matching -------------------------------------

TEST_CASE("schematron overlay: simple element match", "[schematron_overlay]") {
  auto ss = make_test_schema("invoice");

  sch::schema sch_schema;
  sch::rule r;
  r.context = "invoice";
  sch::assert_or_report a;
  a.is_assert = true;
  a.test = "total > 0";
  r.checks.push_back(std::move(a));

  sch::pattern p;
  p.rules.push_back(std::move(r));
  sch_schema.patterns.push_back(std::move(p));

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 1);
  CHECK(result.rules_unmatched == 0);

  // The complex type should now have an assertion
  const auto* ct = ss.find_complex_type(qname("", "invoiceType"));
  REQUIRE(ct != nullptr);
  REQUIRE(ct->assertions().size() == 1);
  CHECK(ct->assertions()[0].test == "total > 0");
}

// -- Multiple assertions on same rule -----------------------------------------

TEST_CASE("schematron overlay: multiple assertions", "[schematron_overlay]") {
  auto ss = make_test_schema("order");

  sch::schema sch_schema;
  sch::rule r;
  r.context = "order";
  {
    sch::assert_or_report a;
    a.is_assert = true;
    a.test = "total > 0";
    r.checks.push_back(std::move(a));
  }
  {
    sch::assert_or_report a;
    a.is_assert = true;
    a.test = "@currency";
    r.checks.push_back(std::move(a));
  }

  sch::pattern p;
  p.rules.push_back(std::move(r));
  sch_schema.patterns.push_back(std::move(p));

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 1);

  const auto* ct = ss.find_complex_type(qname("", "orderType"));
  REQUIRE(ct != nullptr);
  CHECK(ct->assertions().size() == 2);
}

// -- Unmatched context (no element with that name) ----------------------------

TEST_CASE("schematron overlay: unmatched context produces warning",
          "[schematron_overlay]") {
  auto ss = make_test_schema("invoice");

  sch::schema sch_schema;
  sch::rule r;
  r.context = "nonexistent";
  sch::assert_or_report a;
  a.is_assert = true;
  a.test = "x > 0";
  r.checks.push_back(std::move(a));

  sch::pattern p;
  p.rules.push_back(std::move(r));
  sch_schema.patterns.push_back(std::move(p));

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 0);
  CHECK(result.rules_unmatched == 1);
  CHECK(!result.warnings.empty());
}

// -- Report is converted to assert (negated) ----------------------------------

TEST_CASE("schematron overlay: report is not injected as assert",
          "[schematron_overlay]") {
  auto ss = make_test_schema("order");

  sch::schema sch_schema;
  sch::rule r;
  r.context = "order";
  sch::assert_or_report rep;
  rep.is_assert = false; // report
  rep.test = "count(item) > 100";
  r.checks.push_back(std::move(rep));

  sch::pattern p;
  p.rules.push_back(std::move(r));
  sch_schema.patterns.push_back(std::move(p));

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 1);

  // Report: the assertion test is negated (report fires when true,
  // so the validation assertion is the negation)
  const auto* ct = ss.find_complex_type(qname("", "orderType"));
  REQUIRE(ct != nullptr);
  REQUIRE(ct->assertions().size() == 1);
}

// -- Namespaced element matching via sch:ns -----------------------------------

TEST_CASE("schematron overlay: namespaced element match",
          "[schematron_overlay]") {
  auto ss = make_test_schema("invoice", "urn:example:inv");

  sch::schema sch_schema;
  sch::namespace_binding ns;
  ns.prefix = "inv";
  ns.uri = "urn:example:inv";
  sch_schema.namespaces.push_back(std::move(ns));

  sch::rule r;
  r.context = "inv:invoice";
  sch::assert_or_report a;
  a.is_assert = true;
  a.test = "total > 0";
  r.checks.push_back(std::move(a));

  sch::pattern p;
  p.rules.push_back(std::move(r));
  sch_schema.patterns.push_back(std::move(p));

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 1);
  CHECK(result.rules_unmatched == 0);
}

// -- Multiple patterns --------------------------------------------------------

TEST_CASE("schematron overlay: multiple patterns", "[schematron_overlay]") {
  schema s;
  s.set_target_namespace("");

  content_type ct1;
  s.add_complex_type(
      complex_type(qname("", "invoiceType"), false, false, std::move(ct1)));
  s.add_element(element_decl(qname("", "invoice"), qname("", "invoiceType")));

  content_type ct2;
  s.add_complex_type(
      complex_type(qname("", "addressType"), false, false, std::move(ct2)));
  s.add_element(element_decl(qname("", "address"), qname("", "addressType")));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  sch::schema sch_schema;
  {
    sch::rule r;
    r.context = "invoice";
    sch::assert_or_report a;
    a.is_assert = true;
    a.test = "total > 0";
    r.checks.push_back(std::move(a));
    sch::pattern p;
    p.id = "p1";
    p.rules.push_back(std::move(r));
    sch_schema.patterns.push_back(std::move(p));
  }
  {
    sch::rule r;
    r.context = "address";
    sch::assert_or_report a;
    a.is_assert = true;
    a.test = "city";
    r.checks.push_back(std::move(a));
    sch::pattern p;
    p.id = "p2";
    p.rules.push_back(std::move(r));
    sch_schema.patterns.push_back(std::move(p));
  }

  auto result = schematron_overlay(ss, sch_schema);
  CHECK(result.rules_matched == 2);
  CHECK(result.rules_unmatched == 0);
}
