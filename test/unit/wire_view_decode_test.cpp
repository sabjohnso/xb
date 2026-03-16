/// Golden-file decode tests for generated view patterns.
///
/// These tests verify that the accessor patterns emitted by
/// binary_codegen correctly decode hand-crafted byte buffers.
/// Each test constructs a span from known bytes and verifies
/// that the decode expressions produce the expected values.

#include <xb/wire/bits.hpp>
#include <xb/wire/byteswap.hpp>
#include <xb/wire/types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

using namespace xb::wire;

// ---------------------------------------------------------------------------
// Helper: byte literal
// ---------------------------------------------------------------------------

namespace {

  constexpr std::byte
  B(unsigned char c) {
    return static_cast<std::byte>(c);
  }

} // namespace

// ---------------------------------------------------------------------------
// Byte-aligned integer decode (big-endian)
// ---------------------------------------------------------------------------

TEST_CASE("view decode: big-endian uint32 at offset 0", "[wire][view_decode]") {
  // 0x01020304 = 16909060
  std::array<std::byte, 4> buf = {B(0x01), B(0x02), B(0x03), B(0x04)};
  std::span<const std::byte> span(buf);

  // Pattern: memcpy + from_wire<big>
  std::uint32_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  auto result = from_wire<std::endian::big>(v);

  CHECK(result == 0x01020304u);
}

TEST_CASE("view decode: big-endian uint64 at offset 0", "[wire][view_decode]") {
  std::array<std::byte, 8> buf = {B(0x00), B(0x00), B(0x00), B(0x00),
                                  B(0x00), B(0x0F), B(0x42), B(0x40)};
  std::span<const std::byte> span(buf);

  std::uint64_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  auto result = from_wire<std::endian::big>(v);

  CHECK(result == 1000000ULL);
}

TEST_CASE("view decode: big-endian uint16 at offset 4", "[wire][view_decode]") {
  // 4 bytes header + uint16 = 0x1234
  std::array<std::byte, 6> buf = {B(0x00), B(0x00), B(0x00),
                                  B(0x00), B(0x12), B(0x34)};
  std::span<const std::byte> span(buf);

  std::uint16_t v;
  std::memcpy(&v, span.data() + 4, sizeof(v));
  auto result = from_wire<std::endian::big>(v);

  CHECK(result == 0x1234u);
}

// ---------------------------------------------------------------------------
// Byte-aligned integer decode (little-endian)
// ---------------------------------------------------------------------------

TEST_CASE("view decode: little-endian uint32 at offset 0",
          "[wire][view_decode]") {
  // 0x04030201 in LE = 0x01020304
  std::array<std::byte, 4> buf = {B(0x04), B(0x03), B(0x02), B(0x01)};
  std::span<const std::byte> span(buf);

  std::uint32_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  auto result = from_wire<std::endian::little>(v);

  CHECK(result == 0x01020304u);
}

// ---------------------------------------------------------------------------
// Single-byte decode
// ---------------------------------------------------------------------------

TEST_CASE("view decode: uint8 at offset", "[wire][view_decode]") {
  std::array<std::byte, 3> buf = {B(0xAA), B(0xBB), B(0xCC)};
  std::span<const std::byte> span(buf);

  // Pattern for 1-byte: static_cast<uint8_t>(buf_[N])
  auto result = static_cast<std::uint8_t>(span[2]);

  CHECK(result == 0xCC);
}

// ---------------------------------------------------------------------------
// Signed integer decode
// ---------------------------------------------------------------------------

TEST_CASE("view decode: signed int32 big-endian negative",
          "[wire][view_decode]") {
  // -1 in big-endian = 0xFFFFFFFF
  std::array<std::byte, 4> buf = {B(0xFF), B(0xFF), B(0xFF), B(0xFF)};
  std::span<const std::byte> span(buf);

  std::int32_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  auto result = from_wire<std::endian::big>(v);

  CHECK(result == -1);
}

TEST_CASE("view decode: signed int16 big-endian", "[wire][view_decode]") {
  // -256 in big-endian = 0xFF00
  std::array<std::byte, 2> buf = {B(0xFF), B(0x00)};
  std::span<const std::byte> span(buf);

  std::int16_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  auto result = from_wire<std::endian::big>(v);

  CHECK(result == -256);
}

// ---------------------------------------------------------------------------
// Sub-byte bit-packed field decode
// ---------------------------------------------------------------------------

