#include <xb/test_vector_generator.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/facet_set.hpp>
#include <xb/model_group.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>
#include <xb/simple_type.hpp>
#include <xb/test_value_generator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

  // Build a simple complex type with a sequence of elements.
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

  // Build a complex type with a choice group.
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

  // Helper: count vectors with a given label prefix.
  std::size_t
  count_with_label(const std::vector<xb::test_vector>& vectors,
                   const std::string& prefix) {
    return static_cast<std::size_t>(std::count_if(
        vectors.begin(), vectors.end(), [&](const xb::test_vector& v) {
          return v.label.substr(0, prefix.size()) == prefix;
        }));
  }

  // Helper: find a field by local name in a vector of field values.
  const xb::test_field_value*
  find_field(const std::vector<xb::test_field_value>& fields,
             const std::string& local_name) {
    auto it = std::find_if(fields.begin(), fields.end(),
                           [&](const xb::test_field_value& f) {
                             return f.field_name.local_name() == local_name;
                           });
    return it != fields.end() ? &*it : nullptr;
  }

} // namespace

// ---------------------------------------------------------------------------
// Simple sequence: all required fields
// ---------------------------------------------------------------------------

TEST_CASE("Sequence with required fields produces one vector per value "
          "combination",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // complexType Order { stock: xs:string, qty: xs:unsignedInt }
  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "stock"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "qty"},
                                      xb::qname{xs_ns, "unsignedInt"}},
                     xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Order"}, std::move(parts)));
  s.add_element(xb::element_decl{xb::qname{test_ns, "order"},
                                 xb::qname{test_ns, "Order"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "order"});

  // Should produce at least one vector
  REQUIRE(!vectors.empty());

  // Each vector should have both fields
  for (const auto& vec : vectors) {
    CHECK(find_field(vec.fields, "stock") != nullptr);
    CHECK(find_field(vec.fields, "qty") != nullptr);
  }

  // The qty field should contain boundary values from xs:unsignedInt
  bool has_zero_qty = false;
  bool has_max_qty = false;
  for (const auto& vec : vectors) {
    const auto* qty = find_field(vec.fields, "qty");
    if (qty) {
      const auto* val = std::get_if<std::string>(&qty->content);
      if (val) {
        if (*val == "0") has_zero_qty = true;
        if (*val == "4294967295") has_max_qty = true;
      }
    }
  }
  CHECK(has_zero_qty);
  CHECK(has_max_qty);
}

// ---------------------------------------------------------------------------
// Choice: one vector per alternative
// ---------------------------------------------------------------------------

TEST_CASE("Choice produces one vector per alternative",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // complexType Msg { choice { a: xs:string, b: xs:int, c: xs:boolean } }
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

  s.add_complex_type(
      make_choice_type(xb::qname{test_ns, "Msg"}, std::move(alts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "msg"}, xb::qname{test_ns, "Msg"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "msg"});

  // At least 3 vectors: one per choice alternative
  REQUIRE(vectors.size() >= 3);

  // Check that each alternative appears at least once
  bool has_a = false, has_b = false, has_c = false;
  for (const auto& vec : vectors) {
    if (find_field(vec.fields, "a")) has_a = true;
    if (find_field(vec.fields, "b")) has_b = true;
    if (find_field(vec.fields, "c")) has_c = true;
  }
  CHECK(has_a);
  CHECK(has_b);
  CHECK(has_c);
}

// ---------------------------------------------------------------------------
// Optional element: present and absent vectors
// ---------------------------------------------------------------------------

TEST_CASE("Optional element generates present and absent vectors",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // complexType Rec { required: xs:string, opt: xs:int (minOccurs=0) }
  std::vector<xb::particle> parts;
  parts.emplace_back(xb::element_decl{xb::qname{test_ns, "required"},
                                      xb::qname{xs_ns, "string"}},
                     xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "opt"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{0, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Rec"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "rec"}, xb::qname{test_ns, "Rec"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "rec"});

  REQUIRE(vectors.size() >= 2);

  // Should have at least one vector with opt absent
  bool has_absent =
      std::any_of(vectors.begin(), vectors.end(), [](const xb::test_vector& v) {
        return find_field(v.fields, "opt") == nullptr;
      });
  // And at least one with opt present
  bool has_present =
      std::any_of(vectors.begin(), vectors.end(), [](const xb::test_vector& v) {
        return find_field(v.fields, "opt") != nullptr;
      });

  CHECK(has_absent);
  CHECK(has_present);
}

// ---------------------------------------------------------------------------
// Repeating element: boundary counts
// ---------------------------------------------------------------------------

TEST_CASE("Repeating element generates boundary count vectors",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // complexType List { item: xs:string (minOccurs=0, maxOccurs=5) }
  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "item"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{0, 5});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "List"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "list"}, xb::qname{test_ns, "List"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "list"});

  // Should have vectors with different counts
  // At minimum: 0, 1, and 5 (maxOccurs)
  bool has_zero = false;
  bool has_one = false;
  bool has_max = false;

  for (const auto& vec : vectors) {
    const auto* item_field = find_field(vec.fields, "item");
    if (!item_field) {
      has_zero = true;
    } else if (auto* repeated =
                   std::get_if<std::vector<std::vector<xb::test_field_value>>>(
                       &item_field->content)) {
      if (repeated->size() == 1) has_one = true;
      if (repeated->size() == 5) has_max = true;
    }
  }

  CHECK(has_zero);
  CHECK(has_one);
  CHECK(has_max);
}

