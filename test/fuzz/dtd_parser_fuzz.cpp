/// @file
/// libFuzzer harness for xb::dtd_parser.
///
/// Drives the DTD lexer + parser with arbitrary byte sequences.  The
/// parser is plain text-in, AST-out; documented exceptions are caught
/// because parse failures are not bugs — only crashes, hangs, and
/// sanitiser violations are.

#include <xb/dtd_parser.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string source(reinterpret_cast<const char*>(data), size);
  try {
    xb::dtd_parser p;
    auto doc = p.parse(source);
    (void)doc;
  } catch (const std::exception&) {
    // Documented failure modes (malformed DTD, parameter-entity
    // depth exceeded, etc.) are not bugs.
  }
  return 0;
}
