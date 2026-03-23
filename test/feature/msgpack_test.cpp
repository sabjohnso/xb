/// MessagePack binary encoding test.
///
/// Verifies that BES-generated types produce byte-exact MessagePack
/// encoding matching the specification at https://msgpack.org/.
///
/// MessagePack uses big-endian byte order with MSB-first bit packing.
/// The first byte determines the type — several "fix" types pack a
/// tag and small value into sub-byte fields.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

using namespace msgpack;

// ===========================================================================
// Helpers
// ===========================================================================

namespace {

  template <std::size_t N>
  bool
  bytes_match(std::span<const std::byte> buf,
              const std::array<std::uint8_t, N>& expected) {
    if (buf.size() < N) return false;
    for (std::size_t i = 0; i < N; ++i) {
      if (static_cast<std::uint8_t>(buf[i]) != expected[i]) return false;
    }
    return true;
  }

} // namespace

// ===========================================================================
// Wire sizes
// ===========================================================================

TEST_CASE("MessagePack wire sizes", "[msgpack]") {
  CHECK(PositiveFixint_owned::wire_size == 1);
  CHECK(NegativeFixint_owned::wire_size == 1);
  CHECK(Fixmap_owned::wire_size == 1);
  CHECK(Fixarray_owned::wire_size == 1);
  CHECK(Fixstr_owned::wire_size == 1);
  CHECK(FormatOnly_owned::wire_size == 1);
  CHECK(Format8_owned::wire_size == 2);
  CHECK(Format16_owned::wire_size == 3);
  CHECK(Format32_owned::wire_size == 5);
  CHECK(Format64_owned::wire_size == 9);
  CHECK(Fixext1_owned::wire_size == 3);
  CHECK(Fixext2_owned::wire_size == 4);
  CHECK(Fixext4_owned::wire_size == 6);
}

// ===========================================================================
// Positive fixint: 0xxxxxxx (0x00-0x7f)
// ===========================================================================

TEST_CASE("MessagePack positive fixint", "[msgpack]") {
  SECTION("0 → 0x00") {
    PositiveFixint_owned h;
    h.set_tag(0);
    h.set_value(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x00}));
  }

  SECTION("1 → 0x01") {
    PositiveFixint_owned h;
    h.set_tag(0);
    h.set_value(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x01}));
  }

  SECTION("42 → 0x2a") {
    PositiveFixint_owned h;
    h.set_tag(0);
    h.set_value(42);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x2a}));
  }

  SECTION("127 → 0x7f") {
    PositiveFixint_owned h;
    h.set_tag(0);
    h.set_value(127);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x7f}));
  }
}

TEST_CASE("MessagePack positive fixint: exhaustive round-trip", "[msgpack]") {
  for (unsigned v = 0; v < 128; ++v) {
    PositiveFixint_owned h;
    h.set_tag(0);
    h.set_value(static_cast<std::uint8_t>(v));

    PositiveFixint_view view(h.buffer());
    CHECK(view.tag() == 0);
    CHECK(view.value() == v);
    CHECK(static_cast<std::uint8_t>(h.buffer()[0]) == v);
  }
}

// ===========================================================================
// Negative fixint: 111xxxxx (0xe0-0xff) encodes -32 to -1
//   value field = unsigned 5-bit, representing the magnitude offset
//   byte = 0xe0 | value, decoded as (int8_t)byte
// ===========================================================================

TEST_CASE("MessagePack negative fixint", "[msgpack]") {
  SECTION("-1 → 0xff") {
    NegativeFixint_owned h;
    h.set_tag(0x7);  // 111
    h.set_value(31); // 0xff = 111_11111
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xff}));
    // Verify: (int8_t)0xff == -1
    CHECK(static_cast<std::int8_t>(static_cast<std::uint8_t>(h.buffer()[0])) ==
          -1);
  }

  SECTION("-32 → 0xe0") {
    NegativeFixint_owned h;
    h.set_tag(0x7); // 111
    h.set_value(0); // 0xe0 = 111_00000
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xe0}));
    CHECK(static_cast<std::int8_t>(static_cast<std::uint8_t>(h.buffer()[0])) ==
          -32);
  }

  SECTION("-10 → 0xf6") {
    NegativeFixint_owned h;
    h.set_tag(0x7);
    h.set_value(22); // 0xf6 = 111_10110, (int8_t)0xf6 = -10
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf6}));
    CHECK(static_cast<std::int8_t>(static_cast<std::uint8_t>(h.buffer()[0])) ==
          -10);
  }
}

