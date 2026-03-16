#include <xb/wire/binary_codegen.hpp>
#include <xb/wire/layout_engine.hpp>

#include <encoding.hpp>

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
// Owned class generation
// ---------------------------------------------------------------------------

TEST_CASE("owned_codegen: simple integer fields", "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));
  msg.choice.push_back(make_field("side", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("order_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class order_owned"));
  CHECK_THAT(code, ContainsSubstring("std::vector<std::byte>"));
  // Read accessors
  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, ContainsSubstring("quantity()"));
  CHECK_THAT(code, ContainsSubstring("side()"));
  // Write mutators
  CHECK_THAT(code, ContainsSubstring("set_price("));
  CHECK_THAT(code, ContainsSubstring("set_quantity("));
  CHECK_THAT(code, ContainsSubstring("set_side("));
  // Buffer access
  CHECK_THAT(code, ContainsSubstring("buffer()"));
  CHECK_THAT(code, ContainsSubstring("wire_size"));
}

TEST_CASE("owned_codegen: write mutator uses to_wire",
          "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Price";
  msg.choice.push_back(make_field("value", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("price_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("to_wire"));
  CHECK_THAT(code, ContainsSubstring("std::endian::big"));
}

TEST_CASE("owned_codegen: single-byte mutator", "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Flag";
  msg.choice.push_back(make_field("value", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("flag_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("set_value("));
  CHECK_THAT(code, ContainsSubstring("static_cast<std::byte>"));
}

TEST_CASE("owned_codegen: sub-byte mutator uses insert_bits",
          "[wire][owned_codegen]") {
  defaults_type defaults;
  defaults.byte_order = byte_order_type::big_endian;
  defaults.alignment = 0;

  message_type msg;
  msg.name = "BitPacked";
  msg.choice.push_back(make_field("flags", 3));
  msg.choice.push_back(make_field("version", 5));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("bitpacked_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("set_flags("));
  CHECK_THAT(code, ContainsSubstring("set_version("));
  CHECK_THAT(code, ContainsSubstring("insert_bits"));
}

TEST_CASE("owned_codegen: string mutator", "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithString";

  field_type f;
  f.name = "symbol";
  f.bits = 64;
  f.encoding = primitive_encoding_type::ascii;
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("string_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("set_symbol("));
  CHECK_THAT(code, ContainsSubstring("string_view"));
}

TEST_CASE("owned_codegen: raw binary mutator", "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithRaw";

  field_type f;
  f.name = "payload";
  f.bits = 128;
  f.encoding = primitive_encoding_type::raw;
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("raw_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("set_payload("));
  CHECK_THAT(code, ContainsSubstring("std::span<const std::byte>"));
}

TEST_CASE("owned_codegen: wire-only fields have mutators",
          "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Framed";
  msg.choice.push_back(make_wire_field("msg_length", 16));
  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("framed_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("set_msg_length("));
  CHECK_THAT(code, ContainsSubstring("set_data("));
}

TEST_CASE("owned_codegen: constants have no mutator", "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Versioned";

  constant_type c;
  c.name = "protocol_version";
  c.value = "2";
  msg.choice.push_back(std::move(c));

  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("versioned_owned", msg, layout, defaults);

  // Constant has read accessor but no set_ mutator
  CHECK_THAT(code, ContainsSubstring("protocol_version()"));
  CHECK_THAT(code, !ContainsSubstring("set_protocol_version"));
}

// ---------------------------------------------------------------------------
// Mutable view class generation
// ---------------------------------------------------------------------------

TEST_CASE("mutable_view_codegen: has read and write accessors",
          "[wire][mutable_view_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));

  auto layout = compute_layout(msg, defaults);
  auto code =
      generate_mutable_view_class("order_mut_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class order_mut_view"));
  CHECK_THAT(code, ContainsSubstring("std::span<std::byte>"));
  // Read
  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, ContainsSubstring("quantity()"));
  // Write
  CHECK_THAT(code, ContainsSubstring("set_price("));
  CHECK_THAT(code, ContainsSubstring("set_quantity("));
  // No vector — non-owning
  CHECK_THAT(code, !ContainsSubstring("std::vector"));
}

// ---------------------------------------------------------------------------
// Owned class default constructor allocates buffer
// ---------------------------------------------------------------------------

TEST_CASE("owned_codegen: constructor allocates wire_size bytes",
          "[wire][owned_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Small";
  msg.choice.push_back(make_field("a", 16));
  msg.choice.push_back(make_field("b", 16));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_owned_class("small_owned", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("wire_size = 4"));
  CHECK_THAT(code, ContainsSubstring("buf_(wire_size"));
}
