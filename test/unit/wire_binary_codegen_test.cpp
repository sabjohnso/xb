#include <xb/wire/binary_codegen.hpp>
#include <xb/wire/layout_engine.hpp>

#include <xb/wire/encoding.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace xb::bes;
using namespace xb::wire;
using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

  field_type
  make_field(const std::string& name, unsigned bits,
             std::optional<primitive_encoding_type> enc = std::nullopt,
             std::optional<byte_order_type> bo = std::nullopt) {
    field_type f;
    f.name = name;
    f.bits = bits;
    f.encoding = enc;
    f.byte_order = bo;
    return f;
  }

  wire_field_type
  make_wire_field(const std::string& name, unsigned bits) {
    wire_field_type f;
    f.name = name;
    f.bits = bits;
    return f;
  }

  defaults_type
  make_defaults(byte_order_type bo) {
    defaults_type d;
    d.byte_order = bo;
    return d;
  }

} // namespace

// ---------------------------------------------------------------------------
// Basic view class generation
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: simple integer fields", "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));
  msg.choice.push_back(make_field("side", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("order_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class order_view"));
  CHECK_THAT(code, ContainsSubstring("std::span<const std::byte>"));
  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, ContainsSubstring("quantity()"));
  CHECK_THAT(code, ContainsSubstring("side()"));
  CHECK_THAT(code, ContainsSubstring("wire_size"));
}

TEST_CASE("binary_codegen: little-endian fields", "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::little_endian);

  message_type msg;
  msg.name = "Price";
  msg.choice.push_back(make_field("value", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("price_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::endian::little"));
}

TEST_CASE("binary_codegen: per-field byte order override",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Mixed";
  msg.choice.push_back(
      make_field("big_field", 32, std::nullopt, byte_order_type::big_endian));
  msg.choice.push_back(make_field("little_field", 32, std::nullopt,
                                  byte_order_type::little_endian));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("mixed_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::endian::big"));
  CHECK_THAT(code, ContainsSubstring("std::endian::little"));
}

TEST_CASE("binary_codegen: signed integer (twos_complement)",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Signed";
  msg.choice.push_back(
      make_field("delta", 32, primitive_encoding_type::twos_complement));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("signed_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::int32_t"));
}

