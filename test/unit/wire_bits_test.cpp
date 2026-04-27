#include <xb/wire/bits.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#ifdef __unix__
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Helper: create a span from a std::array
template <std::size_t N>
auto
as_span(const std::array<std::byte, N>& a) {
  return std::span<const std::byte>{a};
}

template <std::size_t N>
auto
as_mut_span(std::array<std::byte, N>& a) {
  return std::span<std::byte>{a};
}

// Convenience
constexpr std::byte
B(unsigned char v) {
  return std::byte{v};
}

// ---------------------------------------------------------------------------
// extract_bits: MSB-first (default)
// ---------------------------------------------------------------------------

TEST_CASE("extract_bits: full byte at offset 0", "[wire][bits]") {
  std::array<std::byte, 2> buf = {B(0xAB), B(0xCD)};
  CHECK(xb::wire::extract_bits<0, 8>(as_span(buf)) == 0xAB);
}

TEST_CASE("extract_bits: full byte at offset 8", "[wire][bits]") {
  std::array<std::byte, 2> buf = {B(0xAB), B(0xCD)};
  CHECK(xb::wire::extract_bits<8, 8>(as_span(buf)) == 0xCD);
}

TEST_CASE("extract_bits: high nibble of first byte", "[wire][bits]") {
  // 0xAB = 1010_1011, high nibble = 1010 = 0xA
  std::array<std::byte, 1> buf = {B(0xAB)};
  CHECK(xb::wire::extract_bits<0, 4>(as_span(buf)) == 0x0A);
}

TEST_CASE("extract_bits: low nibble of first byte", "[wire][bits]") {
  // 0xAB = 1010_1011, low nibble = 1011 = 0xB
  std::array<std::byte, 1> buf = {B(0xAB)};
  CHECK(xb::wire::extract_bits<4, 4>(as_span(buf)) == 0x0B);
}

TEST_CASE("extract_bits: single bit", "[wire][bits]") {
  // 0x80 = 1000_0000
  std::array<std::byte, 1> buf = {B(0x80)};
  CHECK(xb::wire::extract_bits<0, 1>(as_span(buf)) == 1);
  CHECK(xb::wire::extract_bits<1, 1>(as_span(buf)) == 0);
}

TEST_CASE("extract_bits: 3-bit field at offset 2", "[wire][bits]") {
  // 0b_0011_1000 = 0x38, bits [2..4] (MSB-first) = 111 = 7
  std::array<std::byte, 1> buf = {B(0x38)};
  CHECK(xb::wire::extract_bits<2, 3>(as_span(buf)) == 7);
}

TEST_CASE("extract_bits: field spanning byte boundary", "[wire][bits]") {
  // buf = [0x0F, 0xF0] = 0000_1111  1111_0000
  // bits [4..12) = 1111_1111 = 0xFF
  std::array<std::byte, 2> buf = {B(0x0F), B(0xF0)};
  CHECK(xb::wire::extract_bits<4, 8>(as_span(buf)) == 0xFF);
}

TEST_CASE("extract_bits: 12-bit field spanning two bytes", "[wire][bits]") {
  // buf = [0xAB, 0xCD] = 1010_1011  1100_1101
  // bits [0..12) = 1010_1011_1100 = 0xABC
  std::array<std::byte, 2> buf = {B(0xAB), B(0xCD)};
  CHECK(xb::wire::extract_bits<0, 12>(as_span(buf)) == 0xABC);
}

TEST_CASE("extract_bits: 16-bit field across two bytes", "[wire][bits]") {
  std::array<std::byte, 2> buf = {B(0xAB), B(0xCD)};
  CHECK(xb::wire::extract_bits<0, 16>(as_span(buf)) == 0xABCD);
}

// ---------------------------------------------------------------------------
// extract_bits: LSB-first
// ---------------------------------------------------------------------------

TEST_CASE("extract_bits LSB: full byte at offset 0", "[wire][bits]") {
  std::array<std::byte, 1> buf = {B(0xAB)};
  CHECK((xb::wire::extract_bits<0, 8, xb::wire::bit_order::lsb_first>(
            as_span(buf))) == 0xAB);
}

TEST_CASE("extract_bits LSB: low 4 bits of first byte", "[wire][bits]") {
  // 0xAB = 1010_1011, LSB-first bits [0..4) = low nibble = 1011 = 0xB
  std::array<std::byte, 1> buf = {B(0xAB)};
  CHECK((xb::wire::extract_bits<0, 4, xb::wire::bit_order::lsb_first>(
            as_span(buf))) == 0x0B);
}

