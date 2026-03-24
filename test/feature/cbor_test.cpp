/// CBOR (RFC 8949) binary encoding test.
///
/// Verifies that BES-generated types produce byte-exact CBOR encoding
/// matching the RFC 8949 specification and Appendix A test vectors.
///
/// CBOR uses big-endian byte order with MSB-first bit packing:
///   Initial byte: major_type[7:5] | additional[4:0]
///   Followed by 0, 1, 2, 4, or 8 argument bytes depending on additional.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

using namespace cbor;

// ===========================================================================
// Helpers
// ===========================================================================

namespace {

  /// Compare a buffer against expected hex bytes.
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

  /// CBOR major types
  constexpr std::uint8_t MT_UINT = 0;   // unsigned integer
  constexpr std::uint8_t MT_NINT = 1;   // negative integer
  constexpr std::uint8_t MT_BSTR = 2;   // byte string
  constexpr std::uint8_t MT_TSTR = 3;   // text string
  constexpr std::uint8_t MT_ARRAY = 4;  // array
  constexpr std::uint8_t MT_MAP = 5;    // map
  constexpr std::uint8_t MT_TAG = 6;    // tag
  constexpr std::uint8_t MT_SIMPLE = 7; // simple/float

} // namespace

// ===========================================================================
// Wire sizes
// ===========================================================================

TEST_CASE("CBOR wire sizes", "[cbor]") {
  CHECK(Head0_owned::wire_size == 1);
  CHECK(Head8_owned::wire_size == 2);
  CHECK(Head16_owned::wire_size == 3);
  CHECK(Head32_owned::wire_size == 5);
  CHECK(Head64_owned::wire_size == 9);
}

// ===========================================================================
// Initial byte bit layout
// ===========================================================================

TEST_CASE("CBOR initial byte: major_type in high 3 bits", "[cbor]") {
  Head0_owned h;
  h.set_major_type(MT_UINT);
  h.set_additional(0);

  auto buf = h.buffer();
  // major_type=0, additional=0 → byte = 0x00
  CHECK(static_cast<std::uint8_t>(buf[0]) == 0x00);

  h.set_major_type(MT_NINT);
  h.set_additional(0);
  buf = h.buffer();
  // major_type=1 → byte = 0x20 (001_00000)
  CHECK(static_cast<std::uint8_t>(buf[0]) == 0x20);

  h.set_major_type(MT_TSTR);
  h.set_additional(0);
  buf = h.buffer();
  // major_type=3 → byte = 0x60 (011_00000)
  CHECK(static_cast<std::uint8_t>(buf[0]) == 0x60);

  h.set_major_type(MT_SIMPLE);
  h.set_additional(20); // false
  buf = h.buffer();
  // major_type=7, additional=20 → byte = 0xF4 (111_10100)
  CHECK(static_cast<std::uint8_t>(buf[0]) == 0xF4);
}

TEST_CASE("CBOR initial byte: round-trip all major types", "[cbor]") {
  for (std::uint8_t mt = 0; mt <= 7; ++mt) {
    Head0_owned h;
    h.set_major_type(mt);
    h.set_additional(0);
    Head0_view view(h.buffer());
    CHECK(view.major_type() == mt);
    CHECK(view.additional() == 0);
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Unsigned integers (major type 0)
// ===========================================================================

TEST_CASE("CBOR unsigned integers", "[cbor][appendix-a]") {
  SECTION("0 → 0x00") {
    Head0_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x00}));
  }

  SECTION("1 → 0x01") {
    Head0_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x01}));
  }

  SECTION("10 → 0x0a") {
    Head0_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(10);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x0a}));
  }

  SECTION("23 → 0x17") {
    Head0_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(23);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x17}));
  }

  SECTION("24 → 0x18 0x18") {
    Head8_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(24);
    h.set_argument(24);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x18, 0x18}));
  }

  SECTION("25 → 0x18 0x19") {
    Head8_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(24);
    h.set_argument(25);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x18, 0x19}));
  }

  SECTION("100 → 0x18 0x64") {
    Head8_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(24);
    h.set_argument(100);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x18, 0x64}));
  }

  SECTION("1000 → 0x19 0x03e8") {
    Head16_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(25);
    h.set_argument(1000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0x19, 0x03, 0xe8}));
  }

  SECTION("1000000 → 0x1a 0x000f4240") {
    Head32_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(26);
    h.set_argument(1000000);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0x1a, 0x00, 0x0f, 0x42, 0x40}));
  }

  SECTION("1000000000000 → 0x1b 0x000000e8d4a51000") {
    Head64_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(27);
    h.set_argument(1000000000000ULL);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0x1b, 0x00, 0x00, 0x00, 0xe8,
                                                  0xd4, 0xa5, 0x10, 0x00}));
  }

  SECTION("18446744073709551615 → 0x1b 0xffffffffffffffff") {
    Head64_owned h;
    h.set_major_type(MT_UINT);
    h.set_additional(27);
    h.set_argument(UINT64_MAX);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0x1b, 0xff, 0xff, 0xff, 0xff,
                                                  0xff, 0xff, 0xff, 0xff}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Negative integers (major type 1)
