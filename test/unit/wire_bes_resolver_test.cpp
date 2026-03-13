#include <xb/wire/bes_resolver.hpp>

#include <encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

using namespace xb::bes;
using xb::wire::merge_defaults;
using xb::wire::resolve_imports;

// ---------------------------------------------------------------------------
// merge_defaults
// ---------------------------------------------------------------------------

TEST_CASE("merge_defaults: overlay with no values returns base",
          "[wire][bes_resolver]") {
  defaults_type base;
  base.byte_order = byte_order_type::big_endian;
  base.bit_order = bit_order_type::msb_first;
  base.alignment = 8;
  base.string_padding = padding_type::space;
  base.string_encoding = charset_type::ascii;

  defaults_type overlay; // all empty

  auto result = merge_defaults(base, overlay);
  CHECK(result.byte_order == byte_order_type::big_endian);
  CHECK(result.bit_order == bit_order_type::msb_first);
  CHECK(*result.alignment == 8);
  CHECK(result.string_padding == padding_type::space);
  CHECK(result.string_encoding == charset_type::ascii);
}

TEST_CASE("merge_defaults: overlay values win over base",
          "[wire][bes_resolver]") {
  defaults_type base;
  base.byte_order = byte_order_type::big_endian;
  base.alignment = 8;

  defaults_type overlay;
  overlay.byte_order = byte_order_type::little_endian;
  overlay.alignment = 32;

  auto result = merge_defaults(base, overlay);
  CHECK(result.byte_order == byte_order_type::little_endian);
  CHECK(*result.alignment == 32);
}

TEST_CASE("merge_defaults: mixed fields from both", "[wire][bes_resolver]") {
  defaults_type base;
  base.byte_order = byte_order_type::big_endian;
  base.string_encoding = charset_type::utf8;

  defaults_type overlay;
  overlay.bit_order = bit_order_type::lsb_first;
  overlay.string_padding = padding_type::null;

  auto result = merge_defaults(base, overlay);
  CHECK(result.byte_order == byte_order_type::big_endian); // from base
  CHECK(result.bit_order == bit_order_type::lsb_first);    // from overlay
  CHECK(result.string_encoding == charset_type::utf8);     // from base
  CHECK(result.string_padding == padding_type::null);      // from overlay
}

TEST_CASE("merge_defaults: both empty returns empty", "[wire][bes_resolver]") {
  defaults_type base;
  defaults_type overlay;
  auto result = merge_defaults(base, overlay);
  CHECK(!result.byte_order.has_value());
  CHECK(!result.bit_order.has_value());
  CHECK(!result.string_padding.has_value());
  CHECK(!result.string_encoding.has_value());
}

// ---------------------------------------------------------------------------
// resolve_imports: no imports (identity)
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: no imports returns root unchanged",
          "[wire][bes_resolver]") {
  encoding_type root;
  root.target_namespace = "http://example.com/test";

  framing_type fr;
  fr.name = "test_frame";
  root.choice.push_back(std::move(fr));

  auto loader = [](const std::string&) -> encoding_type {
    throw std::runtime_error("should not be called");
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.target_namespace == "http://example.com/test");
  CHECK(result.choice.size() == 1);
  CHECK(std::holds_alternative<framing_type>(result.choice[0]));
  CHECK(std::get<framing_type>(result.choice[0]).name == "test_frame");
}

// ---------------------------------------------------------------------------
// resolve_imports: single import, non-overlapping
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: single import merges non-overlapping components",
          "[wire][bes_resolver]") {
  // Imported document has a framing
  encoding_type imported;
  framing_type imported_fr;
  imported_fr.name = "base_frame";
  imported.choice.push_back(std::move(imported_fr));

  // Root has a message
  encoding_type root;
  message_type msg;
  msg.name = "TestMsg";
  root.choice.push_back(std::move(msg));
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string& href) -> encoding_type {
    CHECK(href == "base.xml");
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.choice.size() == 2);

  // Both components present
  bool has_framing = false;
  bool has_message = false;
  for (const auto& c : result.choice) {
    if (auto* fr = std::get_if<framing_type>(&c))
      has_framing = (fr->name == "base_frame");
    if (auto* m = std::get_if<message_type>(&c))
      has_message = (m->name == "TestMsg");
  }
  CHECK(has_framing);
  CHECK(has_message);
}