TEST_CASE("MessagePack negative fixint: exhaustive", "[msgpack]") {
  for (unsigned v = 0; v < 32; ++v) {
    NegativeFixint_owned h;
    h.set_tag(0x7);
    h.set_value(static_cast<std::uint8_t>(v));

    NegativeFixint_view view(h.buffer());
    CHECK(view.tag() == 0x7);
    CHECK(view.value() == v);

    auto byte = static_cast<std::uint8_t>(h.buffer()[0]);
    CHECK(byte == (0xe0 | v));
    auto decoded = static_cast<std::int8_t>(byte);
    CHECK(decoded == static_cast<std::int8_t>(-32 + static_cast<int>(v)));
  }
}

// ===========================================================================
// Fixmap: 1000xxxx (0x80-0x8f)
// ===========================================================================

TEST_CASE("MessagePack fixmap", "[msgpack]") {
  SECTION("empty map → 0x80") {
    Fixmap_owned h;
    h.set_tag(0x8);
    h.set_count(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x80}));
  }

  SECTION("1-entry map → 0x81") {
    Fixmap_owned h;
    h.set_tag(0x8);
    h.set_count(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x81}));
  }

  SECTION("15-entry map → 0x8f") {
    Fixmap_owned h;
    h.set_tag(0x8);
    h.set_count(15);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x8f}));
  }
}

// ===========================================================================
// Fixarray: 1001xxxx (0x90-0x9f)
// ===========================================================================

TEST_CASE("MessagePack fixarray", "[msgpack]") {
  SECTION("empty array → 0x90") {
    Fixarray_owned h;
    h.set_tag(0x9);
    h.set_count(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x90}));
  }

  SECTION("3-item array → 0x93") {
    Fixarray_owned h;
    h.set_tag(0x9);
    h.set_count(3);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x93}));
  }

  SECTION("15-item array → 0x9f") {
    Fixarray_owned h;
    h.set_tag(0x9);
    h.set_count(15);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x9f}));
  }
}

// ===========================================================================
// Fixstr: 101xxxxx (0xa0-0xbf)
// ===========================================================================

TEST_CASE("MessagePack fixstr", "[msgpack]") {
  SECTION("empty string → 0xa0") {
    Fixstr_owned h;
    h.set_tag(0x5);
    h.set_length(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xa0}));
  }

  SECTION("1-byte string → 0xa1") {
    Fixstr_owned h;
    h.set_tag(0x5);
    h.set_length(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xa1}));
  }

  SECTION("31-byte string → 0xbf") {
    Fixstr_owned h;
    h.set_tag(0x5);
    h.set_length(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xbf}));
  }
}

// ===========================================================================
// Single-byte format types: nil, false, true
// ===========================================================================

TEST_CASE("MessagePack nil/false/true", "[msgpack]") {
  SECTION("nil → 0xc0") {
    FormatOnly_owned h;
    h.set_format(0xc0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xc0}));
  }

  SECTION("false → 0xc2") {
    FormatOnly_owned h;
    h.set_format(0xc2);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xc2}));
  }

  SECTION("true → 0xc3") {
    FormatOnly_owned h;
    h.set_format(0xc3);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xc3}));
  }
}

// ===========================================================================
// Unsigned integers: uint8 (0xcc), uint16 (0xcd), uint32 (0xce), uint64 (0xcf)
// ===========================================================================

