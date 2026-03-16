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
             std::optional<primitive_encoding_type> enc = std::nullopt) {
    field_type f;
    f.name = name;
    f.bits = bits;
    f.encoding = enc;
    return f;
  }

  defaults_type
  make_defaults(byte_order_type bo) {
    defaults_type d;
    d.byte_order = bo;
    return d;
  }

} // namespace

// ===========================================================================
// Concept generation
// ===========================================================================

TEST_CASE("concept_codegen: simple integer fields", "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));
  msg.choice.push_back(make_field("side", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("order_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("template <typename T>"));
  CHECK_THAT(code, ContainsSubstring("concept order_message"));
  CHECK_THAT(code, ContainsSubstring("t.price()"));
  CHECK_THAT(code, ContainsSubstring("t.quantity()"));
  CHECK_THAT(code, ContainsSubstring("t.side()"));
  CHECK_THAT(code, ContainsSubstring("std::convertible_to"));
}

TEST_CASE("concept_codegen: signed integer uses int type",
          "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Delta";
  msg.choice.push_back(
      make_field("value", 32, primitive_encoding_type::twos_complement));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("delta_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::int32_t"));
}

TEST_CASE("concept_codegen: string field uses string_view",
          "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Symbol";
  msg.choice.push_back(make_field("name", 64, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("symbol_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::string_view"));
}

TEST_CASE("concept_codegen: raw field uses span", "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Payload";
  msg.choice.push_back(make_field("data", 128, primitive_encoding_type::raw));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("payload_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::span<const std::byte>"));
}

TEST_CASE("concept_codegen: optional field uses std::optional",
          "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "WithOpt";

  field_type f;
  f.name = "price";
  f.bits = 32;
  f.null_value = "0xFFFFFFFF";
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("opt_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::optional"));
}

TEST_CASE("concept_codegen: constant field included",
          "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Versioned";

  constant_type c;
  c.name = "version";
  c.value = "2";
  msg.choice.push_back(std::move(c));

  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("versioned_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("t.version()"));
  CHECK_THAT(code, ContainsSubstring("t.data()"));
}

TEST_CASE("concept_codegen: padding excluded", "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Padded";
  msg.choice.push_back(make_field("a", 16));

  padding_field_type pad;
  pad.bits = 16;
  msg.choice.push_back(std::move(pad));

  msg.choice.push_back(make_field("b", 16));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("padded_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("t.a()"));
  CHECK_THAT(code, ContainsSubstring("t.b()"));
  // Padding should not appear as a requirement
  CHECK_THAT(code, !ContainsSubstring("padding"));
}

TEST_CASE("concept_codegen: empty message", "[wire][concept_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Empty";

  auto layout = compute_layout(msg, defaults);
  auto code = generate_concept("empty_message", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("concept empty_message"));
}

// ===========================================================================
// Base class generation
// ===========================================================================

TEST_CASE("base_codegen: simple integer fields", "[wire][base_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));
  msg.choice.push_back(make_field("side", 8));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_base_class("order_base", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("class order_base"));
  CHECK_THAT(code, ContainsSubstring("virtual ~order_base()"));
  CHECK_THAT(code, ContainsSubstring("virtual"));
  CHECK_THAT(code, ContainsSubstring("price()"));
  CHECK_THAT(code, ContainsSubstring("quantity()"));
  CHECK_THAT(code, ContainsSubstring("side()"));
  CHECK_THAT(code, ContainsSubstring("= 0"));
}

TEST_CASE("base_codegen: signed and string fields", "[wire][base_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Mixed";
  msg.choice.push_back(
      make_field("delta", 32, primitive_encoding_type::twos_complement));
  msg.choice.push_back(
      make_field("symbol", 64, primitive_encoding_type::ascii));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_base_class("mixed_base", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("std::int32_t"));
  CHECK_THAT(code, ContainsSubstring("std::string_view"));
}

TEST_CASE("base_codegen: constant is non-virtual static",
          "[wire][base_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Versioned";

  constant_type c;
  c.name = "version";
  c.value = "2";
  msg.choice.push_back(std::move(c));

  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_base_class("versioned_base", msg, layout, defaults);

  // Constants should be static constexpr, not virtual
  CHECK_THAT(code, ContainsSubstring("version()"));
  CHECK_THAT(code, ContainsSubstring("constexpr"));
}

TEST_CASE("base_codegen: padding excluded", "[wire][base_codegen]") {
  auto defaults = make_defaults(byte_order_type::big_endian);

  message_type msg;
  msg.name = "Padded";
  msg.choice.push_back(make_field("a", 16));

  padding_field_type pad;
  pad.bits = 16;
  msg.choice.push_back(std::move(pad));

  msg.choice.push_back(make_field("b", 16));

  auto layout = compute_layout(msg, defaults);
  auto code = generate_base_class("padded_base", msg, layout, defaults);

  CHECK_THAT(code, ContainsSubstring("a()"));
  CHECK_THAT(code, ContainsSubstring("b()"));
  CHECK_THAT(code, !ContainsSubstring("padding"));
}

// ===========================================================================
// Discriminator generation
// ===========================================================================

TEST_CASE("discriminator_codegen: generates variant dispatch",
          "[wire][discriminator_codegen]") {
  discriminant_info disc;
  disc.field_name = "msg_type";
  disc.offset_bits = 0;
  disc.width_bits = 8;

  std::vector<message_entry> entries;
  entries.push_back({"add_order_view", "0x41"});
  entries.push_back({"delete_order_view", "0x44"});

  auto code = generate_discriminator("discriminate", disc, entries);

  CHECK_THAT(code, ContainsSubstring("template"));
  CHECK_THAT(code, ContainsSubstring("validation_level"));
  CHECK_THAT(code, ContainsSubstring("discriminate"));
  CHECK_THAT(code, ContainsSubstring("std::variant"));
  CHECK_THAT(code, ContainsSubstring("add_order_view"));
  CHECK_THAT(code, ContainsSubstring("delete_order_view"));
  CHECK_THAT(code, ContainsSubstring("0x41"));
  CHECK_THAT(code, ContainsSubstring("0x44"));
}

TEST_CASE("discriminator_codegen: throws on unknown type",
          "[wire][discriminator_codegen]") {
  discriminant_info disc;
  disc.field_name = "msg_type";
  disc.offset_bits = 0;
  disc.width_bits = 8;

  std::vector<message_entry> entries;
  entries.push_back({"msg_view", "1"});

  auto code = generate_discriminator("dispatch", disc, entries);

  CHECK_THAT(code, ContainsSubstring("throw"));
}