// ---------------------------------------------------------------------------
// resolve_imports: local overrides imported (same name)
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: local framing overrides imported same-name",
          "[wire][bes_resolver]") {
  encoding_type imported;
  framing_type imported_fr;
  imported_fr.name = "shared_frame";
  imported_fr.byte_order = byte_order_type::big_endian;
  imported.choice.push_back(std::move(imported_fr));

  encoding_type root;
  framing_type local_fr;
  local_fr.name = "shared_frame";
  local_fr.byte_order = byte_order_type::little_endian;
  root.choice.push_back(std::move(local_fr));
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string&) -> encoding_type {
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.choice.size() == 1);

  auto& fr = std::get<framing_type>(result.choice[0]);
  CHECK(fr.name == "shared_frame");
  CHECK(fr.byte_order == byte_order_type::little_endian); // local wins
}

TEST_CASE("resolve_imports: local message overrides imported same-name",
          "[wire][bes_resolver]") {
  encoding_type imported;
  message_type imported_msg;
  imported_msg.name = "OrderMsg";
  imported_msg.discriminant_value = "1";
  imported.choice.push_back(std::move(imported_msg));

  encoding_type root;
  message_type local_msg;
  local_msg.name = "OrderMsg";
  local_msg.discriminant_value = "99";
  root.choice.push_back(std::move(local_msg));
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string&) -> encoding_type {
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.choice.size() == 1);

  auto& m = std::get<message_type>(result.choice[0]);
  CHECK(m.name == "OrderMsg");
  CHECK(m.discriminant_value == "99"); // local wins
}

// ---------------------------------------------------------------------------
// resolve_imports: defaults merging through import
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: local defaults override imported defaults",
          "[wire][bes_resolver]") {
  encoding_type imported;
  defaults_type imp_defs;
  imp_defs.byte_order = byte_order_type::big_endian;
  imp_defs.alignment = 8;
  imp_defs.string_encoding = charset_type::ascii;
  imported.defaults = std::move(imp_defs);

  encoding_type root;
  defaults_type local_defs;
  local_defs.byte_order = byte_order_type::little_endian;
  root.defaults = std::move(local_defs);
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string&) -> encoding_type {
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  REQUIRE(result.defaults.has_value());
  CHECK(result.defaults->byte_order == byte_order_type::little_endian); // local
  CHECK(*result.defaults->alignment == 8); // from imported
  CHECK(result.defaults->string_encoding ==
        charset_type::ascii); // from imported
}

// ---------------------------------------------------------------------------
// resolve_imports: multiple imports (later overrides earlier)
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: later import overrides earlier import",
          "[wire][bes_resolver]") {
  encoding_type first_import;
  framing_type fr1;
  fr1.name = "shared_frame";
  fr1.byte_order = byte_order_type::big_endian;
  first_import.choice.push_back(std::move(fr1));

  encoding_type second_import;
  framing_type fr2;
  fr2.name = "shared_frame";
  fr2.byte_order = byte_order_type::little_endian;
  second_import.choice.push_back(std::move(fr2));

  encoding_type root;
  root.import.push_back(import_type{"first.xml"});
  root.import.push_back(import_type{"second.xml"});

  auto loader = [&](const std::string& href) -> encoding_type {
    if (href == "first.xml") return std::move(first_import);
    if (href == "second.xml") return std::move(second_import);
    throw std::runtime_error("unexpected href: " + href);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.choice.size() == 1);

  auto& fr = std::get<framing_type>(result.choice[0]);
  CHECK(fr.byte_order == byte_order_type::little_endian); // second wins
}

// ---------------------------------------------------------------------------
// resolve_imports: transitive imports
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: transitive import chain A->B->C",
          "[wire][bes_resolver]") {
  encoding_type doc_c;
  framing_type fr_c;
  fr_c.name = "c_frame";
  doc_c.choice.push_back(std::move(fr_c));

  encoding_type doc_b;
  framing_type fr_b;
  fr_b.name = "b_frame";
  doc_b.choice.push_back(std::move(fr_b));
  doc_b.import.push_back(import_type{"c.xml"});

  encoding_type root;
  message_type msg_a;
  msg_a.name = "a_msg";
  root.choice.push_back(std::move(msg_a));
  root.import.push_back(import_type{"b.xml"});

  auto loader = [&](const std::string& href) -> encoding_type {
    if (href == "b.xml") return std::move(doc_b);
    if (href == "c.xml") return std::move(doc_c);
    throw std::runtime_error("unexpected href: " + href);
  };

  auto result = resolve_imports(std::move(root), loader);

  // Should have: c_frame (from C), b_frame (from B), a_msg (from A)
  bool has_c_frame = false, has_b_frame = false, has_a_msg = false;
  for (const auto& c : result.choice) {
    if (auto* fr = std::get_if<framing_type>(&c)) {
      if (fr->name == "c_frame") has_c_frame = true;
      if (fr->name == "b_frame") has_b_frame = true;
    }
    if (auto* m = std::get_if<message_type>(&c))
      if (m->name == "a_msg") has_a_msg = true;
  }
  CHECK(has_c_frame);
  CHECK(has_b_frame);
  CHECK(has_a_msg);
}