//   CBOR encodes -1-n, so -1 is encoded as major=1, argument=0
// ===========================================================================

TEST_CASE("CBOR negative integers", "[cbor][appendix-a]") {
  SECTION("-1 → 0x20") {
    Head0_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(0); // -1-0 = -1
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x20}));
  }

  SECTION("-10 → 0x29") {
    Head0_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(9); // -1-9 = -10
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x29}));
  }

  SECTION("-24 → 0x37") {
    Head0_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(23); // -1-23 = -24
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x37}));
  }

  SECTION("-25 → 0x38 0x18") {
    Head8_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(24);
    h.set_argument(24); // -1-24 = -25
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x38, 0x18}));
  }

  SECTION("-100 → 0x38 0x63") {
    Head8_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(24);
    h.set_argument(99); // -1-99 = -100
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x38, 0x63}));
  }

  SECTION("-1000 → 0x39 0x03e7") {
    Head16_owned h;
    h.set_major_type(MT_NINT);
    h.set_additional(25);
    h.set_argument(999); // -1-999 = -1000
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0x39, 0x03, 0xe7}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Simple values (major type 7)
// ===========================================================================

TEST_CASE("CBOR simple values", "[cbor][appendix-a]") {
  SECTION("false → 0xf4") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(20);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf4}));
  }

  SECTION("true → 0xf5") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(21);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf5}));
  }

  SECTION("null → 0xf6") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(22);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf6}));
  }

  SECTION("undefined → 0xf7") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(23);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf7}));
  }

  SECTION("simple(16) → 0xf0") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(16);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xf0}));
  }

  SECTION("simple(255) → 0xf8 0xff") {
    Head8_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(24);
    h.set_argument(255);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xf8, 0xff}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Byte strings (major type 2)
//   We test the head only (the string bytes follow the head)
// ===========================================================================

TEST_CASE("CBOR byte string heads", "[cbor][appendix-a]") {
  SECTION("h'' (empty) → 0x40") {
    Head0_owned h;
    h.set_major_type(MT_BSTR);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x40}));
  }

  SECTION("h'01020304' (4 bytes) → 0x44") {
    Head0_owned h;
    h.set_major_type(MT_BSTR);
    h.set_additional(4);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x44}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Text strings (major type 3)
// ===========================================================================

TEST_CASE("CBOR text string heads", "[cbor][appendix-a]") {
  SECTION("\"\" (empty) → 0x60") {
    Head0_owned h;
    h.set_major_type(MT_TSTR);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x60}));
  }

  SECTION("\"a\" (1 byte) → 0x61") {
    Head0_owned h;
    h.set_major_type(MT_TSTR);
    h.set_additional(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x61}));
  }

  SECTION("\"IETF\" (4 bytes) → 0x64") {
    Head0_owned h;
    h.set_major_type(MT_TSTR);
    h.set_additional(4);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x64}));
  }

  SECTION("256-byte string → 0x79 0x0100") {
    Head16_owned h;
    h.set_major_type(MT_TSTR);
    h.set_additional(25);
    h.set_argument(256);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0x79, 0x01, 0x00}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Arrays (major type 4)
// ===========================================================================

TEST_CASE("CBOR array heads", "[cbor][appendix-a]") {
  SECTION("[] (empty) → 0x80") {
    Head0_owned h;
    h.set_major_type(MT_ARRAY);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x80}));
  }

  SECTION("[_, _, _] (3 items) → 0x83") {
    Head0_owned h;
    h.set_major_type(MT_ARRAY);
    h.set_additional(3);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x83}));
  }

  SECTION("25-item array → 0x98 0x19") {
    Head8_owned h;
    h.set_major_type(MT_ARRAY);
    h.set_additional(24);
    h.set_argument(25);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0x98, 0x19}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Maps (major type 5)
// ===========================================================================

