#include <xb/schematron_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace sch = xb::schematron;

// -- assert_or_report ---------------------------------------------------------

TEST_CASE("schematron model: assert", "[schematron_model]") {
  sch::assert_or_report ar;
  ar.is_assert = true;
  ar.test = "count(item) > 0";
  ar.message = "At least one item is required";
  CHECK(ar.is_assert == true);
  CHECK(ar.test == "count(item) > 0");
}

TEST_CASE("schematron model: report", "[schematron_model]") {
  sch::assert_or_report ar;
  ar.is_assert = false;
  ar.test = "count(item) > 100";
  ar.message = "More than 100 items found";
  CHECK(ar.is_assert == false);
}

// -- rule ---------------------------------------------------------------------

TEST_CASE("schematron model: rule with checks", "[schematron_model]") {
  sch::assert_or_report a;
  a.is_assert = true;
  a.test = "total > 0";
  a.message = "Total must be positive";

  sch::rule r;
  r.context = "invoice";
  r.checks.push_back(std::move(a));

  CHECK(r.context == "invoice");
  CHECK(r.checks.size() == 1);
}

// -- pattern ------------------------------------------------------------------

TEST_CASE("schematron model: pattern with rules", "[schematron_model]") {
  sch::rule r;
  r.context = "invoice";

  sch::pattern p;
  p.id = "invoice-rules";
  p.name = "Invoice Validation";
  p.rules.push_back(std::move(r));

  CHECK(p.id == "invoice-rules");
  CHECK(p.rules.size() == 1);
}

// -- namespace_binding --------------------------------------------------------

TEST_CASE("schematron model: namespace binding", "[schematron_model]") {
  sch::namespace_binding ns;
  ns.prefix = "inv";
  ns.uri = "urn:example:invoice";
  CHECK(ns.prefix == "inv");
  CHECK(ns.uri == "urn:example:invoice");
}

// -- schema -------------------------------------------------------------------

TEST_CASE("schematron model: schema collects all", "[schematron_model]") {
  sch::schema s;
  s.title = "Invoice Validation Rules";

  sch::namespace_binding ns;
  ns.prefix = "inv";
  ns.uri = "urn:example:invoice";
  s.namespaces.push_back(std::move(ns));

  sch::pattern p;
  p.id = "basic";
  s.patterns.push_back(std::move(p));

  CHECK(s.title == "Invoice Validation Rules");
  CHECK(s.namespaces.size() == 1);
  CHECK(s.patterns.size() == 1);
}

// -- phase --------------------------------------------------------------------

TEST_CASE("schematron model: phase selects patterns", "[schematron_model]") {
  sch::phase ph;
  ph.id = "basic";
  ph.active_patterns = {"invoice-rules", "address-rules"};
  CHECK(ph.id == "basic");
  CHECK(ph.active_patterns.size() == 2);
}