TEST_CASE("resolve_imports: transitive override chain C->B->A",
          "[wire][bes_resolver]") {
  // C defines framing X, B imports C and overrides X, A imports B
  encoding_type doc_c;
  framing_type fr_c;
  fr_c.name = "shared";
  fr_c.byte_order = byte_order_type::big_endian;
  doc_c.choice.push_back(std::move(fr_c));

  encoding_type doc_b;
  framing_type fr_b;
  fr_b.name = "shared";
  fr_b.byte_order = byte_order_type::little_endian;
  doc_b.choice.push_back(std::move(fr_b));
  doc_b.import.push_back(import_type{"c.xml"});

  encoding_type root;
  root.import.push_back(import_type{"b.xml"});

  auto loader = [&](const std::string& href) -> encoding_type {
    if (href == "b.xml") return std::move(doc_b);
    if (href == "c.xml") return std::move(doc_c);
    throw std::runtime_error("unexpected href: " + href);
  };

  auto result = resolve_imports(std::move(root), loader);
  REQUIRE(result.choice.size() == 1);

  auto& fr = std::get<framing_type>(result.choice[0]);
  CHECK(fr.name == "shared");
  CHECK(fr.byte_order == byte_order_type::little_endian); // B's version
}

// ---------------------------------------------------------------------------
// resolve_imports: circular import detection
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: circular import A->B->A throws",
          "[wire][bes_resolver]") {
  // Loader constructs fresh documents each time to avoid move issues
  auto loader = [](const std::string& href) -> encoding_type {
    encoding_type doc;
    if (href == "b.xml")
      doc.import.push_back(import_type{"a.xml"});
    else if (href == "a.xml")
      doc.import.push_back(import_type{"b.xml"});
    else
      throw std::runtime_error("unexpected href: " + href);
    return doc;
  };

  encoding_type root;
  root.import.push_back(import_type{"b.xml"});

  CHECK_THROWS_AS(resolve_imports(std::move(root), loader), std::runtime_error);
}

TEST_CASE("resolve_imports: circular import A->B->C->A throws",
          "[wire][bes_resolver]") {
  auto loader = [](const std::string& href) -> encoding_type {
    encoding_type doc;
    if (href == "b.xml")
      doc.import.push_back(import_type{"c.xml"});
    else if (href == "c.xml")
      doc.import.push_back(import_type{"b.xml"}); // cycle back to b
    else
      throw std::runtime_error("unexpected href: " + href);
    return doc;
  };

  encoding_type root;
  root.import.push_back(import_type{"b.xml"});

  CHECK_THROWS_AS(resolve_imports(std::move(root), loader), std::runtime_error);
}

// ---------------------------------------------------------------------------
// resolve_imports: frame-stack override
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: local frame-stack overrides imported same-name",
          "[wire][bes_resolver]") {
  encoding_type imported;
  frame_stack_type imported_fs;
  imported_fs.name = "my_stack";
  layer_type l1;
  l1.ref = "old_layer";
  imported_fs.layer.push_back(std::move(l1));
  imported.choice.push_back(std::move(imported_fs));

  encoding_type root;
  frame_stack_type local_fs;
  local_fs.name = "my_stack";
  layer_type l2;
  l2.ref = "new_layer";
  local_fs.layer.push_back(std::move(l2));
  root.choice.push_back(std::move(local_fs));
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string&) -> encoding_type {
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.choice.size() == 1);
  auto& fs = std::get<frame_stack_type>(result.choice[0]);
  CHECK(fs.name == "my_stack");
  REQUIRE(fs.layer.size() == 1);
  CHECK(fs.layer[0].ref == "new_layer"); // local wins
}

// ---------------------------------------------------------------------------
// resolve_imports: imports are cleared after resolution
// ---------------------------------------------------------------------------

TEST_CASE("resolve_imports: imports list is empty in result",
          "[wire][bes_resolver]") {
  encoding_type imported;
  framing_type fr;
  fr.name = "base_frame";
  imported.choice.push_back(std::move(fr));

  encoding_type root;
  root.import.push_back(import_type{"base.xml"});

  auto loader = [&](const std::string&) -> encoding_type {
    return std::move(imported);
  };

  auto result = resolve_imports(std::move(root), loader);
  CHECK(result.import.empty());
}