TEST_CASE("view decode: 3-bit and 5-bit fields MSB-first",
          "[wire][view_decode]") {
  // Byte: 0b_101_11010 = 0xBA
  //   bits 0-2: 101 = 5 (3-bit field)
  //   bits 3-7: 11010 = 26 (5-bit field)
  std::array<std::byte, 1> buf = {B(0xBA)};
  std::span<const std::byte> span(buf);

  auto flags = static_cast<std::uint8_t>(extract_bits<0, 3>(span));
  auto version = static_cast<std::uint8_t>(extract_bits<3, 5>(span));

  CHECK(flags == 5);
  CHECK(version == 26);
}

TEST_CASE("view decode: 12-bit field spanning two bytes",
          "[wire][view_decode]") {
  // Bytes: 0xAB 0xC0
  //   bits 0-11: 0xABC = 2748
  //   bits 12-15: 0 (unused)
  std::array<std::byte, 2> buf = {B(0xAB), B(0xC0)};
  std::span<const std::byte> span(buf);

  auto result = extract_bits<0, 12>(span);

  CHECK(result == 0xABC);
}

// ---------------------------------------------------------------------------
// Fixed-width string decode
// ---------------------------------------------------------------------------

TEST_CASE("view decode: fixed-width ASCII string", "[wire][view_decode]") {
  // "AAPL    " (8 bytes, space-padded)
  std::array<std::byte, 8> buf = {B('A'), B('A'), B('P'), B('L'),
                                  B(' '), B(' '), B(' '), B(' ')};
  std::span<const std::byte> span(buf);

  // Pattern: reinterpret_cast<const char*> + string_view
  auto result =
      std::string_view(reinterpret_cast<const char*>(span.data() + 0), 8);

  CHECK(result == "AAPL    ");
}

TEST_CASE("view decode: string at non-zero offset", "[wire][view_decode]") {
  // 4-byte header + "IBM " (4 bytes)
  std::array<std::byte, 8> buf = {B(0x00), B(0x01), B(0x02), B(0x03),
                                  B('I'),  B('B'),  B('M'),  B(' ')};
  std::span<const std::byte> span(buf);

  auto result =
      std::string_view(reinterpret_cast<const char*>(span.data() + 4), 4);

  CHECK(result == "IBM ");
}

// ---------------------------------------------------------------------------
// Raw binary data decode
// ---------------------------------------------------------------------------

TEST_CASE("view decode: raw binary span at offset", "[wire][view_decode]") {
  // 4-byte header + 8 bytes of raw payload
  std::array<std::byte, 12> buf = {B(0x00), B(0x01), B(0x02), B(0x03),
                                   B(0xDE), B(0xAD), B(0xBE), B(0xEF),
                                   B(0xCA), B(0xFE), B(0xBA), B(0xBE)};
  std::span<const std::byte> span(buf);

  // Pattern: buf_.subspan(offset, size)
  auto result = span.subspan(4, 8);

  REQUIRE(result.size() == 8);
  CHECK(result[0] == B(0xDE));
  CHECK(result[1] == B(0xAD));
  CHECK(result[7] == B(0xBE));
}

// ---------------------------------------------------------------------------
// Optional field decode (null sentinel)
// ---------------------------------------------------------------------------

TEST_CASE("view decode: optional uint32 with null sentinel - present",
          "[wire][view_decode]") {
  // price = 150000 = 0x000249F0, null = 0xFFFFFFFF
  std::array<std::byte, 4> buf = {B(0x00), B(0x02), B(0x49), B(0xF0)};
  std::span<const std::byte> span(buf);

  std::uint32_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  v = from_wire<std::endian::big>(v);
  auto result = (v == static_cast<std::uint32_t>(0xFFFFFFFF))
                    ? std::optional<std::uint32_t>(std::nullopt)
                    : std::optional<std::uint32_t>(v);

  REQUIRE(result.has_value());
  CHECK(*result == 150000);
}

