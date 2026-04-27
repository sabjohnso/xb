/// @file
/// libFuzzer harness for xb::expat_reader.
///
/// Drives the hardened XML parser with arbitrary byte sequences.  The
/// harness catches xb's own @c std::runtime_error / @c std::length_error
/// (parse failures, billion-laughs cap, XXE refusal) because those are
/// the parser's documented failure modes — only crashes, hangs, and
/// sanitiser violations are interesting bugs.
///
/// Build:
///   cmake --preset clang-20 -Dxb_BUILD_FUZZERS=ON
///   cmake --build build-clang-20 --config Release \
///         --target xb_expat_reader_fuzz
///
/// Run (manually, with a corpus directory of seeds and a per-run budget):
///   mkdir -p test/fuzz/corpus/expat_reader
///   build-clang-20/test/fuzz/Release/xb_expat_reader_fuzz \
///       test/fuzz/corpus/expat_reader -max_total_time=300

#include <xb/expat_reader.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string_view xml(reinterpret_cast<const char*>(data), size);
  try {
    xb::expat_reader reader(xml);
    while (reader.read()) {
      // Drain the event stream so accessors that lazily compute
      // depth, line, column, namespace bindings are exercised too.
      (void)reader.node_type();
      switch (reader.node_type()) {
        case xb::xml_node_type::start_element:
        case xb::xml_node_type::end_element:
          (void)reader.name();
          for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
            (void)reader.attribute_name(i);
            (void)reader.attribute_value(i);
          }
          break;
        case xb::xml_node_type::characters:
          (void)reader.text();
          break;
      }
      (void)reader.depth();
      (void)reader.line();
      (void)reader.column();
    }
  } catch (const std::exception&) {
    // Documented failure modes (XXE refused, billion-laughs cap, malformed
    // XML, etc.) are not bugs.  Anything that escapes through std::abort
    // (the wire/bits.hpp bounds-check), SIGSEGV, ASan, or UBSan IS a bug
    // and is caught by libFuzzer or the sanitiser.
  }
  return 0;
}
