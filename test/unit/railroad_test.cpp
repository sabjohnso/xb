#include <xb/railroad.hpp>
#include <xb/railroad_svg.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/expat_reader.hpp>
#include <xb/model_group.hpp>
#include <xb/model_group_def.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>
#include <xb/wildcard.hpp>

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

// ---------------------------------------------------------------------------
// Wildcard particles (xs:any)
// ---------------------------------------------------------------------------

TEST_CASE("Wildcard particle produces a terminal, not '?'", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "name"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(xb::wildcard{}, xb::occurrence{0, xb::unbounded});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Open"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Open"});

  // The wildcard should NOT appear as "?"
  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();
  CHECK(contains(svg, "name"));
  CHECK(!contains(svg, ">?<"));
}

// ---------------------------------------------------------------------------
// Deeply nested structures
// ---------------------------------------------------------------------------

TEST_CASE("Deeply nested optional(repeat(choice)) renders without "
          "overlap",
          "[railroad][svg]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Inner choice: a | b
  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "a"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "b"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "c"}, xb::qname{xs_ns, "boolean"}},
      xb::occurrence{1, 1});

  // Wrap in: sequence containing optional(repeat(choice))
  auto choice_group = std::make_unique<xb::model_group>(
      xb::compositor_kind::choice, std::move(alts));

  std::vector<xb::particle> parts;
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "header"},
                                      xb::qname{xs_ns, "string"}},
                     xb::occurrence{1, 1});
  // choice with minOccurs=0 maxOccurs=unbounded → optional(repeat(choice))
  parts.emplace_back(std::move(choice_group), xb::occurrence{0, xb::unbounded});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Deep"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Deep"});

  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();

  // Should contain all three choice alternatives
  CHECK(contains(svg, ">a<"));
  CHECK(contains(svg, ">b<"));
  CHECK(contains(svg, ">c<"));
  // Should have both bypass (optional) and loop (repeat)
  CHECK(contains(svg, "<path")); // arcs for branching/looping

  // SVG dimensions should be positive and reasonable
  auto w_pos = svg.find("width=\"");
  REQUIRE(w_pos != std::string::npos);
  double w = std::stod(svg.substr(w_pos + 7));
  CHECK(w > 0);
  CHECK(w < 5000);

  auto h_pos = svg.find("height=\"");
  REQUIRE(h_pos != std::string::npos);
  double h = std::stod(svg.substr(h_pos + 8));
  CHECK(h > 0);
  CHECK(h < 5000);
}

// ---------------------------------------------------------------------------
// Recursive/cyclic type references
// ---------------------------------------------------------------------------

TEST_CASE("build_diagrams handles recursive type references", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Type A has element of type B
  std::vector<xb::particle> parts_a;
  parts_a.emplace_back(
      xb::element_decl{xb::qname{test_ns, "child"}, xb::qname{test_ns, "B"}},
      xb::occurrence{0, xb::unbounded});
  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "A"}, std::move(parts_a)));

  // Type B has element of type A (cycle)
  std::vector<xb::particle> parts_b;
  parts_b.emplace_back(
      xb::element_decl{xb::qname{test_ns, "parent"}, xb::qname{test_ns, "A"}},
      xb::occurrence{0, 1});
  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "B"}, std::move(parts_b)));

  s.add_element(
      xb::element_decl{xb::qname{test_ns, "root"}, xb::qname{test_ns, "A"}});

  auto ss = make_schema_set(std::move(s));
  auto diagrams = xb::railroad::build_diagrams(ss, xb::qname{test_ns, "root"});

  // Should produce exactly two diagrams (A and B), no infinite loop
  CHECK(diagrams.size() == 2);

  bool has_a = false, has_b = false;
  for (const auto& d : diagrams) {
    if (d.name == "A") has_a = true;
    if (d.name == "B") has_b = true;
  }
  CHECK(has_a);
  CHECK(has_b);
}

// ---------------------------------------------------------------------------
// Group references (xs:group ref)
// ---------------------------------------------------------------------------

TEST_CASE("Group reference is resolved in diagram", "[railroad]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Define a named model group: ItemGroup = choice { x, y }
  xb::model_group group_content(xb::compositor_kind::choice);
  group_content.add_particle(xb::particle{
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1}});
  group_content.add_particle(xb::particle{
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1}});

  s.add_model_group_def(xb::model_group_def{xb::qname{test_ns, "ItemGroup"},
                                            std::move(group_content)});

  // Complex type uses the group ref
  std::vector<xb::particle> parts;
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "header"},
                                      xb::qname{xs_ns, "string"}},
                     xb::occurrence{1, 1});
  parts.emplace_back(xb::group_ref{xb::qname{test_ns, "ItemGroup"}},
                     xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Container"}, std::move(parts)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Container"});

  // The group content should be inlined — x and y should appear
  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();

  CHECK(contains(svg, "header"));
  CHECK(contains(svg, ">x<"));
  CHECK(contains(svg, ">y<"));
  // Should NOT contain "ItemGroup" as a box — it's inlined
  CHECK(!contains(svg, "ItemGroup"));
}

// ---------------------------------------------------------------------------
// SVG well-formedness
// ---------------------------------------------------------------------------

TEST_CASE("Generated SVG is well-formed XML", "[railroad][svg]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Build a non-trivial schema with choice + optional + repeat
  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "text"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "num"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  auto choice_group = std::make_unique<xb::model_group>(
      xb::compositor_kind::choice, std::move(alts));

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "id"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(std::move(choice_group), xb::occurrence{0, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "tag"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, xb::unbounded});

  std::vector<xb::attribute_use> attrs;
  attrs.push_back({xb::qname{test_ns, "version"}, xb::qname{xs_ns, "string"},
                   false, std::nullopt, std::nullopt});

  s.add_complex_type(make_sequence_type(xb::qname{test_ns, "Complex"},
                                        std::move(parts), std::move(attrs)));

  auto ss = make_schema_set(std::move(s));
  auto diag = xb::railroad::build_diagram(ss, xb::qname{test_ns, "Complex"});

  std::ostringstream out;
  xb::railroad::render_svg(diag, out);
  std::string svg = out.str();

  // Parse the SVG with expat to verify well-formedness
  REQUIRE_NOTHROW([&] {
    xb::expat_reader reader(svg);
    while (reader.read()) {
      // Just consume all events — if parsing completes, it's well-formed
    }
  }());

  // Basic structural checks
  CHECK(contains(svg, "<svg"));
  CHECK(contains(svg, "</svg>"));

  // Dimensions should be positive numbers
  auto w_pos = svg.find("width=\"");
  REQUIRE(w_pos != std::string::npos);
  double w = std::stod(svg.substr(w_pos + 7));
  CHECK(w > 0);

  auto h_pos = svg.find("height=\"");
  REQUIRE(h_pos != std::string::npos);
  double h = std::stod(svg.substr(h_pos + 8));
  CHECK(h > 0);
}