TEST_CASE("view decode: optional uint32 with null sentinel - absent",
          "[wire][view_decode]") {
  // null = 0xFFFFFFFF
  std::array<std::byte, 4> buf = {B(0xFF), B(0xFF), B(0xFF), B(0xFF)};
  std::span<const std::byte> span(buf);

  std::uint32_t v;
  std::memcpy(&v, span.data() + 0, sizeof(v));
  v = from_wire<std::endian::big>(v);
  auto result = (v == static_cast<std::uint32_t>(0xFFFFFFFF))
                    ? std::optional<std::uint32_t>(std::nullopt)
                    : std::optional<std::uint32_t>(v);

  CHECK_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Multi-field message decode (ITCH-like)
// ---------------------------------------------------------------------------

TEST_CASE("view decode: ITCH-like message layout", "[wire][view_decode]") {
  // Message layout (big-endian):
  //   offset 0:  uint8  msg_type = 'A' (0x41)
  //   offset 1:  uint16 locate = 1234
  //   offset 3:  uint16 tracking = 5678
  //   offset 5:  uint64 timestamp = 1000000000 (nanoseconds)
  //   offset 13: uint64 order_ref = 42
  //   offset 21: uint8  side = 'B' (0x42)
  //   offset 22: uint32 shares = 100
  //   offset 26: 8-byte ASCII stock = "AAPL    "
  //   offset 34: uint32 price = 150000 (4 decimal places)

  std::array<std::byte, 38> buf{};
  auto* p = buf.data();

  // msg_type
  p[0] = B(0x41);
  // locate = 1234 = 0x04D2
  p[1] = B(0x04);
  p[2] = B(0xD2);
  // tracking = 5678 = 0x162E
  p[3] = B(0x16);
  p[4] = B(0x2E);
  // timestamp = 1000000000 = 0x3B9ACA00
  p[5] = B(0x00);
  p[6] = B(0x00);
  p[7] = B(0x00);
  p[8] = B(0x00);
  p[9] = B(0x3B);
  p[10] = B(0x9A);
  p[11] = B(0xCA);
  p[12] = B(0x00);
  // order_ref = 42 = 0x2A
  p[13] = B(0x00);
  p[14] = B(0x00);
  p[15] = B(0x00);
  p[16] = B(0x00);
  p[17] = B(0x00);
  p[18] = B(0x00);
  p[19] = B(0x00);
  p[20] = B(0x2A);
  // side
  p[21] = B(0x42);
  // shares = 100 = 0x64
  p[22] = B(0x00);
  p[23] = B(0x00);
  p[24] = B(0x00);
  p[25] = B(0x64);
  // stock
  p[26] = B('A');
  p[27] = B('A');
  p[28] = B('P');
  p[29] = B('L');
  p[30] = B(' ');
  p[31] = B(' ');
  p[32] = B(' ');
  p[33] = B(' ');
  // price = 150000 = 0x00024968 (well, 0x000249F0 actually)
  // Let's use 150000 = 0x000249F0
  p[34] = B(0x00);
  p[35] = B(0x02);
  p[36] = B(0x49);
  p[37] = B(0xF0);

  std::span<const std::byte> span(buf);

  // Decode each field using the generated accessor patterns

  // msg_type (uint8 at 0)
  CHECK(static_cast<std::uint8_t>(span[0]) == 0x41);

  // locate (uint16 at 1)
  std::uint16_t locate;
  std::memcpy(&locate, span.data() + 1, sizeof(locate));
  CHECK(from_wire<std::endian::big>(locate) == 1234);

  // tracking (uint16 at 3)
  std::uint16_t tracking;
  std::memcpy(&tracking, span.data() + 3, sizeof(tracking));
  CHECK(from_wire<std::endian::big>(tracking) == 5678);

  // timestamp (uint64 at 5)
  std::uint64_t timestamp;
  std::memcpy(&timestamp, span.data() + 5, sizeof(timestamp));
  CHECK(from_wire<std::endian::big>(timestamp) == 1000000000ULL);

  // order_ref (uint64 at 13)
  std::uint64_t order_ref;
  std::memcpy(&order_ref, span.data() + 13, sizeof(order_ref));
  CHECK(from_wire<std::endian::big>(order_ref) == 42ULL);

  // side (uint8 at 21)
  CHECK(static_cast<std::uint8_t>(span[21]) == 0x42);

  // shares (uint32 at 22)
  std::uint32_t shares;
  std::memcpy(&shares, span.data() + 22, sizeof(shares));
  CHECK(from_wire<std::endian::big>(shares) == 100);

  // stock (8-byte ASCII at 26)
  auto stock =
      std::string_view(reinterpret_cast<const char*>(span.data() + 26), 8);
  CHECK(stock == "AAPL    ");

  // price (uint32 at 34)
  std::uint32_t price;
  std::memcpy(&price, span.data() + 34, sizeof(price));
  CHECK(from_wire<std::endian::big>(price) == 150000);
}

// ---------------------------------------------------------------------------
// Round-trip: encode via mutator pattern, decode via accessor pattern
// ---------------------------------------------------------------------------

TEST_CASE("round-trip: big-endian uint32 encode then decode",
          "[wire][round_trip]") {
  std::array<std::byte, 4> buf{};
  std::span<std::byte> wspan(buf);

  // Encode: to_wire + memcpy (mutator pattern)
  std::uint32_t write_val = to_wire<std::endian::big>(std::uint32_t{42});
  std::memcpy(wspan.data() + 0, &write_val, sizeof(write_val));

  // Decode: memcpy + from_wire (accessor pattern)
  std::span<const std::byte> rspan(buf);
  std::uint32_t read_val;
  std::memcpy(&read_val, rspan.data() + 0, sizeof(read_val));
  CHECK(from_wire<std::endian::big>(read_val) == 42);
}

TEST_CASE("round-trip: little-endian uint64 encode then decode",
          "[wire][round_trip]") {
  std::array<std::byte, 8> buf{};
  std::span<std::byte> wspan(buf);

  std::uint64_t write_val =
      to_wire<std::endian::little>(std::uint64_t{1000000000ULL});
  std::memcpy(wspan.data() + 0, &write_val, sizeof(write_val));

  std::span<const std::byte> rspan(buf);
  std::uint64_t read_val;
  std::memcpy(&read_val, rspan.data() + 0, sizeof(read_val));
  CHECK(from_wire<std::endian::little>(read_val) == 1000000000ULL);
}

TEST_CASE("round-trip: single-byte encode then decode", "[wire][round_trip]") {
  std::array<std::byte, 1> buf{};

  // Encode
  buf[0] = static_cast<std::byte>(0xAB);

  // Decode
  std::span<const std::byte> rspan(buf);
  CHECK(static_cast<std::uint8_t>(rspan[0]) == 0xAB);
}

TEST_CASE("round-trip: sub-byte bit fields encode then decode",
          "[wire][round_trip]") {
  std::array<std::byte, 1> buf{};
  std::span<std::byte> wspan(buf);

  // Encode 3-bit field at offset 0, value = 5
  insert_bits<0, 3>(wspan, std::uint8_t{5});
  // Encode 5-bit field at offset 3, value = 26
  insert_bits<3, 5>(wspan, std::uint8_t{26});

  // Decode
  std::span<const std::byte> rspan(buf);
  CHECK(extract_bits<0, 3>(rspan) == 5);
  CHECK(extract_bits<3, 5>(rspan) == 26);
}

TEST_CASE("round-trip: string encode then decode", "[wire][round_trip]") {
  std::array<std::byte, 8> buf{};
  std::span<std::byte> wspan(buf);

  // Encode: copy string, pad with spaces
  std::string_view sym = "AAPL";
  std::memcpy(wspan.data(), sym.data(), sym.size());
  std::memset(wspan.data() + sym.size(), ' ', 8 - sym.size());

  // Decode
  std::span<const std::byte> rspan(buf);
  auto result =
      std::string_view(reinterpret_cast<const char*>(rspan.data()), 8);
  CHECK(result == "AAPL    ");
}

TEST_CASE("round-trip: multi-field message encode then decode",
          "[wire][round_trip]") {
  // 13-byte message: uint64 price + uint32 quantity + uint8 side
  std::array<std::byte, 13> buf{};
  std::span<std::byte> wspan(buf);

  // Encode
  auto price_w = to_wire<std::endian::big>(std::uint64_t{1500000});
  std::memcpy(wspan.data() + 0, &price_w, sizeof(price_w));

  auto qty_w = to_wire<std::endian::big>(std::uint32_t{100});
  std::memcpy(wspan.data() + 8, &qty_w, sizeof(qty_w));

  wspan[12] = static_cast<std::byte>(1); // side = Buy

  // Decode
  std::span<const std::byte> rspan(buf);

  std::uint64_t price_r;
  std::memcpy(&price_r, rspan.data() + 0, sizeof(price_r));
  CHECK(from_wire<std::endian::big>(price_r) == 1500000ULL);

  std::uint32_t qty_r;
  std::memcpy(&qty_r, rspan.data() + 8, sizeof(qty_r));
  CHECK(from_wire<std::endian::big>(qty_r) == 100);

  CHECK(static_cast<std::uint8_t>(rspan[12]) == 1);
}