TEST_CASE("extract_bits LSB: high 4 bits of first byte", "[wire][bits]") {
  // 0xAB = 1010_1011, LSB-first bits [4..8) = high nibble = 1010 = 0xA
  std::array<std::byte, 1> buf = {B(0xAB)};
  CHECK((xb::wire::extract_bits<4, 4, xb::wire::bit_order::lsb_first>(
            as_span(buf))) == 0x0A);
}

TEST_CASE("extract_bits LSB: single bit 0", "[wire][bits]") {
  // 0x01 = 0000_0001, bit 0 (LSB) = 1
  std::array<std::byte, 1> buf = {B(0x01)};
  CHECK((xb::wire::extract_bits<0, 1, xb::wire::bit_order::lsb_first>(
            as_span(buf))) == 1);
  CHECK((xb::wire::extract_bits<1, 1, xb::wire::bit_order::lsb_first>(
            as_span(buf))) == 0);
}

// ---------------------------------------------------------------------------
// insert_bits: MSB-first (default)
// ---------------------------------------------------------------------------

TEST_CASE("insert_bits: full byte at offset 0", "[wire][bits]") {
  std::array<std::byte, 2> buf = {B(0), B(0)};
  xb::wire::insert_bits<0, 8>(as_mut_span(buf), std::uint8_t{0xAB});
  CHECK(buf[0] == B(0xAB));
  CHECK(buf[1] == B(0x00));
}

TEST_CASE("insert_bits: high nibble without clobbering low", "[wire][bits]") {
  std::array<std::byte, 1> buf = {B(0x0F)}; // low nibble set
  xb::wire::insert_bits<0, 4>(as_mut_span(buf), std::uint8_t{0x0A});
  // high nibble = 0xA, low nibble preserved = 0xF
  CHECK(buf[0] == B(0xAF));
}

TEST_CASE("insert_bits: low nibble without clobbering high", "[wire][bits]") {
  std::array<std::byte, 1> buf = {B(0xF0)}; // high nibble set
  xb::wire::insert_bits<4, 4>(as_mut_span(buf), std::uint8_t{0x0B});
  CHECK(buf[0] == B(0xFB));
}

TEST_CASE("insert_bits: field spanning byte boundary", "[wire][bits]") {
  std::array<std::byte, 2> buf = {B(0), B(0)};
  xb::wire::insert_bits<4, 8>(as_mut_span(buf), std::uint8_t{0xFF});
  // bits [4..12) = 0xFF => buf = 0000_1111 1111_0000 = 0x0F, 0xF0
  CHECK(buf[0] == B(0x0F));
  CHECK(buf[1] == B(0xF0));
}

TEST_CASE("insert_bits: single bit", "[wire][bits]") {
  std::array<std::byte, 1> buf = {B(0)};
  xb::wire::insert_bits<0, 1>(as_mut_span(buf), std::uint8_t{1});
  CHECK(buf[0] == B(0x80));
}

// ---------------------------------------------------------------------------
// Round-trip property: extract(insert(buf, v)) == v
// ---------------------------------------------------------------------------

TEST_CASE("extract/insert round-trip: 4-bit field", "[wire][bits]") {
  for (unsigned v = 0; v < 16; ++v) {
    std::array<std::byte, 1> buf = {B(0)};
    xb::wire::insert_bits<2, 4>(as_mut_span(buf), static_cast<std::uint8_t>(v));
    CHECK(xb::wire::extract_bits<2, 4>(as_span(buf)) == v);
  }
}

TEST_CASE("extract/insert round-trip: 8-bit cross-boundary", "[wire][bits]") {
  for (unsigned v = 0; v < 256; v += 17) {
    std::array<std::byte, 2> buf = {B(0), B(0)};
    xb::wire::insert_bits<3, 8>(as_mut_span(buf), static_cast<std::uint8_t>(v));
    CHECK(xb::wire::extract_bits<3, 8>(as_span(buf)) == v);
  }
}

TEST_CASE("extract/insert round-trip: 12-bit field", "[wire][bits]") {
  for (unsigned v : {0u, 1u, 0xABCu, 0xFFFu}) {
    std::array<std::byte, 2> buf = {B(0), B(0)};
    xb::wire::insert_bits<0, 12>(as_mut_span(buf),
                                 static_cast<std::uint16_t>(v));
    CHECK(xb::wire::extract_bits<0, 12>(as_span(buf)) == v);
  }
}

