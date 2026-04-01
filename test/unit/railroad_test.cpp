#include <xb/railroad.hpp>
#include <xb/railroad_svg.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
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

  bool
  contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
  }

} // namespace

// ---------------------------------------------------------------------------
// IR construction: sequence type
// ---------------------------------------------------------------------------

TEST_CASE("build_diagram for sequence type produces sequence node",
          "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "name"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "age"},
                                      xb::qname{xs_ns, "unsignedByte"}},
                     xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Person"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Person"});

  CHECK(diag.name == "Person");
  auto* seq = std::get_if<xb::railroad::sequence>(&diag.root.content);
  REQUIRE(seq != nullptr);
  CHECK(seq->children.size() == 2);

  auto* t0 = std::get_if<xb::railroad::terminal>(&seq->children[0].content);
  REQUIRE(t0 != nullptr);
  CHECK(t0->label == "name");

  auto* t1 = std::get_if<xb::railroad::terminal>(&seq->children[1].content);
  REQUIRE(t1 != nullptr);
  CHECK(t1->label == "age");
}

// ---------------------------------------------------------------------------
// IR construction: choice type
// ---------------------------------------------------------------------------

TEST_CASE("build_diagram for choice type produces choice node", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "text"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "number"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_choice_type(xb::qname{test_ns, "Value"}, std::move(alts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Value"});

  auto* ch = std::get_if<xb::railroad::choice>(&diag.root.content);
  REQUIRE(ch != nullptr);
  CHECK(ch->alternatives.size() == 2);
}

// ---------------------------------------------------------------------------
// IR construction: optional element → optional_node
// ---------------------------------------------------------------------------

TEST_CASE("Optional element produces optional_node wrapper", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "required"},
                                      xb::qname{xs_ns, "string"}},
                     xb::occurrence{1, 1});
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "optional"},
                                      xb::qname{xs_ns, "string"}},
                     xb::occurrence{0, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "T"});

  auto* seq = std::get_if<xb::railroad::sequence>(&diag.root.content);
  REQUIRE(seq != nullptr);
  REQUIRE(seq->children.size() == 2);

  // Second child should be optional_node
  auto* opt =
      std::get_if<xb::railroad::optional_node>(&seq->children[1].content);
  CHECK(opt != nullptr);
}

// ---------------------------------------------------------------------------
// IR construction: repeating element → repeat_node
// ---------------------------------------------------------------------------

TEST_CASE("Repeating element produces repeat_node", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "item"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, xb::unbounded});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "List"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "List"});

  // Single-element sequence is collapsed — root is the repeat directly
  auto* rep = std::get_if<xb::railroad::repeat_node>(&diag.root.content);
  REQUIRE(rep != nullptr);
  CHECK(rep->count_label == "1..*");
}

// ---------------------------------------------------------------------------
// IR construction: optional + repeating → optional(repeat)
// ---------------------------------------------------------------------------

TEST_CASE("Optional repeating element produces optional(repeat)",
          "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "item"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{0, xb::unbounded});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "T"});

  // Single-element sequence is collapsed — root is optional directly
  auto* opt = std::get_if<xb::railroad::optional_node>(&diag.root.content);
  REQUIRE(opt != nullptr);
  auto* rep = std::get_if<xb::railroad::repeat_node>(&opt->child->content);
  CHECK(rep != nullptr);
}

// ---------------------------------------------------------------------------
// IR construction: attributes collected
// ---------------------------------------------------------------------------

TEST_CASE("Attributes are collected in diagram", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  std::vector<xb::attribute_use> attrs;
  attrs.push_back({xb::qname{test_ns, "id"}, xb::qname{xs_ns, "string"}, true,
                   std::nullopt, std::nullopt});
  attrs.push_back({xb::qname{test_ns, "lang"}, xb::qname{xs_ns, "string"},
                   false, std::nullopt, std::nullopt});

  s.add_complex_type(make_sequence_type(xb::qname{test_ns, "T"},
                                        std::move(parts), std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "T"});

  REQUIRE(diag.attributes.size() == 2);
  CHECK(diag.attributes[0].label == "id");
  CHECK(diag.attributes[1].label == "lang");
}

// ---------------------------------------------------------------------------
// SVG rendering: produces valid SVG
// ---------------------------------------------------------------------------

TEST_CASE("render_svg produces well-formed SVG", "[railroad][svg]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "name"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "age"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{0, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Person"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Person"});

  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();

  CHECK(contains(svg, "<svg"));
  CHECK(contains(svg, "</svg>"));
  CHECK(contains(svg, "name"));
  CHECK(contains(svg, "age"));
  CHECK(contains(svg, "Person"));
}

// ---------------------------------------------------------------------------
// SVG rendering: choice has branching paths
// ---------------------------------------------------------------------------

TEST_CASE("SVG for choice type contains path elements", "[railroad][svg]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "a"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "b"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_choice_type(xb::qname{test_ns, "C"}, std::move(alts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "C"});

  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();

  CHECK(contains(svg, "<path"));
  CHECK(contains(svg, "a"));
  CHECK(contains(svg, "b"));
}

// ---------------------------------------------------------------------------
// Multiple diagrams in one SVG
// ---------------------------------------------------------------------------

TEST_CASE("render_svg with multiple diagrams", "[railroad][svg]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts1;
  parts1.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});
  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "A"}, std::move(parts1)));

  std::vector<xb::particle> parts2;
  parts2.emplace_back(
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "B"}, std::move(parts2)));

  s.add_element(
      xb::element_decl{xb::qname{test_ns, "root"}, xb::qname{test_ns, "A"}});

  auto ss = make_schema_set(std::move(s));

  std::vector<xb::railroad::diagram> diagrams;
  diagrams.push_back(xb::railroad::build_diagram(ss, xb::qname{test_ns, "A"}));
  diagrams.push_back(xb::railroad::build_diagram(ss, xb::qname{test_ns, "B"}));

  std::ostringstream out;
  xb::railroad::render_svg(diagrams, out);
  std::string svg = out.str();

  CHECK(contains(svg, "A"));
  CHECK(contains(svg, "B"));
  // Should be one SVG document
  CHECK(svg.find("<svg") != std::string::npos);
  // Only one closing svg tag
  auto first_close = svg.find("</svg>");
  CHECK(first_close != std::string::npos);
  CHECK(svg.find("</svg>", first_close + 1) == std::string::npos);
}
