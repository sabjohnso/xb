#include <xb/wire/types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

// ---------------------------------------------------------------------------
// validation_level: enum values and ordering
// ---------------------------------------------------------------------------

TEST_CASE("validation_level has three levels", "[wire][types]") {
  using xb::wire::validation_level;

  // Verify they are distinct values
  CHECK(validation_level::discriminant != validation_level::structural);
  CHECK(validation_level::structural != validation_level::full);
  CHECK(validation_level::discriminant != validation_level::full);
}

TEST_CASE("validation_level ordering: discriminant < structural < full",
          "[wire][types]") {
  using xb::wire::validation_level;

  CHECK(validation_level::discriminant < validation_level::structural);
  CHECK(validation_level::structural < validation_level::full);
  CHECK(validation_level::discriminant < validation_level::full);
}

TEST_CASE("validation_level usable in if constexpr", "[wire][types]") {
  // Compile-time test: this must compile
  constexpr auto level = xb::wire::validation_level::discriminant;

  if constexpr (level >= xb::wire::validation_level::full) {
    FAIL("should not reach full validation branch");
  }

  if constexpr (level >= xb::wire::validation_level::structural) {
    FAIL("should not reach structural validation branch");
  }

  // Discriminant level: should reach here
  CHECK(true);
}

// ---------------------------------------------------------------------------
// const_buffer / mutable_buffer type aliases
// ---------------------------------------------------------------------------

TEST_CASE("const_buffer is span<const byte>", "[wire][types]") {
  static_assert(
      std::is_same_v<xb::wire::const_buffer, std::span<const std::byte>>);
}

TEST_CASE("mutable_buffer is span<byte>", "[wire][types]") {
  static_assert(std::is_same_v<xb::wire::mutable_buffer, std::span<std::byte>>);
}

TEST_CASE("const_buffer constructible from array", "[wire][types]") {
  std::array<std::byte, 4> arr = {};
  xb::wire::const_buffer buf{arr};
  CHECK(buf.size() == 4);
}

TEST_CASE("mutable_buffer constructible from array", "[wire][types]") {
  std::array<std::byte, 4> arr = {};
  xb::wire::mutable_buffer buf{arr};
  CHECK(buf.size() == 4);
}