TEST_CASE("MessagePack unsigned integers", "[msgpack]") {
  SECTION("uint8 200 → 0xcc 0xc8") {
    Format8_owned h;
    h.set_format(0xcc);
    h.set_argument(200);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xcc, 0xc8}));
  }

  SECTION("uint16 1000 → 0xcd 0x03e8") {
    Format16_owned h;
    h.set_format(0xcd);
    h.set_argument(1000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xcd, 0x03, 0xe8}));
  }

  SECTION("uint32 100000 → 0xce 0x000186a0") {
    Format32_owned h;
    h.set_format(0xce);
    h.set_argument(100000);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xce, 0x00, 0x01, 0x86, 0xa0}));
  }

  SECTION("uint64 1000000000000 → 0xcf + 8 bytes") {
    Format64_owned h;
    h.set_format(0xcf);
    h.set_argument(1000000000000ULL);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xcf, 0x00, 0x00, 0x00, 0xe8,
                                                  0xd4, 0xa5, 0x10, 0x00}));
  }
}

// ===========================================================================
// Signed integers: int8 (0xd0), int16 (0xd1), int32 (0xd2), int64 (0xd3)
//   Argument is stored as two's complement big-endian.
//   We store the raw bits via memcpy.
// ===========================================================================

TEST_CASE("MessagePack signed integers", "[msgpack]") {
  SECTION("int8 -33 → 0xd0 0xdf") {
    Format8_owned h;
    h.set_format(0xd0);
    std::int8_t val = -33;
    h.set_argument(static_cast<std::uint8_t>(val));
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xd0, 0xdf}));
  }

  SECTION("int16 -1000 → 0xd1 0xfc18") {
    Format16_owned h;
    h.set_format(0xd1);
    std::int16_t val = -1000;
    std::uint16_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xd1, 0xfc, 0x18}));
  }

  SECTION("int32 -100000 → 0xd2 0xfffe7960") {
    Format32_owned h;
    h.set_format(0xd2);
    std::int32_t val = -100000;
    std::uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xd2, 0xff, 0xfe, 0x79, 0x60}));
  }

  SECTION("int64 -1000000000000 → 0xd3 + 8 bytes") {
    Format64_owned h;
    h.set_format(0xd3);
    std::int64_t val = -1000000000000LL;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xd3, 0xff, 0xff, 0xff, 0x17,
                                                  0x2b, 0x5a, 0xf0, 0x00}));
  }
}

// ===========================================================================
// Floats: float32 (0xca), float64 (0xcb)
// ===========================================================================

TEST_CASE("MessagePack floats", "[msgpack]") {
  SECTION("float32 1.5 → 0xca 0x3fc00000") {
    Format32_owned h;
    h.set_format(0xca);
    float val = 1.5f;
    std::uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xca, 0x3f, 0xc0, 0x00, 0x00}));
  }

  SECTION("float64 1.1 → 0xcb 0x3ff199999999999a") {
    Format64_owned h;
    h.set_format(0xcb);
    double val = 1.1;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xcb, 0x3f, 0xf1, 0x99, 0x99,
                                                  0x99, 0x99, 0x99, 0x9a}));
  }

  SECTION("float64 -0.0 → 0xcb 0x8000000000000000") {
    Format64_owned h;
    h.set_format(0xcb);
    double val = -0.0;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xcb, 0x80, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00}));
  }
}

// ===========================================================================
// String/binary/array/map length-prefixed heads
// ===========================================================================

TEST_CASE("MessagePack str/bin/array/map heads", "[msgpack]") {
  SECTION("str8 length=100 → 0xd9 0x64") {
    Format8_owned h;
    h.set_format(0xd9);
    h.set_argument(100);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xd9, 0x64}));
  }

  SECTION("str16 length=1000 → 0xda 0x03e8") {
    Format16_owned h;
    h.set_format(0xda);
    h.set_argument(1000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xda, 0x03, 0xe8}));
  }

  SECTION("str32 length=100000 → 0xdb 0x000186a0") {
    Format32_owned h;
    h.set_format(0xdb);
    h.set_argument(100000);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xdb, 0x00, 0x01, 0x86, 0xa0}));
  }

  SECTION("bin8 length=50 → 0xc4 0x32") {
    Format8_owned h;
    h.set_format(0xc4);
    h.set_argument(50);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xc4, 0x32}));
  }

  SECTION("array16 count=1000 → 0xdc 0x03e8") {
    Format16_owned h;
    h.set_format(0xdc);
    h.set_argument(1000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xdc, 0x03, 0xe8}));
  }

  SECTION("map16 count=500 → 0xde 0x01f4") {
    Format16_owned h;
    h.set_format(0xde);
    h.set_argument(500);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xde, 0x01, 0xf4}));
  }
}