// ---------------------------------------------------------------------------
// Optional attribute: present and absent
// ---------------------------------------------------------------------------

TEST_CASE("Optional attribute generates present and absent vectors",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // complexType Elem { @req: xs:string (required), @opt: xs:int (optional) }
  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "value"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});

  std::vector<xb::attribute_use> attrs;
  attrs.push_back({xb::qname{test_ns, "req"}, xb::qname{xs_ns, "string"}, true,
                   std::nullopt, std::nullopt});
  attrs.push_back({xb::qname{test_ns, "opt"}, xb::qname{xs_ns, "int"}, false,
                   std::nullopt, std::nullopt});

  xb::model_group seq(xb::compositor_kind::sequence, std::move(parts));
  xb::complex_content cc(xb::qname{xs_ns, "anyType"},
                         xb::derivation_method::restriction, std::move(seq));
  xb::complex_type ct(
      xb::qname{test_ns, "Elem"}, false, false,
      xb::content_type(xb::content_kind::element_only, std::move(cc)),
      std::move(attrs));

  s.add_complex_type(std::move(ct));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "elem"}, xb::qname{test_ns, "Elem"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "elem"});

  REQUIRE(vectors.size() >= 2);

  // Should have at least one vector with opt attribute absent
  bool has_attr_absent =
      std::any_of(vectors.begin(), vectors.end(), [](const xb::test_vector& v) {
        return find_field(v.fields, "opt") == nullptr;
      });
  bool has_attr_present =
      std::any_of(vectors.begin(), vectors.end(), [](const xb::test_vector& v) {
        return find_field(v.fields, "opt") != nullptr;
      });

  CHECK(has_attr_absent);
  CHECK(has_attr_present);
}

// ---------------------------------------------------------------------------
// Vectors have labels
// ---------------------------------------------------------------------------

TEST_CASE("Generated vectors have descriptive labels",
          "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> alts;
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{1, 1});
  alts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "int"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_choice_type(xb::qname{test_ns, "C"}, std::move(alts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "c"}, xb::qname{test_ns, "C"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "c"});

  for (const auto& vec : vectors) {
    CHECK(!vec.label.empty());
  }

  // Should have choice-related labels
  CHECK(count_with_label(vectors, "choice") >= 2);
}

// ---------------------------------------------------------------------------
// Coverage levels
// ---------------------------------------------------------------------------

TEST_CASE("exhaustive: cross-product of all field values",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "Point"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "p"}, xb::qname{test_ns, "Point"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto boundary = vec_gen.generate(xb::qname{test_ns, "p"});

  xb::test_vector_options opts;
  opts.coverage = xb::coverage_level::exhaustive;
  auto exhaustive = vec_gen.generate(xb::qname{test_ns, "p"}, opts);

  CHECK(exhaustive.size() > boundary.size());

  auto x_vals = val_gen.generate(xb::qname{xs_ns, "byte"});
  auto y_vals = val_gen.generate(xb::qname{xs_ns, "byte"});
  CHECK(exhaustive.size() == x_vals.values.size() * y_vals.values.size());
}

