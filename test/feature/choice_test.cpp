/// BES discriminated choice test.
///
/// Verifies that BES <choice discriminant-field="..."> generates working
/// dispatch logic: per-alternative field accessors, discriminant accessor,
/// correct wire layout, and owned construction.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

using namespace choice_test;

// ===========================================================================
// TaggedMessage: variable-size alternatives
// ===========================================================================

TEST_CASE("choice: heartbeat (tag=1) round-trip", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(1);
  msg.set_sequence(42);

  TaggedMessage_view view(msg.buffer());
  CHECK(view.tag() == 1);
  CHECK(view.sequence() == 42);
}

TEST_CASE("choice: temperature (tag=2) round-trip", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(2);
  msg.set_sensor_id(0x0A0B);
  msg.set_value(98765);

  TaggedMessage_view view(msg.buffer());
  CHECK(view.tag() == 2);
  CHECK(view.sensor_id() == 0x0A0B);
  CHECK(view.value() == 98765);
}

TEST_CASE("choice: alert (tag=3) round-trip", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(3);
  msg.set_level(5);
  msg.set_code(0x1234);

  TaggedMessage_view view(msg.buffer());
  CHECK(view.tag() == 3);
  CHECK(view.level() == 5);
  CHECK(view.code() == 0x1234);
}

// ===========================================================================
// TaggedMessage: wire layout verification
// ===========================================================================

namespace {

  auto
  read_be16(std::span<const std::byte> buf, std::size_t off) -> std::uint16_t {
    return (static_cast<std::uint16_t>(buf[off]) << 8) |
           static_cast<std::uint16_t>(buf[off + 1]);
  }

  auto
  read_be32(std::span<const std::byte> buf, std::size_t off) -> std::uint32_t {
    return (static_cast<std::uint32_t>(buf[off]) << 24) |
           (static_cast<std::uint32_t>(buf[off + 1]) << 16) |
           (static_cast<std::uint32_t>(buf[off + 2]) << 8) |
           static_cast<std::uint32_t>(buf[off + 3]);
  }

} // namespace

TEST_CASE("choice: heartbeat wire layout", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(1);
  msg.set_sequence(0xDEADBEEF);

  auto buf = msg.buffer();
  // tag at offset 0
  CHECK(static_cast<std::uint8_t>(buf[0]) == 1);
  // sequence at offset 1 (big-endian)
  CHECK(read_be32(buf, 1) == 0xDEADBEEF);
}

TEST_CASE("choice: temperature wire layout", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(2);
  msg.set_sensor_id(0x0102);
  msg.set_value(0x03040506);

  auto buf = msg.buffer();
  CHECK(static_cast<std::uint8_t>(buf[0]) == 2);
  CHECK(read_be16(buf, 1) == 0x0102);
  CHECK(read_be32(buf, 3) == 0x03040506);
}

TEST_CASE("choice: alert wire layout", "[choice]") {
  TaggedMessage_owned msg;
  msg.set_tag(3);
  msg.set_level(0xFF);
  msg.set_code(0xABCD);

  auto buf = msg.buffer();
  CHECK(static_cast<std::uint8_t>(buf[0]) == 3);
  CHECK(static_cast<std::uint8_t>(buf[1]) == 0xFF);
  CHECK(read_be16(buf, 2) == 0xABCD);
}

// ===========================================================================
// FixedChoice: all alternatives same size → is_fixed, known wire_size
// ===========================================================================

TEST_CASE("choice: FixedChoice wire_size", "[choice]") {
  // kind(8) + body(32) = 40 bits = 5 bytes
  CHECK(FixedChoice_owned::wire_size == 5);
}

TEST_CASE("choice: FixedChoice alpha round-trip", "[choice]") {
  FixedChoice_owned msg;
  msg.set_kind(1);
  msg.set_x(12345);

  FixedChoice_view view(msg.buffer());
  CHECK(view.kind() == 1);
  CHECK(view.x() == 12345);
}

TEST_CASE("choice: FixedChoice beta round-trip", "[choice]") {
  FixedChoice_owned msg;
  msg.set_kind(2);
  msg.set_y(99999);

  FixedChoice_view view(msg.buffer());
  CHECK(view.kind() == 2);
  CHECK(view.y() == 99999);
}

TEST_CASE("choice: FixedChoice gamma round-trip", "[choice]") {
  FixedChoice_owned msg;
  msg.set_kind(3);
  msg.set_z(0xCAFEBABE);

  FixedChoice_view view(msg.buffer());
  CHECK(view.kind() == 3);
  CHECK(view.z() == 0xCAFEBABE);
}

TEST_CASE("choice: FixedChoice wire layout", "[choice]") {
  FixedChoice_owned msg;
  msg.set_kind(2);
  msg.set_y(0x01020304);

  auto buf = msg.buffer();
  CHECK(static_cast<std::uint8_t>(buf[0]) == 2);
  CHECK(read_be32(buf, 1) == 0x01020304);
}