TEST_CASE("binary_codegen: wire-only fields are accessible",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Framed";
  msg.choice.push_back(make_wire_field("msg_length", 16));
  msg.choice.push_back(make_field("payload", 64));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("framed_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("msg_length()"));
  CHECK_THAT(code, ContainsSubstring("payload()"));
}

TEST_CASE("binary_codegen: constant field returns value",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Versioned";

  constant_type c;
  c.name = "protocol_version";
  c.value = "2";
  msg.choice.push_back(std::move(c));

  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("versioned_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("protocol_version()"));
  CHECK_THAT(code, ContainsSubstring("2"));
}

TEST_CASE("binary_codegen: sub-byte fields use extract_bits",
          "[wire][binary_codegen]") {
  defaults_type defaults;
  defaults.byte_order = byte_order_type::big_endian;
  defaults.alignment = 0; // no alignment for bit packing

  message_type msg;
  msg.name = "BitPacked";
  msg.choice.push_back(make_field("flags", 3));
  msg.choice.push_back(make_field("version", 5));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("bitpacked_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("extract_bits"));
  CHECK_THAT(code, ContainsSubstring("flags()"));
  CHECK_THAT(code, ContainsSubstring("version()"));
}

TEST_CASE("binary_codegen: wire_size reflects total layout",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Small";
  msg.choice.push_back(make_field("a", 8));
  msg.choice.push_back(make_field("b", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("small_view", msg, layout, defaults);

  // 16 bits = 2 bytes
  CHECK_THAT(code, ContainsSubstring("wire_size = 2"));
}

TEST_CASE("binary_codegen: fixed-width string field",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithString";

  field_type f;
  f.name = "symbol";
  f.bits = 64; // 8 bytes
  f.encoding = primitive_encoding_type::ascii;
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("string_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("symbol()"));
  CHECK_THAT(code, ContainsSubstring("string_view"));
}

TEST_CASE("binary_codegen: raw binary field returns span",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithPayload";

  field_type f;
  f.name = "payload";
  f.bits = 128; // 16 bytes
  f.encoding = primitive_encoding_type::raw;
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("payload_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("payload()"));
  CHECK_THAT(code, ContainsSubstring("std::span<const std::byte>"));
  // Should use subspan at correct offset and size
  CHECK_THAT(code, ContainsSubstring(".subspan("));
}

TEST_CASE("binary_codegen: enum-integer field returns integer",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithEnum";

  field_type f;
  f.name = "side";
  f.bits = 8;
  f.encoding = primitive_encoding_type::enum_integer;

  // Add enum map with XSD→wire mappings
  enum_map_type em;
  enum_entry_type e1;
  e1.xsd_value = "Buy";
  e1.wire_value = "1";
  enum_entry_type e2;
  e2.xsd_value = "Sell";
  e2.wire_value = "2";
  em.entry = {e1, e2};
  f.enum_map = em;

  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("enum_view", msg, layout, defaults);

  // At the view layer, enum fields decode as integers
  CHECK_THAT(code, ContainsSubstring("side()"));
  CHECK_THAT(code, ContainsSubstring("std::uint8_t"));
}

TEST_CASE("binary_codegen: enum-char field returns integer",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithCharEnum";

  field_type f;
  f.name = "msg_type";
  f.bits = 8;
  f.encoding = primitive_encoding_type::enum_char;

  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("char_enum_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("msg_type()"));
  CHECK_THAT(code, ContainsSubstring("std::uint8_t"));
}

TEST_CASE("binary_codegen: null_value field returns optional",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithOptional";

  field_type f;
  f.name = "price";
  f.bits = 32;
  f.null_value = "0xFFFFFFFF";
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("optional_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, ContainsSubstring("std::optional"));
  CHECK_THAT(code, ContainsSubstring("0xFFFFFFFF"));
}

TEST_CASE("binary_codegen: null_value sub-byte field returns optional",
          "[wire][binary_codegen]") {
  defaults_type defaults;
  defaults.byte_order = byte_order_type::big_endian;
  defaults.alignment = 0;

  message_type msg;
  msg.name = "BitOptional";

  field_type f;
  f.name = "level";
  f.bits = 4;
  f.null_value = "0xF";
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("bit_opt_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("level()"));
  CHECK_THAT(code, ContainsSubstring("std::optional"));
  CHECK_THAT(code, ContainsSubstring("extract_bits"));
  CHECK_THAT(code, ContainsSubstring("0xF"));
}

// ---------------------------------------------------------------------------
// Validation level NTTP
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: view class has validation_level template parameter",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Simple";
  msg.choice.push_back(make_field("value", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("simple_view", msg, layout, defaults);

  // Class should be templated on validation_level
  CHECK_THAT(code, ContainsSubstring("validation_level"));
  CHECK_THAT(code, ContainsSubstring("template"));
}

TEST_CASE("binary_codegen: structural validation checks buffer size",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Checked";
  msg.choice.push_back(make_field("data", 64));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("checked_view", msg, layout, defaults);

  // At structural level or above, constructor should validate buffer size
  CHECK_THAT(code, ContainsSubstring("if constexpr"));
  CHECK_THAT(code, ContainsSubstring("wire_size"));
}

TEST_CASE("binary_codegen: empty message", "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Empty";

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("empty_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class empty_view"));
  CHECK_THAT(code, ContainsSubstring("wire_size = 0"));
}

// ---------------------------------------------------------------------------
// String padding trimming (UX-4)
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: string accessor trims trailing spaces by default",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);
  // Default string_padding is space (or absent → space)

  message_type msg;
  msg.name = "WithString";
  msg.choice.push_back(
      make_field("symbol", 64, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("str_view", msg, layout, defaults);

  // Trimmed accessor should scan from the right
  CHECK_THAT(code, ContainsSubstring("symbol()"));
  CHECK_THAT(code, ContainsSubstring("find_last_not_of"));
}

TEST_CASE("binary_codegen: string accessor trims trailing nulls",
          "[wire][binary_codegen]") {
  defaults_type defaults;
  defaults.byte_order = byte_order_type::big_endian;
  defaults.string_padding = padding_type::null;

  message_type msg;
  msg.name = "NullPadded";
  msg.choice.push_back(make_field("name", 80, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("null_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("name()"));
  CHECK_THAT(code, ContainsSubstring("\\0"));
}

TEST_CASE("binary_codegen: raw_ accessor returns full-width unstripped view",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "RawString";
  msg.choice.push_back(
      make_field("ticker", 64, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("raw_str_view", msg, layout, defaults);

  // Should have both trimmed and raw accessors
  CHECK_THAT(code, ContainsSubstring("ticker()"));
  CHECK_THAT(code, ContainsSubstring("raw_ticker()"));
}

// ---------------------------------------------------------------------------
// Wire-field vs data field visibility (UX-5)
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: view class has wire-only annotations",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Annotated";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("price", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("ann_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("[wire-only]"));
  CHECK_THAT(code, ContainsSubstring("[data]"));
}

TEST_CASE("binary_codegen: concept excludes wire-only fields",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "MixedFields";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("price", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("mixed_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, !ContainsSubstring("msg_type"));
}

TEST_CASE("binary_codegen: base class excludes wire-only virtuals",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "BaseFields";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("price", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_base_class("base_cls", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, !ContainsSubstring("msg_type"));
}

TEST_CASE("binary_codegen: wire-only fields still accessible on view",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Accessible";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("price", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("acc_view", msg, layout, defaults);

  // Both fields accessible on view (not hidden, just annotated)
  CHECK_THAT(code, ContainsSubstring("msg_type()"));
  CHECK_THAT(code, ContainsSubstring("price()"));
}

TEST_CASE("binary_codegen: view class groups data fields before wire-only",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Grouped";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("price", 32));
  msg.choice.push_back(make_field("quantity", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("grp_view", msg, layout, defaults);

  // Data fields should come before wire-only separator
  auto data_pos = code.find("[data]");
  auto wire_pos = code.find("wire-only fields");
  REQUIRE(data_pos != std::string::npos);
  REQUIRE(wire_pos != std::string::npos);
  CHECK(data_pos < wire_pos);
}

// ---------------------------------------------------------------------------
// Builder / constructor (UX-6)
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: owned class has aggregate constructor",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_wire_field("msg_type", 8));
  msg.choice.push_back(make_field("stock_locate", 16));
  msg.choice.push_back(make_field("shares", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("order_owned", msg, layout, defaults);

  // Constructor takes data fields only (not wire-only)
  CHECK_THAT(code, ContainsSubstring("order_owned("));
  CHECK_THAT(code, ContainsSubstring("stock_locate"));
  CHECK_THAT(code, ContainsSubstring("shares"));
  // Should not take msg_type as parameter (it's wire-only)
  // The default constructor should still exist
  CHECK_THAT(code, ContainsSubstring("order_owned()"));
}

TEST_CASE("binary_codegen: owned class has args struct",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Trade";
  msg.choice.push_back(make_field("price", 32));
  msg.choice.push_back(
      make_field("symbol", 64, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("trade_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("struct args"));
  CHECK_THAT(code, ContainsSubstring("trade_owned(args"));
}

TEST_CASE("binary_codegen: constructor auto-sets wire-only discriminant",
          "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "AddOrder";
  msg.discriminant_value = "0x41";

  wire_field_type wf;
  wf.name = "msg_type";
  wf.bits = 8;
  msg.choice.push_back(std::move(wf));
  msg.choice.push_back(make_field("price", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("add_order_owned", msg, layout, defaults);

  // Constructor body should auto-set msg_type to 0x41
  CHECK_THAT(code, ContainsSubstring("set_msg_type"));
  CHECK_THAT(code, ContainsSubstring("0x41"));
}

// ---------------------------------------------------------------------------
// Discriminator endianness (UX-7)
// ---------------------------------------------------------------------------

TEST_CASE("binary_codegen: little-endian discriminator uses from_wire<little>",
          "[wire][binary_codegen]") {
  discriminant_info disc;
  disc.field_name = "msg_type";
  disc.offset_bits = 0;
  disc.width_bits = 16;
  disc.endian = "std::endian::little";

  std::vector<message_entry> entries;
  entries.push_back({"OrderView", "0x0041"});

  auto code = generate_discriminator("disc_le", disc, entries);
  CHECK_THAT(code, ContainsSubstring("std::endian::little"));
  CHECK_THAT(code, !ContainsSubstring("std::endian::big"));
}

TEST_CASE("binary_codegen: big-endian discriminator uses from_wire<big>",
          "[wire][binary_codegen]") {
  discriminant_info disc;
  disc.field_name = "msg_type";
  disc.offset_bits = 0;
  disc.width_bits = 16;
  disc.endian = "std::endian::big";

  std::vector<message_entry> entries;
  entries.push_back({"OrderView", "0x0041"});

  auto code = generate_discriminator("disc_be", disc, entries);
  CHECK_THAT(code, ContainsSubstring("std::endian::big"));
}

TEST_CASE("binary_codegen: single-byte discriminator skips from_wire",
          "[wire][binary_codegen]") {
  discriminant_info disc;
  disc.field_name = "msg_type";
  disc.offset_bits = 0;
  disc.width_bits = 8;
  disc.endian = "std::endian::big"; // should be ignored for 8-bit

  std::vector<message_entry> entries;
  entries.push_back({"OrderView", "0x41"});

  auto code = generate_discriminator("disc_1b", disc, entries);
  CHECK_THAT(code, !ContainsSubstring("from_wire"));
}
