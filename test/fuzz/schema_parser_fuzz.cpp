/// @file
/// libFuzzer harness for xb::schema_parser (XML Schema 1.1 / XSD).
///
/// Drives the full XSD reader: expat front end, then schema_parser
/// walking the parsed event stream.  XSD has the largest grammar
/// surface in xb (~1500 lines of parser logic), so this harness is
/// arguably the most valuable of the set.

#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string_view xml(reinterpret_cast<const char*>(data), size);
  try {
    xb::expat_reader reader(xml);
    xb::schema_parser parser;
    auto schema = parser.parse(reader);
    (void)schema;
  } catch (const std::exception&) {
    // Malformed XSD or expat hardening trip is not a bug.
  }
  return 0;
}
