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

TEST_CASE("binary_codegen: empty message", "[wire][binary_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Empty";

  auto layout = compute_layout(msg, defaults);
  auto code = generate_view_class("empty_view", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class empty_view"));
  CHECK_THAT(code, ContainsSubstring("wire_size = 0"));
}
