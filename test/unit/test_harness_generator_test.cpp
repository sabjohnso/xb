#include <xb/test_harness_generator.hpp>

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
  make_sequence_type(const xb::qname& name,
                     std::vector<xb::particle> particles) {
    xb::model_group seq(xb::compositor_kind::sequence, std::move(particles));
    xb::complex_content cc(xb::qname{xs_ns, "anyType"},
                           xb::derivation_method::restriction, std::move(seq));
    return xb::complex_type(
        name, false, false,
        xb::content_type(xb::content_kind::element_only, std::move(cc)));
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
// Basic: generates Catch2 test source with TEST_CASE blocks
// ---------------------------------------------------------------------------

TEST_CASE("Generates Catch2 test source for sequence type",
          "[test_harness_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "stock"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "qty"},
                                      xb::qname{xs_ns, "unsignedInt"}},
                     xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "OrderType"}, std::move(parts)));
  s.add_element(xb::element_decl{xb::qname{test_ns, "order"},
                                 xb::qname{test_ns, "OrderType"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_harness_generator gen(ss);

  std::ostringstream out;
  gen.generate(xb::qname{test_ns, "order"}, out);
  std::string src = out.str();

  // Should include Catch2 header
  CHECK(contains(src, "#include <catch2/catch_test_macros.hpp>"));

  // Should have TEST_CASE blocks
  CHECK(contains(src, "TEST_CASE("));

  // Should reference the generated type
  CHECK(contains(src, "order_type"));

  // Should have field assignments
  CHECK(contains(src, "stock"));
  CHECK(contains(src, "qty"));

  // Should have round-trip logic
  CHECK(contains(src, "write_order_type"));
  CHECK(contains(src, "read_order_type"));
  CHECK(contains(src, "REQUIRE("));
}

// ---------------------------------------------------------------------------
// Choice: test cases for each alternative
// ---------------------------------------------------------------------------

TEST_CASE("Generates test cases for each choice alternative",
          "[test_harness_generator]") {
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
  xb::test_harness_generator gen(ss);

  std::ostringstream out;
  gen.generate(xb::qname{test_ns, "val"}, out);
  std::string src = out.str();

  // Should have multiple TEST_CASE blocks
  std::size_t test_case_count = 0;
  std::string::size_type pos = 0;
  while ((pos = src.find("TEST_CASE(", pos)) != std::string::npos) {
    ++test_case_count;
    pos += 10;
  }
  CHECK(test_case_count >= 2);

  // Should reference both alternatives
  CHECK(contains(src, "text"));
  CHECK(contains(src, "number"));
}

// ---------------------------------------------------------------------------
// Includes proper headers
// ---------------------------------------------------------------------------

TEST_CASE("Generated source includes necessary headers",
          "[test_harness_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_harness_generator gen(ss);

  std::ostringstream out;
  gen.generate(xb::qname{test_ns, "t"}, out);
  std::string src = out.str();

  CHECK(contains(src, "#include <catch2/catch_test_macros.hpp>"));
  CHECK(contains(src, "#include <xb/ostream_writer.hpp>"));
  CHECK(contains(src, "#include <xb/expat_reader.hpp>"));
  CHECK(contains(src, "#include <sstream>"));
}

// ---------------------------------------------------------------------------
// Uses correct C++ namespace from XML namespace
// ---------------------------------------------------------------------------

TEST_CASE("Generated source uses C++ namespace derived from XML namespace",
          "[test_harness_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "val"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "MyType"}, std::move(parts)));
  s.add_element(xb::element_decl{xb::qname{test_ns, "item"},
                                 xb::qname{test_ns, "MyType"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_harness_generator gen(ss);

  std::ostringstream out;
  gen.generate(xb::qname{test_ns, "item"}, out);
  std::string src = out.str();

  // Namespace derived from "http://example.com/test" → "test" or similar
  CHECK(contains(src, "test::"));
}
