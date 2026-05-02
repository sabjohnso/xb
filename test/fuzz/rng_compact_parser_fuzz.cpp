/// @file
/// libFuzzer harness for xb::rng_compact_parser (RELAX NG Compact
/// Syntax).  The compact parser has its own lexer (no XML front
/// end), so the harness drives it with raw bytes directly.

#include <xb/rng_compact_parser.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string source(reinterpret_cast<const char*>(data), size);
  try {
    xb::rng_compact_parser p;
    auto pattern = p.parse(source);
    (void)pattern;
  } catch (const std::exception&) {
    // Malformed RNC is not a bug.
  }
  return 0;
}