// ---------------------------------------------------------------------------
// constexpr
// ---------------------------------------------------------------------------

TEST_CASE("extract_bits is constexpr", "[wire][bits]") {
  constexpr std::array<std::byte, 2> buf = {B(0xAB), B(0xCD)};
  constexpr auto val = xb::wire::extract_bits<0, 8>(std::span{buf});
  static_assert(val == 0xAB);
}

// ---------------------------------------------------------------------------
// Wide fields (> 32 bits) — insert_bits must not truncate to unsigned
// ---------------------------------------------------------------------------

TEST_CASE("insert_bits/extract_bits: 48-bit round-trip (MSB)", "[wire][bits]") {
  std::array<std::byte, 12> buf{};
  buf[11] = B(0xAA); // canary

  std::uint64_t value = 1234567890ULL;
  xb::wire::insert_bits<40, 48>(as_mut_span(buf), value);
  auto result = xb::wire::extract_bits<40, 48>(as_span(buf));

  CHECK(result == value);
  CHECK(buf[11] == B(0xAA)); // canary preserved
}

TEST_CASE("insert_bits/extract_bits: 48-bit large value (MSB)",
          "[wire][bits]") {
  std::array<std::byte, 8> buf{};

  std::uint64_t value = 281474976710655ULL; // 2^48 - 1
  xb::wire::insert_bits<0, 48>(as_mut_span(buf), value);
  auto result = xb::wire::extract_bits<0, 48>(as_span(buf));

  CHECK(result == value);
  // Bytes 6-7 should be untouched
  CHECK(buf[6] == B(0x00));
  CHECK(buf[7] == B(0x00));
}

TEST_CASE("insert_bits/extract_bits: 48-bit round-trip (LSB)", "[wire][bits]") {
  std::array<std::byte, 8> buf{};

  std::uint64_t value = 1234567890ULL;
  xb::wire::insert_bits<0, 48, xb::wire::bit_order::lsb_first>(as_mut_span(buf),
                                                               value);
  auto result = xb::wire::extract_bits<0, 48, xb::wire::bit_order::lsb_first>(
      as_span(buf));

  CHECK(result == value);
}

// ---------------------------------------------------------------------------
// Security: offset/width widened to size_t (no 32-bit overflow)
// ---------------------------------------------------------------------------

TEST_CASE("extract_bits offset+width does not overflow at codegen time",
          "[wire][bits][security]") {
  // Use a moderately large offset that would still be representable as
  // unsigned but easily exceeds the 4 GiB threshold a bit-level field
  // could hit on a multi-MiB message frame. Verify the templates
  // accept size_t-class offsets.
  constexpr std::size_t kOffset = std::size_t{1} << 20; // 1 Mi bits
  constexpr std::size_t kBufBytes = (kOffset / 8) + 1;
  std::vector<std::byte> big(kBufBytes, B(0));
  big[kOffset / 8] = B(0xAB);
  std::span<const std::byte> sp{big};
  CHECK(xb::wire::extract_bits<kOffset, 8>(sp) == 0xAB);
}

// ---------------------------------------------------------------------------
// Security: defensive bounds check on undersized buffers
// ---------------------------------------------------------------------------

#ifdef __unix__
TEST_CASE("extract_bits aborts on undersized buffer",
          "[wire][bits][security]") {
  // The bounds check in detail::extract_msb refuses to read past the
  // span; rather than emit UB, it aborts the program. Verified via a
  // forked child so the parent process survives.
  auto pid = ::fork();
  REQUIRE(pid >= 0);
  if (pid == 0) {
    // Child process: ask for 16 bits from a 1-byte buffer. This must
    // not return — the bounds check aborts instead of reading past
    // the end.
    std::array<std::byte, 1> buf = {B(0xAB)};
    auto v = xb::wire::extract_bits<0, 16>(std::span<const std::byte>{buf});
    // Use the value to prevent dead-code elimination, then exit with
    // a sentinel so the parent can distinguish "fell through" from
    // "aborted".
    ::_exit(v == 0 ? 99 : 100);
  }
  int status = 0;
  ::waitpid(pid, &status, 0);
  CHECK(WIFSIGNALED(status));
  if (WIFSIGNALED(status)) { CHECK(WTERMSIG(status) == SIGABRT); }
}
#endif
