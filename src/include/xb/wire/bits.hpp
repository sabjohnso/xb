#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace xb::wire {

  enum class bit_order { msb_first, lsb_first };

  // --- extract_bits: read a sub-byte field from a buffer -------------------
  //
  // Offset and Width are in bits. The return type is the smallest unsigned
  // integer that can hold Width bits.
  //
  // MSB-first (default): bit 0 is the most significant bit of byte 0.
  //   Byte:  [  byte 0  ] [  byte 1  ]
  //   Bits:   0 1 2 3 4 5  6 7 8 9 ...
  //
  // LSB-first: bit 0 is the least significant bit of byte 0.
  //   Byte:  [  byte 0  ] [  byte 1  ]
  //   Bits:   7 6 5 4 3 2  1 0  15 14 ...

  namespace detail {

    template <unsigned Width>
    using uint_for_width_t = std::conditional_t<
        (Width <= 8), std::uint8_t,
        std::conditional_t<
            (Width <= 16), std::uint16_t,
            std::conditional_t<(Width <= 32), std::uint32_t, std::uint64_t>>>;

    template <unsigned Offset, unsigned Width>
    constexpr auto
    extract_msb(std::span<const std::byte> buf) noexcept
        -> uint_for_width_t<Width> {
      using R = uint_for_width_t<Width>;
      R result = 0;

      constexpr unsigned start_byte = Offset / 8;
      constexpr unsigned start_bit = Offset % 8;

      // Accumulate bits from each byte that overlaps the field
      constexpr unsigned end_bit = Offset + Width;
      constexpr unsigned end_byte = (end_bit + 7) / 8;

      unsigned bits_remaining = Width;
      for (unsigned i = start_byte; i < end_byte; ++i) {
        auto byte_val = static_cast<unsigned>(buf[i]);

        unsigned lo = (i == start_byte) ? start_bit : 0;
        unsigned hi =
            (i + 1 == end_byte && end_bit % 8 != 0) ? (end_bit % 8) : 8;
        unsigned chunk_width = hi - lo;
        unsigned chunk = (byte_val >> (8 - hi)) & ((1u << chunk_width) - 1u);

        bits_remaining -= chunk_width;
        result =
            static_cast<R>(result | (static_cast<R>(chunk) << bits_remaining));
      }

      return result;
    }

    template <unsigned Offset, unsigned Width>
    constexpr auto
    extract_lsb(std::span<const std::byte> buf) noexcept
        -> uint_for_width_t<Width> {
      using R = uint_for_width_t<Width>;
      R result = 0;

      constexpr unsigned start_byte = Offset / 8;
      constexpr unsigned start_bit = Offset % 8;
      constexpr unsigned end_bit = Offset + Width;
      constexpr unsigned end_byte = (end_bit + 7) / 8;

      unsigned bits_collected = 0;
      for (unsigned i = start_byte; i < end_byte; ++i) {
        auto byte_val = static_cast<unsigned>(buf[i]);

        unsigned lo = (i == start_byte) ? start_bit : 0;
        unsigned hi =
            (i + 1 == end_byte && end_bit % 8 != 0) ? (end_bit % 8) : 8;
        unsigned chunk_width = hi - lo;
        unsigned chunk = (byte_val >> lo) & ((1u << chunk_width) - 1u);

        result =
            static_cast<R>(result | (static_cast<R>(chunk) << bits_collected));
        bits_collected += chunk_width;
      }

      return result;
    }

    template <unsigned Offset, unsigned Width>
    constexpr void
    insert_msb(std::span<std::byte> buf,
               uint_for_width_t<Width> value) noexcept {
      constexpr unsigned start_byte = Offset / 8;
      constexpr unsigned start_bit = Offset % 8;
      constexpr unsigned end_bit = Offset + Width;
      constexpr unsigned end_byte = (end_bit + 7) / 8;

      unsigned bits_remaining = Width;
      for (unsigned i = start_byte; i < end_byte; ++i) {
        unsigned lo = (i == start_byte) ? start_bit : 0;
        unsigned hi =
            (i + 1 == end_byte && end_bit % 8 != 0) ? (end_bit % 8) : 8;
        unsigned chunk_width = hi - lo;
        bits_remaining -= chunk_width;

        unsigned chunk = (static_cast<unsigned>(value) >> bits_remaining) &
                         ((1u << chunk_width) - 1u);
        unsigned mask = ((1u << chunk_width) - 1u) << (8 - hi);
        auto byte_val = static_cast<unsigned>(buf[i]);
        byte_val = (byte_val & ~mask) | (chunk << (8 - hi));
        buf[i] = static_cast<std::byte>(byte_val);
      }
    }

    template <unsigned Offset, unsigned Width>
    constexpr void
    insert_lsb(std::span<std::byte> buf,
               uint_for_width_t<Width> value) noexcept {
      constexpr unsigned start_byte = Offset / 8;
      constexpr unsigned start_bit = Offset % 8;
      constexpr unsigned end_bit = Offset + Width;
      constexpr unsigned end_byte = (end_bit + 7) / 8;

      unsigned bits_consumed = 0;
      for (unsigned i = start_byte; i < end_byte; ++i) {
        unsigned lo = (i == start_byte) ? start_bit : 0;
        unsigned hi =
            (i + 1 == end_byte && end_bit % 8 != 0) ? (end_bit % 8) : 8;
        unsigned chunk_width = hi - lo;

        unsigned chunk = (static_cast<unsigned>(value) >> bits_consumed) &
                         ((1u << chunk_width) - 1u);
        unsigned mask = ((1u << chunk_width) - 1u) << lo;
        auto byte_val = static_cast<unsigned>(buf[i]);
        byte_val = (byte_val & ~mask) | (chunk << lo);
        buf[i] = static_cast<std::byte>(byte_val);
        bits_consumed += chunk_width;
      }
    }

  } // namespace detail

  template <unsigned Offset, unsigned Width,
            bit_order Order = bit_order::msb_first>
  constexpr auto
  extract_bits(std::span<const std::byte> buf) noexcept
      -> detail::uint_for_width_t<Width> {
    if constexpr (Order == bit_order::msb_first) {
      return detail::extract_msb<Offset, Width>(buf);
    } else {
      return detail::extract_lsb<Offset, Width>(buf);
    }
  }

  template <unsigned Offset, unsigned Width,
            bit_order Order = bit_order::msb_first>
  constexpr void
  insert_bits(std::span<std::byte> buf,
              detail::uint_for_width_t<Width> value) noexcept {
    if constexpr (Order == bit_order::msb_first) {
      detail::insert_msb<Offset, Width>(buf, value);
    } else {
      detail::insert_lsb<Offset, Width>(buf, value);
    }
  }

} // namespace xb::wire