TEST_CASE("CBOR map heads", "[cbor][appendix-a]") {
  SECTION("{} (empty) → 0xa0") {
    Head0_owned h;
    h.set_major_type(MT_MAP);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xa0}));
  }

  SECTION("{_, _, _, _, _} (5 pairs) → 0xa5") {
    Head0_owned h;
    h.set_major_type(MT_MAP);
    h.set_additional(5);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xa5}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Tags (major type 6)
// ===========================================================================

TEST_CASE("CBOR tag heads", "[cbor][appendix-a]") {
  SECTION("tag(0) date/time string → 0xc0") {
    Head0_owned h;
    h.set_major_type(MT_TAG);
    h.set_additional(0);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xc0}));
  }

  SECTION("tag(1) epoch timestamp → 0xc1") {
    Head0_owned h;
    h.set_major_type(MT_TAG);
    h.set_additional(1);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xc1}));
  }

  SECTION("tag(23) expected base16 → 0xd7") {
    Head0_owned h;
    h.set_major_type(MT_TAG);
    h.set_additional(23);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xd7}));
  }

  SECTION("tag(24) embedded CBOR → 0xd8 0x18") {
    Head8_owned h;
    h.set_major_type(MT_TAG);
    h.set_additional(24);
    h.set_argument(24);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xd8, 0x18}));
  }

  SECTION("tag(32) URI → 0xd8 0x20") {
    Head8_owned h;
    h.set_major_type(MT_TAG);
    h.set_additional(24);
    h.set_argument(32);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 2>{0xd8, 0x20}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Floats (major type 7, additional 25/26/27)
//   Float arguments are encoded as IEEE 754 in network byte order.
//   We use memcpy to reinterpret the float bits as an integer argument.
//
//   IEEE 754 binary16 (half-precision): sign(1) + exponent(5) + mantissa(10)
// ===========================================================================

TEST_CASE("CBOR float16 encoding", "[cbor][appendix-a]") {
  SECTION("0.0 → 0xf9 0x0000") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25); // float16
    h.set_argument(0x0000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x00, 0x00}));
  }

  SECTION("-0.0 → 0xf9 0x8000") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x8000);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x80, 0x00}));
  }

  SECTION("1.0 → 0xf9 0x3c00") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x3c00);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x3c, 0x00}));
  }

  SECTION("1.5 → 0xf9 0x3e00") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x3e00);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x3e, 0x00}));
  }

  SECTION("65504.0 → 0xf9 0x7bff") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x7bff);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x7b, 0xff}));
  }

  SECTION("5.960464477539063e-8 (smallest subnormal) → 0xf9 0x0001") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x0001);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x00, 0x01}));
  }

  SECTION("+Infinity → 0xf9 0x7c00") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x7c00);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x7c, 0x00}));
  }

  SECTION("-Infinity → 0xf9 0xfc00") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0xfc00);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0xfc, 0x00}));
  }

  SECTION("NaN → 0xf9 0x7e00") {
    Head16_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(25);
    h.set_argument(0x7e00);
    CHECK(
        bytes_match(h.buffer(), std::array<std::uint8_t, 3>{0xf9, 0x7e, 0x00}));
  }

  SECTION("round-trip all special values") {
    for (std::uint16_t bits :
         {std::uint16_t{0x0000}, std::uint16_t{0x8000}, std::uint16_t{0x3c00},
          std::uint16_t{0x3e00}, std::uint16_t{0x7bff}, std::uint16_t{0x7c00},
          std::uint16_t{0xfc00}, std::uint16_t{0x7e00},
          std::uint16_t{0x0001}}) {
      Head16_owned h;
      h.set_major_type(MT_SIMPLE);
      h.set_additional(25);
      h.set_argument(bits);

      Head16_view view(h.buffer());
      CHECK(view.major_type() == MT_SIMPLE);
      CHECK(view.additional() == 25);
      CHECK(view.argument() == bits);
    }
  }
}

TEST_CASE("CBOR float32 encoding", "[cbor][appendix-a]") {
  SECTION("100000.0f → 0xfa 0x47c35000") {
    Head32_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(26); // float32
    float val = 100000.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xfa, 0x47, 0xc3, 0x50, 0x00}));
  }

  SECTION("3.4028234663852886e+38f → 0xfa 0x7f7fffff") {
    Head32_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(26);
    float val = 3.4028234663852886e+38f;
    std::uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xfa, 0x7f, 0x7f, 0xff, 0xff}));
  }

  SECTION("Infinity → 0xfa 0x7f800000") {
    Head32_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(26);
    float val = std::numeric_limits<float>::infinity();
    std::uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(
        h.buffer(), std::array<std::uint8_t, 5>{0xfa, 0x7f, 0x80, 0x00, 0x00}));
  }
}