// ===========================================================================
// Ext types: fixext1 (0xd4), fixext2 (0xd5), fixext4 (0xd6)
// ===========================================================================

TEST_CASE("MessagePack fixext types", "[msgpack]") {
  SECTION("fixext1: type=1, data=0x42 → 0xd4 0x01 0x42") {
    Fixext1_owned h;
    h.set_format(0xd4);
    h.set_type_id(1);
    h.set_data(0x42);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xd4, 0x01, 0x42}));
  }

  SECTION("fixext2: type=2, data=0xABCD → 0xd5 0x02 0xabcd") {
    Fixext2_owned h;
    h.set_format(0xd5);
    h.set_type_id(2);
    h.set_data(0xABCD);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 4>{0xd5, 0x02, 0xab, 0xcd}));
  }

  SECTION("fixext4: type=3, data=0xDEADBEEF → 0xd6 0x03 0xdeadbeef") {
    Fixext4_owned h;
    h.set_format(0xd6);
    h.set_type_id(3);
    h.set_data(0xDEADBEEF);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 6>{
                                      0xd6, 0x03, 0xde, 0xad, 0xbe, 0xef}));
  }
}

// ===========================================================================
// Round-trip: all fix types
// ===========================================================================

TEST_CASE("MessagePack fixmap: exhaustive round-trip", "[msgpack]") {
  for (unsigned c = 0; c < 16; ++c) {
    Fixmap_owned h;
    h.set_tag(0x8);
    h.set_count(static_cast<std::uint8_t>(c));

    Fixmap_view view(h.buffer());
    CHECK(view.tag() == 0x8);
    CHECK(view.count() == c);
    CHECK(static_cast<std::uint8_t>(h.buffer()[0]) == (0x80 | c));
  }
}

TEST_CASE("MessagePack fixarray: exhaustive round-trip", "[msgpack]") {
  for (unsigned c = 0; c < 16; ++c) {
    Fixarray_owned h;
    h.set_tag(0x9);
    h.set_count(static_cast<std::uint8_t>(c));

    Fixarray_view view(h.buffer());
    CHECK(view.tag() == 0x9);
    CHECK(view.count() == c);
    CHECK(static_cast<std::uint8_t>(h.buffer()[0]) == (0x90 | c));
  }
}

TEST_CASE("MessagePack fixstr: exhaustive round-trip", "[msgpack]") {
  for (unsigned len = 0; len < 32; ++len) {
    Fixstr_owned h;
    h.set_tag(0x5);
    h.set_length(static_cast<std::uint8_t>(len));

    Fixstr_view view(h.buffer());
    CHECK(view.tag() == 0x5);
    CHECK(view.length() == len);
    CHECK(static_cast<std::uint8_t>(h.buffer()[0]) == (0xa0 | len));
  }
}

TEST_CASE("MessagePack Format16: round-trip", "[msgpack]") {
  Format16_owned h;
  h.set_format(0xcd); // uint16
  h.set_argument(0xABCD);

  Format16_view view(h.buffer());
  CHECK(view.format() == 0xcd);
  CHECK(view.argument() == 0xABCD);
}

TEST_CASE("MessagePack Format32: round-trip", "[msgpack]") {
  Format32_owned h;
  h.set_format(0xce); // uint32
  h.set_argument(0xDEADBEEF);

  Format32_view view(h.buffer());
  CHECK(view.format() == 0xce);
  CHECK(view.argument() == 0xDEADBEEF);
}

TEST_CASE("MessagePack Format64: round-trip", "[msgpack]") {
  Format64_owned h;
  h.set_format(0xcf); // uint64
  h.set_argument(0x0123456789ABCDEFULL);

  Format64_view view(h.buffer());
  CHECK(view.format() == 0xcf);
  CHECK(view.argument() == 0x0123456789ABCDEFULL);
}