TEST_CASE("pairwise: every value pair appears in at least one vector",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Three byte fields — each has ~6 boundary values, so exhaustive would
  // be ~216 vectors.  Pairwise should be much fewer but cover all pairs.
  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "a"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "b"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "c"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  // Get the value sets so we know what pairs to expect.
  auto a_vals = val_gen.generate(xb::qname{xs_ns, "byte"});
  auto b_vals = val_gen.generate(xb::qname{xs_ns, "byte"});
  auto c_vals = val_gen.generate(xb::qname{xs_ns, "byte"});

  xb::test_vector_options pw_opts;
  pw_opts.coverage = xb::coverage_level::pairwise;
  auto vectors = vec_gen.generate(xb::qname{test_ns, "t"}, pw_opts);

  // Fewer than exhaustive
  xb::test_vector_options ex_opts;
  ex_opts.coverage = xb::coverage_level::exhaustive;
  auto exhaustive = vec_gen.generate(xb::qname{test_ns, "t"}, ex_opts);
  CHECK(vectors.size() < exhaustive.size());

  // Extract field values from each vector into a (val_a, val_b, val_c) tuple.
  // Fields are identified by local name.
  struct row {
    std::string a, b, c;
  };

  std::vector<row> rows;
  for (const auto& vec : vectors) {
    row r;
    for (const auto& f : vec.fields) {
      auto name = f.field_name.local_name();
      if (const auto* val = std::get_if<std::string>(&f.content)) {
        if (name == "a")
          r.a = *val;
        else if (name == "b")
          r.b = *val;
        else if (name == "c")
          r.c = *val;
      }
    }
    rows.push_back(r);
  }

  // Verify: every (a_val, b_val) pair appears in at least one row.
  for (const auto& av : a_vals.values) {
    for (const auto& bv : b_vals.values) {
      bool found = false;
      for (const auto& r : rows) {
        if (r.a == av.xml_text && r.b == bv.xml_text) {
          found = true;
          break;
        }
      }
      INFO("Missing pair: a=" << av.xml_text << ", b=" << bv.xml_text);
      CHECK(found);
    }
  }

  // Verify: every (a_val, c_val) pair appears.
  for (const auto& av : a_vals.values) {
    for (const auto& cv : c_vals.values) {
      bool found = false;
      for (const auto& r : rows) {
        if (r.a == av.xml_text && r.c == cv.xml_text) {
          found = true;
          break;
        }
      }
      INFO("Missing pair: a=" << av.xml_text << ", c=" << cv.xml_text);
      CHECK(found);
    }
  }

  // Verify: every (b_val, c_val) pair appears.
  for (const auto& bv : b_vals.values) {
    for (const auto& cv : c_vals.values) {
      bool found = false;
      for (const auto& r : rows) {
        if (r.b == bv.xml_text && r.c == cv.xml_text) {
          found = true;
          break;
        }
      }
      INFO("Missing pair: b=" << bv.xml_text << ", c=" << cv.xml_text);
      CHECK(found);
    }
  }
}

TEST_CASE("max_vectors caps output count",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  xb::test_vector_options opts;
  opts.max_vectors = 3;
  auto vectors = vec_gen.generate(xb::qname{test_ns, "t"}, opts);
  CHECK(vectors.size() <= 3);
}

TEST_CASE("boundary default equals explicit boundary option",
          "[test_vector_generator][coverage]") {
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
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto default_result = vec_gen.generate(xb::qname{test_ns, "t"});

  xb::test_vector_options opts;
  opts.coverage = xb::coverage_level::boundary;
  auto explicit_result = vec_gen.generate(xb::qname{test_ns, "t"}, opts);

  CHECK(default_result.size() == explicit_result.size());
}

TEST_CASE("max_vectors works with exhaustive",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "x"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "y"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  xb::test_vector_options opts;
  opts.coverage = xb::coverage_level::exhaustive;
  opts.max_vectors = 5;
  auto vectors = vec_gen.generate(xb::qname{test_ns, "t"}, opts);
  CHECK(vectors.size() == 5);
}

TEST_CASE("max_vectors works with pairwise",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "a"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "b"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "c"}, xb::qname{xs_ns, "byte"}},
      xb::occurrence{1, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  xb::test_vector_options opts;
  opts.coverage = xb::coverage_level::pairwise;
  opts.max_vectors = 5;
  auto vectors = vec_gen.generate(xb::qname{test_ns, "t"}, opts);
  CHECK(vectors.size() <= 5);
}

TEST_CASE("nonexistent element returns empty", "[test_vector_generator]") {
  xb::schema s;
  s.set_target_namespace(test_ns);
  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  auto vectors = vec_gen.generate(xb::qname{test_ns, "nonexistent"});
  CHECK(vectors.empty());
}

TEST_CASE("exhaustive falls back to boundary for no scalar fields",
          "[test_vector_generator][coverage]") {
  xb::schema s;
  s.set_target_namespace(test_ns);

  // Only an optional field — no required scalar fields.
  std::vector<xb::particle> parts;
  parts.emplace_back(
      xb::element_decl{xb::qname{test_ns, "note"}, xb::qname{xs_ns, "string"}},
      xb::occurrence{0, 1});

  s.add_complex_type(
      make_sequence_type(xb::qname{test_ns, "T"}, std::move(parts)));
  s.add_element(
      xb::element_decl{xb::qname{test_ns, "t"}, xb::qname{test_ns, "T"}});

  auto ss = make_schema_set(std::move(s));
  xb::test_value_generator val_gen(ss);
  xb::test_vector_generator vec_gen(ss, val_gen);

  // Exhaustive should fall back to boundary (not return empty).
  xb::test_vector_options opts;
  opts.coverage = xb::coverage_level::exhaustive;
  auto exhaustive = vec_gen.generate(xb::qname{test_ns, "t"}, opts);
  auto boundary = vec_gen.generate(xb::qname{test_ns, "t"});

  CHECK(!exhaustive.empty());
  CHECK(exhaustive.size() == boundary.size());
}
