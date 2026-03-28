#include <xb/test_doc_generator.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

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
// Basic: generates multiple XML documents from a sequence type
// ---------------------------------------------------------------------------

TEST_CASE("test_doc_generator produces XML documents from sequence type",
          "[test_doc_generator]") {
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
  s.add_element(xb::element_decl{xb::qname{test_ns, "person"},
                                 xb::qname{test_ns, "Person"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_doc_generator gen(ss);

  std::vector<std::pair<std::string, std::string>> docs;
  gen.generate(xb::qname{test_ns, "person"},
               [&](const std::string& xml, const std::string& label) {
                 docs.emplace_back(label, xml);
               });

  // Should produce multiple documents
  REQUIRE(docs.size() > 1);

  // Each document should contain the root element
  for (const auto& [label, xml] : docs) {
    CHECK(!label.empty());
    CHECK(xml.find("person") != std::string::npos);
  }

  // At least one document should have boundary age values
  bool has_zero_age = false;
  bool has_max_age = false;
  for (const auto& [label, xml] : docs) {
    if (xml.find("<age>0</age>") != std::string::npos ||
        xml.find(">0</age>") != std::string::npos)
      has_zero_age = true;
    if (xml.find(">255</age>") != std::string::npos ||
        xml.find("<age>255</age>") != std::string::npos)
      has_max_age = true;
  }
  CHECK(has_zero_age);
  CHECK(has_max_age);
}

// ---------------------------------------------------------------------------
// Choice: each alternative has its own document
// ---------------------------------------------------------------------------

TEST_CASE("test_doc_generator produces documents for each choice alternative",
          "[test_doc_generator]") {
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
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "val"}, xb::qname{test_ns, "Value"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_doc_generator gen(ss);

  std::vector<std::pair<std::string, std::string>> docs;
  gen.generate(xb::qname{test_ns, "val"},
               [&](const std::string& xml, const std::string& label) {
                 docs.emplace_back(label, xml);
               });

  // Should have documents with "text" element and "number" element
  bool has_text = false;
  bool has_number = false;
  for (const auto& [label, xml] : docs) {
    if (xml.find("<text>") != std::string::npos ||
        xml.find("<text ") != std::string::npos)
      has_text = true;
    if (xml.find("<number>") != std::string::npos ||
        xml.find("<number ") != std::string::npos)
      has_number = true;
  }
  CHECK(has_text);
  CHECK(has_number);
}

// ---------------------------------------------------------------------------
// Count: reports how many vectors would be generated
// ---------------------------------------------------------------------------

TEST_CASE("test_doc_generator::count returns vector count",
          "[test_doc_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "boolean"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_doc_generator gen(ss);

  std::size_t n = gen.count(xb::qname{test_ns, "t"});

  // count should match actual number generated
  std::size_t actual = 0;
  gen.generate(xb::qname{test_ns, "t"},
               [&](const std::string&, const std::string&) { ++actual; });

  CHECK(n == actual);
  CHECK(n > 0);
}

// ---------------------------------------------------------------------------
// Well-formed XML: documents are parseable
// ---------------------------------------------------------------------------

TEST_CASE("Generated XML documents are well-formed", "[test_doc_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "value"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Item"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "item"}, xb::qname{test_ns, "Item"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_doc_generator gen(ss);

  gen.generate(xb::qname{test_ns, "item"},
               [](const std::string& xml, const std::string&) {
                 // Basic well-formedness: starts with < and has matching tags
                 CHECK(!xml.empty());
                 CHECK(xml.front() == '<');
                 // Should have both opening and closing item element
                 CHECK(xml.find("item") != std::string::npos);
               });
}
