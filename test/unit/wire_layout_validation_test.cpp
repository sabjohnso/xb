#include <xb/wire/layout_validation.hpp>

#include <xb/wire/encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb::bes;
using namespace xb::wire;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

  field_type
  make_field(const std::string& name, unsigned bits,
             const std::string& xsd_type = "") {
    field_type f;
    f.name = name;
    f.bits = bits;
    if (!xsd_type.empty()) f.type = xsd_type;
    return f;
  }

} // namespace

// ---------------------------------------------------------------------------
// Valid widths
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: uint8 field with 8 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("price", 8, "xs:unsignedByte");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: uint16 field with 16 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("locate", 16, "xs:unsignedShort");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: uint32 field with 32 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("shares", 32, "xs:unsignedInt");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: uint64 field with 64 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("order_ref", 64, "xs:unsignedLong");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: signed int32 with 32 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("delta", 32, "xs:int");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: boolean with 1 bit is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("flag", 1, "xs:boolean");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: boolean with 8 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("flag", 8, "xs:boolean");
  CHECK(result.valid);
}

// ---------------------------------------------------------------------------
// Wider than minimum is valid
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: uint8 in 16 bits is valid (wider ok)",
          "[wire][layout_validation]") {
  auto result = validate_field_width("value", 16, "xs:unsignedByte");
  CHECK(result.valid);
}

TEST_CASE("layout_validation: int16 in 32 bits is valid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("value", 32, "xs:short");
  CHECK(result.valid);
}

// ---------------------------------------------------------------------------
// Too narrow — invalid
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: uint16 in 8 bits is invalid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("locate", 8, "xs:unsignedShort");
  CHECK_FALSE(result.valid);
  CHECK_FALSE(result.message.empty());
}

TEST_CASE("layout_validation: int32 in 16 bits is invalid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("delta", 16, "xs:int");
  CHECK_FALSE(result.valid);
}

TEST_CASE("layout_validation: uint64 in 32 bits is invalid",
          "[wire][layout_validation]") {
  auto result = validate_field_width("order_ref", 32, "xs:unsignedLong");
  CHECK_FALSE(result.valid);
}

// ---------------------------------------------------------------------------
// String and binary types — any width is valid
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: string with any width is valid",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("sym", 64, "xs:string").valid);
  CHECK(validate_field_width("sym", 8, "xs:string").valid);
  CHECK(validate_field_width("sym", 256, "xs:string").valid);
}

TEST_CASE("layout_validation: hexBinary with any width is valid",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("data", 128, "xs:hexBinary").valid);
}

// ---------------------------------------------------------------------------
// Float and double
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: float needs at least 32 bits",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("f", 32, "xs:float").valid);
  CHECK_FALSE(validate_field_width("f", 16, "xs:float").valid);
}

TEST_CASE("layout_validation: double needs at least 64 bits",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("d", 64, "xs:double").valid);
  CHECK_FALSE(validate_field_width("d", 32, "xs:double").valid);
}

// ---------------------------------------------------------------------------
// Unknown types pass (no constraint)
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: unknown XSD type passes",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("x", 8, "xs:customType").valid);
}

TEST_CASE("layout_validation: empty type string passes",
          "[wire][layout_validation]") {
  CHECK(validate_field_width("x", 8, "").valid);
}

// ---------------------------------------------------------------------------
// Batch validation of a message
// ---------------------------------------------------------------------------

TEST_CASE("layout_validation: validate_message reports all errors",
          "[wire][layout_validation]") {
  message_type msg;
  msg.name = "Bad";

  // Good field
  auto f1 = make_field("ok_field", 32, "xs:unsignedInt");
  msg.choice.push_back(std::move(f1));

  // Bad field: 8 bits too narrow for xs:int (needs 32)
  auto f2 = make_field("bad_field", 8, "xs:int");
  msg.choice.push_back(std::move(f2));

  // Bad field: 16 bits too narrow for xs:unsignedLong (needs 64)
  auto f3 = make_field("worse_field", 16, "xs:unsignedLong");
  msg.choice.push_back(std::move(f3));

  auto errors = validate_message_fields(msg);
  REQUIRE(errors.size() == 2);
  CHECK(errors[0].field_name == "bad_field");
  CHECK(errors[1].field_name == "worse_field");
}

TEST_CASE("layout_validation: valid message returns no errors",
          "[wire][layout_validation]") {
  message_type msg;
  msg.name = "Good";
  msg.choice.push_back(make_field("a", 32, "xs:unsignedInt"));
  msg.choice.push_back(make_field("b", 64, "xs:unsignedLong"));

  auto errors = validate_message_fields(msg);
  CHECK(errors.empty());
}
