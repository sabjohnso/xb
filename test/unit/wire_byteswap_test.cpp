#include <xb/wire/byteswap.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>

// ---------------------------------------------------------------------------
// byteswap: basic identity and known values
// ---------------------------------------------------------------------------

TEST_CASE("byteswap uint8_t is identity", "[wire][byteswap]") {
  CHECK(xb::wire::byteswap(std::uint8_t{0x00}) == 0x00);
  CHECK(xb::wire::byteswap(std::uint8_t{0xFF}) == 0xFF);
  CHECK(xb::wire::byteswap(std::uint8_t{0x42}) == 0x42);
}

TEST_CASE("byteswap uint16_t reverses bytes", "[wire][byteswap]") {
  CHECK(xb::wire::byteswap(std::uint16_t{0x0102}) == 0x0201);
  CHECK(xb::wire::byteswap(std::uint16_t{0xFF00}) == 0x00FF);
  CHECK(xb::wire::byteswap(std::uint16_t{0x0000}) == 0x0000);
  CHECK(xb::wire::byteswap(std::uint16_t{0xFFFF}) == 0xFFFF);
}

TEST_CASE("byteswap uint32_t reverses bytes", "[wire][byteswap]") {
  CHECK(xb::wire::byteswap(std::uint32_t{0x01020304}) == 0x04030201);
  CHECK(xb::wire::byteswap(std::uint32_t{0xFF000000}) == 0x000000FF);
  CHECK(xb::wire::byteswap(std::uint32_t{0x00000000}) == 0x00000000);
  CHECK(xb::wire::byteswap(std::uint32_t{0xFFFFFFFF}) == 0xFFFFFFFF);
}

TEST_CASE("byteswap uint64_t reverses bytes", "[wire][byteswap]") {
  CHECK(xb::wire::byteswap(std::uint64_t{0x0102030405060708ULL}) ==
        0x0807060504030201ULL);
  CHECK(xb::wire::byteswap(std::uint64_t{0xFF00000000000000ULL}) ==
        0x00000000000000FFULL);
  CHECK(xb::wire::byteswap(std::uint64_t{0}) == 0);
  CHECK(xb::wire::byteswap(std::uint64_t{0xFFFFFFFFFFFFFFFFULL}) ==
        0xFFFFFFFFFFFFFFFFULL);
}

TEST_CASE("byteswap signed types", "[wire][byteswap]") {
  CHECK(xb::wire::byteswap(std::int16_t{0x0102}) == std::int16_t(0x0201));
  CHECK(xb::wire::byteswap(std::int32_t{0x01020304}) ==
        std::int32_t(0x04030201));
}

// ---------------------------------------------------------------------------
// byteswap: involution property (swap twice == identity)
// ---------------------------------------------------------------------------

TEST_CASE("byteswap is an involution for uint16_t", "[wire][byteswap]") {
  for (std::uint16_t v :
       {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x8000},
        std::uint16_t{0xBEEF}, std::numeric_limits<std::uint16_t>::max()}) {
    CHECK(xb::wire::byteswap(xb::wire::byteswap(v)) == v);
  }
}

TEST_CASE("byteswap is an involution for uint32_t", "[wire][byteswap]") {
  for (std::uint32_t v :
       {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{0xDEADBEEF},
        std::numeric_limits<std::uint32_t>::max()}) {
    CHECK(xb::wire::byteswap(xb::wire::byteswap(v)) == v);
  }
}

TEST_CASE("byteswap is an involution for uint64_t", "[wire][byteswap]") {
  for (std::uint64_t v : {std::uint64_t{0}, std::uint64_t{1},
                          std::uint64_t{0xDEADBEEFCAFEBABEULL},
                          std::numeric_limits<std::uint64_t>::max()}) {
    CHECK(xb::wire::byteswap(xb::wire::byteswap(v)) == v);
  }
}

// ---------------------------------------------------------------------------
// byteswap: constexpr
// ---------------------------------------------------------------------------

TEST_CASE("byteswap is constexpr", "[wire][byteswap]") {
  constexpr auto v16 = xb::wire::byteswap(std::uint16_t{0x0102});
  static_assert(v16 == 0x0201);

  constexpr auto v32 = xb::wire::byteswap(std::uint32_t{0x01020304});
  static_assert(v32 == 0x04030201);

  constexpr auto v64 = xb::wire::byteswap(std::uint64_t{0x0102030405060708ULL});
  static_assert(v64 == 0x0807060504030201ULL);
}

// ---------------------------------------------------------------------------
// to_wire / from_wire: no-op when order matches native
// ---------------------------------------------------------------------------

TEST_CASE("to_wire is no-op when wire order matches native",
          "[wire][byteswap]") {
  constexpr auto native = std::endian::native;
  std::uint32_t v = 0x01020304;
  CHECK(xb::wire::to_wire<native>(v) == v);
  CHECK(xb::wire::from_wire<native>(v) == v);
}

TEST_CASE("to_wire swaps when wire order differs from native",
          "[wire][byteswap]") {
  constexpr auto other = (std::endian::native == std::endian::little)
                             ? std::endian::big
                             : std::endian::little;
  std::uint32_t v = 0x01020304;
  CHECK(xb::wire::to_wire<other>(v) == xb::wire::byteswap(v));
  CHECK(xb::wire::from_wire<other>(v) == xb::wire::byteswap(v));
}

// ---------------------------------------------------------------------------
// to_wire / from_wire: round-trip property
// ---------------------------------------------------------------------------

TEST_CASE("from_wire(to_wire(x)) == x for big-endian", "[wire][byteswap]") {
  std::uint32_t v = 0xDEADBEEF;
  CHECK(xb::wire::from_wire<std::endian::big>(
            xb::wire::to_wire<std::endian::big>(v)) == v);
}

TEST_CASE("from_wire(to_wire(x)) == x for little-endian", "[wire][byteswap]") {
  std::uint32_t v = 0xDEADBEEF;
  CHECK(xb::wire::from_wire<std::endian::little>(
            xb::wire::to_wire<std::endian::little>(v)) == v);
}

// ---------------------------------------------------------------------------
// to_wire / from_wire: constexpr
// ---------------------------------------------------------------------------

TEST_CASE("to_wire and from_wire are constexpr", "[wire][byteswap]") {
  constexpr std::uint32_t v = 0x01020304;
  constexpr auto wired = xb::wire::to_wire<std::endian::big>(v);
  constexpr auto back = xb::wire::from_wire<std::endian::big>(wired);
  static_assert(back == v);
}
