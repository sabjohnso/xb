#include <xb/wire/bes_value_generator.hpp>

#include <xb/wire/encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

  using xb::bes::field_type;
  using xb::bes::primitive_encoding_type;
  using xb::wire::bes_test_value;
  using xb::wire::bes_test_value_set;
  using xb::wire::bes_value_generator;

  bool
  has_value(const bes_test_value_set& vs, const std::string& text) {
    for (const auto& v : vs.values) {
      if (v.xml_text == text) return true;
    }
    return false;
  }

  field_type
  make_field(const std::string& name, unsigned bits,
             primitive_encoding_type enc) {
    field_type f;
    f.name = name;
    f.bits = bits;
    f.encoding = enc;
    return f;
  }

} // namespace

// ---------------------------------------------------------------------------
// Unsigned integer boundaries from bit width
// ---------------------------------------------------------------------------

TEST_CASE("8-bit unsigned: 0..255", "[bes_value_generator]") {
  auto f = make_field("val", 8, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "254"));
  CHECK(has_value(vs, "255"));
}

TEST_CASE("12-bit unsigned: 0..4095", "[bes_value_generator]") {
  auto f = make_field("val", 12, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "4094"));
  CHECK(has_value(vs, "4095"));
  CHECK(!has_value(vs, "65535")); // NOT the xs:unsignedShort range
}

TEST_CASE("16-bit unsigned: 0..65535", "[bes_value_generator]") {
  auto f = make_field("val", 16, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "65535"));
}

TEST_CASE("32-bit unsigned: 0..4294967295", "[bes_value_generator]") {
  auto f = make_field("val", 32, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "4294967295"));
}

TEST_CASE("1-bit unsigned: 0..1", "[bes_value_generator]") {
  auto f = make_field("flag", 1, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
}

TEST_CASE("3-bit unsigned: 0..7", "[bes_value_generator]") {
  auto f = make_field("tag", 3, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "7"));
}

// ---------------------------------------------------------------------------
// Signed (twos-complement) boundaries from bit width
// ---------------------------------------------------------------------------

TEST_CASE("8-bit twos-complement: -128..127", "[bes_value_generator]") {
  auto f = make_field("val", 8, primitive_encoding_type::twos_complement);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "-128"));
  CHECK(has_value(vs, "-127"));
  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "126"));
  CHECK(has_value(vs, "127"));
}

TEST_CASE("16-bit twos-complement: -32768..32767", "[bes_value_generator]") {
  auto f = make_field("val", 16, primitive_encoding_type::twos_complement);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "-32768"));
  CHECK(has_value(vs, "32767"));
  CHECK(has_value(vs, "0"));
}

// ---------------------------------------------------------------------------
// Boolean
// ---------------------------------------------------------------------------

TEST_CASE("Boolean field: true and false", "[bes_value_generator]") {
  auto f = make_field("active", 1, primitive_encoding_type::boolean);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "true"));
  CHECK(has_value(vs, "false"));
}

// ---------------------------------------------------------------------------
// IEEE 754 floats
// ---------------------------------------------------------------------------

TEST_CASE("32-bit ieee754: representative values", "[bes_value_generator]") {
  auto f = make_field("price", 32, primitive_encoding_type::ieee754);
  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0"));
  CHECK(has_value(vs, "1"));
  CHECK(has_value(vs, "-1"));
}

// ---------------------------------------------------------------------------
// Null sentinel
// ---------------------------------------------------------------------------

TEST_CASE("Field with null sentinel includes sentinel value",
          "[bes_value_generator]") {
  field_type f;
  f.name = "price";
  f.bits = 32;
  f.encoding = primitive_encoding_type::unsigned_;
  f.null_value = "4294967295";

  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "4294967295")); // null sentinel
  // Should also have regular boundaries
  CHECK(has_value(vs, "0"));
}

TEST_CASE("Field with hex null sentinel", "[bes_value_generator]") {
  field_type f;
  f.name = "val";
  f.bits = 16;
  f.encoding = primitive_encoding_type::unsigned_;
  f.null_value = "0xFFFF";

  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "0xFFFF")); // sentinel as-is
}

// ---------------------------------------------------------------------------
// ASCII string
// ---------------------------------------------------------------------------

TEST_CASE("ASCII string field: boundary lengths", "[bes_value_generator]") {
  auto f = make_field("stock", 64, primitive_encoding_type::ascii);
  auto vs = bes_value_generator::generate(f);

  // 64 bits = 8 bytes = 8 characters
  CHECK(!vs.values.empty());
  bool has_full_length = false;
  for (const auto& v : vs.values) {
    if (v.xml_text.size() == 8) has_full_length = true;
  }
  CHECK(has_full_length);
}

// ---------------------------------------------------------------------------
// Enum
// ---------------------------------------------------------------------------

TEST_CASE("Enum field with enum_map: all entries", "[bes_value_generator]") {
  field_type f;
  f.name = "side";
  f.bits = 8;
  f.encoding = primitive_encoding_type::enum_char;
  xb::bes::enum_map_type em;
  em.entry.push_back({"buy", "B"});
  em.entry.push_back({"sell", "S"});
  f.enum_map = em;

  auto vs = bes_value_generator::generate(f);

  CHECK(has_value(vs, "buy"));
  CHECK(has_value(vs, "sell"));
}

// ---------------------------------------------------------------------------
// Reason metadata
// ---------------------------------------------------------------------------

TEST_CASE("BES values include reason metadata", "[bes_value_generator]") {
  auto f = make_field("val", 8, primitive_encoding_type::unsigned_);
  auto vs = bes_value_generator::generate(f);

  bool has_min = false;
  bool has_max = false;
  for (const auto& v : vs.values) {
    if (v.reason == "wire-min") has_min = true;
    if (v.reason == "wire-max") has_max = true;
  }
  CHECK(has_min);
  CHECK(has_max);
}