TEST_CASE("CBOR float64 encoding", "[cbor][appendix-a]") {
  SECTION("1.1 → 0xfb 0x3ff199999999999a") {
    Head64_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(27); // float64
    double val = 1.1;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xfb, 0x3f, 0xf1, 0x99, 0x99,
                                                  0x99, 0x99, 0x99, 0x9a}));
  }

  SECTION("1.0e+300 → 0xfb 0x7e37e43c8800759c") {
    Head64_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(27);
    double val = 1.0e+300;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xfb, 0x7e, 0x37, 0xe4, 0x3c,
                                                  0x88, 0x00, 0x75, 0x9c}));
  }

  SECTION("-4.1 → 0xfb 0xc010666666666666") {
    Head64_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(27);
    double val = -4.1;
    std::uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    h.set_argument(bits);
    CHECK(bytes_match(h.buffer(),
                      std::array<std::uint8_t, 9>{0xfb, 0xc0, 0x10, 0x66, 0x66,
                                                  0x66, 0x66, 0x66, 0x66}));
  }
}

// ===========================================================================
// RFC 8949 Appendix A — Indefinite-length and break
// ===========================================================================

TEST_CASE("CBOR indefinite-length and break", "[cbor][appendix-a]") {
  SECTION("indefinite byte string → 0x5f") {
    Head0_owned h;
    h.set_major_type(MT_BSTR);
    h.set_additional(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x5f}));
  }

  SECTION("indefinite text string → 0x7f") {
    Head0_owned h;
    h.set_major_type(MT_TSTR);
    h.set_additional(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x7f}));
  }

  SECTION("indefinite array → 0x9f") {
    Head0_owned h;
    h.set_major_type(MT_ARRAY);
    h.set_additional(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0x9f}));
  }

  SECTION("indefinite map → 0xbf") {
    Head0_owned h;
    h.set_major_type(MT_MAP);
    h.set_additional(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xbf}));
  }

  SECTION("break → 0xff") {
    Head0_owned h;
    h.set_major_type(MT_SIMPLE);
    h.set_additional(31);
    CHECK(bytes_match(h.buffer(), std::array<std::uint8_t, 1>{0xff}));
  }
}

// ===========================================================================
// Round-trip: decode → encode for all head types
// ===========================================================================

TEST_CASE("CBOR Head0: exhaustive round-trip", "[cbor]") {
  for (unsigned mt = 0; mt < 8; ++mt) {
    for (unsigned ai = 0; ai < 24; ++ai) {
      Head0_owned h;
      h.set_major_type(static_cast<std::uint8_t>(mt));
      h.set_additional(static_cast<std::uint8_t>(ai));

      Head0_view view(h.buffer());
      CHECK(view.major_type() == mt);
      CHECK(view.additional() == ai);

      // Verify the byte matches manual construction
      auto expected = static_cast<std::uint8_t>((mt << 5) | ai);
      CHECK(static_cast<std::uint8_t>(h.buffer()[0]) == expected);
    }
  }
}

TEST_CASE("CBOR Head16: round-trip", "[cbor]") {
  Head16_owned h;
  h.set_major_type(MT_UINT);
  h.set_additional(25);
  h.set_argument(0xABCD);

  Head16_view view(h.buffer());
  CHECK(view.major_type() == MT_UINT);
  CHECK(view.additional() == 25);
  CHECK(view.argument() == 0xABCD);
}

TEST_CASE("CBOR Head32: round-trip", "[cbor]") {
  Head32_owned h;
  h.set_major_type(MT_UINT);
  h.set_additional(26);
  h.set_argument(0xDEADBEEF);

  Head32_view view(h.buffer());
  CHECK(view.major_type() == MT_UINT);
  CHECK(view.additional() == 26);
  CHECK(view.argument() == 0xDEADBEEF);
}

TEST_CASE("CBOR Head64: round-trip", "[cbor]") {
  Head64_owned h;
  h.set_major_type(MT_UINT);
  h.set_additional(27);
  h.set_argument(0x0123456789ABCDEFULL);

  Head64_view view(h.buffer());
  CHECK(view.major_type() == MT_UINT);
  CHECK(view.additional() == 27);
  CHECK(view.argument() == 0x0123456789ABCDEFULL);
}
