#pragma once

#include <bit>
#include <type_traits>

namespace xb::wire {

  // --- byteswap: constexpr byte-order reversal for integral types ----------
  //
  // Portable shift-and-mask implementation. Compilers recognize this pattern
  // and emit a single bswap instruction at -O1 and above (GCC, Clang, MSVC).

  template <typename T>
    requires std::is_integral_v<T>
  constexpr T
  byteswap(T value) noexcept {
    if constexpr (sizeof(T) == 1) {
      return value;
    } else {
      using U = std::make_unsigned_t<T>;
      auto u = static_cast<U>(value);

      if constexpr (sizeof(T) == 2) {
        u = static_cast<U>((u >> 8) | (u << 8));
      } else if constexpr (sizeof(T) == 4) {
        u = ((u >> 24) & U{0xFF}) | ((u >> 8) & U{0xFF00}) |
            ((u << 8) & U{0xFF0000}) | ((u << 24) & U{0xFF000000});
      } else if constexpr (sizeof(T) == 8) {
        u = ((u >> 56) & U{0xFF}) | ((u >> 40) & U{0xFF00}) |
            ((u >> 24) & U{0xFF0000}) | ((u >> 8) & U{0xFF000000}) |
            ((u << 8) & U{0xFF00000000}) | ((u << 24) & U{0xFF0000000000}) |
            ((u << 40) & U{0xFF000000000000}) |
            ((u << 56) & U{0xFF00000000000000});
      }

      return static_cast<T>(u);
    }
  }

  // --- to_wire / from_wire: endianness conversion --------------------------
  //
  // No-op when wire order matches native order (zero instructions).
  // Calls byteswap when conversion is needed.

  template <std::endian Order, typename T>
    requires std::is_integral_v<T>
  constexpr T
  to_wire(T value) noexcept {
    if constexpr (Order == std::endian::native) {
      return value;
    } else {
      return byteswap(value);
    }
  }

  template <std::endian Order, typename T>
    requires std::is_integral_v<T>
  constexpr T
  from_wire(T value) noexcept {
    if constexpr (Order == std::endian::native) {
      return value;
    } else {
      return byteswap(value);
    }
  }

} // namespace xb::wire
