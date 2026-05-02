/// @file
/// libFuzzer harness for xb::rng_xml_parser (RELAX NG XML Syntax).
///
/// The XML form of RELAX NG goes through xb::expat_reader first.
/// This harness exercises both the front-end XML parsing AND the
/// rng_xml_parser pattern walker, so coverage spans both layers.

#include <xb/expat_reader.hpp>
#include <xb/rng_parser.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string_view xml(reinterpret_cast<const char*>(data), size);
  try {
    xb::expat_reader reader(xml);
    xb::rng_xml_parser parser;
    auto pattern = parser.parse(reader);
    (void)pattern;
  } catch (const std::exception&) {
    // Malformed XML, expat hardening rejecting external entities,
    // RNG schema-shape violations — all documented and not bugs.
  }
  return 0;
}
