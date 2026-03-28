#include <xb/coverage_analyzer.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace {

  const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
  const std::string test_ns = "http://example.com/test";

  xb::schema_set
  make_schema_set(xb::schema s) {
    xb::schema_set ss;
    ss.add(std::move(s));
    ss.resolve();
    return ss;
  }

  xb::complex_type
  make_sequence_type(const xb::qname& name, std::vector<xb::particle> particles,
                     std::vector<xb::attribute_use> attrs = {}) {
    xb::model_group seq(xb::compositor_kind::sequence, std::move(particles));
    xb::complex_content cc(xb::qname{xs_ns, "anyType"},
                           xb::derivation_method::restriction, std::move(seq));
    return xb::complex_type(
        name, false, false,
        xb::content_type(xb::content_kind::element_only, std::move(cc)),
        std::move(attrs));
  }

  xb::complex_type
  make_choice_type(const xb::qname& name,
                   std::vector<xb::particle> alternatives) {
    xb::model_group choice(xb::compositor_kind::choice,
                           std::move(alternatives));
    xb::complex_content cc(xb::qname{xs_ns, "anyType"},
                           xb::derivation_method::restriction,
                           std::move(choice));
    return xb::complex_type(
        name, false, false,
        xb::content_type(xb::content_kind::element_only, std::move(cc)));
  }

} // namespace

// ---------------------------------------------------------------------------
// Sequence type: counts types, boundary values, optional combos
// ---------------------------------------------------------------------------

TEST_CASE("Coverage analysis for sequence with required and optional fields",
          "[coverage_analyzer]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "name"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "age"},
                                      xb::qname{xs_ns, "unsignedByte"}},
                     xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "email"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{0, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Person"}, std::move(parts)));
  s.add_element(xb::element_decl{xb::qname{test_ns, "person"},
                                 xb::qname{test_ns, "Person"}});

  auto ss = make_schema_set(std::move(s));
  xb::coverage_analyzer analyzer(ss);

  auto report = analyzer.analyze(xb::qname{test_ns, "person"});

  // Should have covered the complex type
  CHECK(report.covered_types >= 1);
  CHECK(report.total_types >= 1);

  // Boundary values for unsignedByte (6) + string (2) + optional string (2)
  CHECK(report.total_boundary_values >= 6);
  CHECK(report.covered_boundary_values == report.total_boundary_values);

  // One optional field → 2 combos (present/absent)
  CHECK(report.total_optional_combos == 2);
  CHECK(report.covered_optional_combos == 2);

  // No choices
  CHECK(report.total_choice_alts == 0);
}

// ---------------------------------------------------------------------------
// Choice type: counts alternatives
// ---------------------------------------------------------------------------

TEST_CASE("Coverage analysis for choice type", "[coverage_analyzer]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "text"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "number"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "flag"}, xb::qname{xs_ns, "boolean"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_choice_type(xb::qname{test_ns, "Value"}, std::move(alts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "val"}, xb::qname{test_ns, "Value"}});

  auto ss = make_schema_set(std::move(s));
  xb::coverage_analyzer analyzer(ss);

  auto report = analyzer.analyze(xb::qname{test_ns, "val"});

  CHECK(report.total_choice_alts == 3);
  CHECK(report.covered_choice_alts == 3);
}

// ---------------------------------------------------------------------------
// Enumeration type: counts enum values
// ---------------------------------------------------------------------------

TEST_CASE("Coverage analysis counts enumeration values",
          "[coverage_analyzer]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  xb::facet_set facets;
  facets.enumeration = {"red", "green", "blue", "yellow"};

  s.add_simple_type(xb::simple_type{xb::qname{test_ns, "Color"},
                                    xb::simple_type_variety::atomic,
                                    xb::qname{xs_ns, "string"}, facets});

  std::vector<xb::particle> parts;
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "color"},
                                      xb::qname{test_ns, "Color"}},
                     xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Item"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "item"}, xb::qname{test_ns, "Item"}});

  auto ss = make_schema_set(std::move(s));
  xb::coverage_analyzer analyzer(ss);

  auto report = analyzer.analyze(xb::qname{test_ns, "item"});

  CHECK(report.total_enum_values == 4);
  CHECK(report.covered_enum_values == 4);
}

// ---------------------------------------------------------------------------
// Print produces readable output
// ---------------------------------------------------------------------------

TEST_CASE("Coverage report prints readable summary", "[coverage_analyzer]") {
  xb::coverage_report report{};
  report.total_types = 5;
  report.covered_types = 5;
  report.total_choice_alts = 10;
  report.covered_choice_alts = 10;
  report.total_enum_values = 20;
  report.covered_enum_values = 20;
  report.total_boundary_values = 50;
  report.covered_boundary_values = 50;
  report.total_optional_combos = 8;
  report.covered_optional_combos = 6;

  std::ostringstream out;
  xb::coverage_analyzer::print(report, out);
  std::string text = out.str();

  CHECK(text.find("Types:") != std::string::npos);
  CHECK(text.find("100.0%") != std::string::npos);
  CHECK(text.find("CHOICE") != std::string::npos);
  CHECK(text.find("Enum") != std::string::npos);
  CHECK(text.find("Boundary") != std::string::npos);
  CHECK(text.find("Optional") != std::string::npos);
  CHECK(text.find("75.0%") != std::string::npos); // 6/8
}

// ---------------------------------------------------------------------------
// Optional attribute contributes to optional combos
// ---------------------------------------------------------------------------

TEST_CASE("Coverage analysis counts optional attributes",
          "[coverage_analyzer]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  std::vector<xb::attribute_use> attrs;
  attrs.push_back({xb::qname{test_ns, "opt1"}, xb::qname{xs_ns, "string"},
                   false, std::nullopt, std::nullopt});
  attrs.push_back({xb::qname{test_ns, "opt2"}, xb::qname{xs_ns, "int"}, false,
                   std::nullopt, std::nullopt});

  s.add_complex_type(make_sequence_type(xb::qname{test_ns, "T"},
                                        std::move(parts), std::move(attrs)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::coverage_analyzer analyzer(ss);

  auto report = analyzer.analyze(xb::qname{test_ns, "t"});

  // 2 optional attrs × 2 (present/absent) = 4 combos
  CHECK(report.total_optional_combos == 4);
  CHECK(report.covered_optional_combos == 4);
}
