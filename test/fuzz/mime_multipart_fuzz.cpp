/// @file
/// libFuzzer harness for xb::mime::parse_multipart, exercising the
/// XOP / cid: resolution path on top.
///
/// Strategy:
///  1. Treat the fuzzer's first byte as a pseudo-length for a small
///     boundary string drawn from the next bytes; the rest is the
///     multipart body.  This lets libFuzzer mutate the boundary
///     string and the body independently.  An empty boundary falls
///     back to a fixed default so xb's parser still gets exercised.
///  2. parse_multipart() is the primary target.
///  3. On success, we feed the parsed multipart_message through
///     xop::from_multipart() to construct an mtom_message, then
///     xop::deoptimize() to walk every <xop:Include cid:...>
///     reference.  This exercises the case-(in)sensitive Content-ID
///     match and base64 encoding paths uncovered by the
///     SecurityReview.

#include <xb/mime_multipart.hpp>
#include <xb/xop.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

  /// Pull a small boundary string out of the head of @p data.  The
  /// first byte names a length in [0, 32]; the next L bytes are the
  /// boundary, ASCII-filtered.  Returns the boundary plus the offset
  /// at which the body begins.
  std::pair<std::string, std::size_t>
  extract_boundary(const std::uint8_t* data, std::size_t size) {
    if (size == 0) return {"FUZZ-DEFAULT-BOUNDARY", 0};
    const std::size_t boundary_len =
        std::min<std::size_t>(data[0] & 0x1f, size - 1);
    std::string boundary;
    boundary.reserve(boundary_len);
    for (std::size_t i = 0; i < boundary_len; ++i) {
      auto c = data[1 + i];
      // Restrict to bytes RFC 2046 actually permits in a boundary —
      // a fuzzer that explores arbitrary bytes here adds noise without
      // testing realistic shapes.
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
        boundary.push_back(static_cast<char>(c));
      }
    }
    if (boundary.empty()) boundary = "FUZZ-DEFAULT-BOUNDARY";
    return {std::move(boundary), 1 + boundary_len};
  }

} // namespace

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  auto [boundary, body_offset] = extract_boundary(data, size);
  std::vector<std::byte> body;
  if (body_offset < size) {
    body.reserve(size - body_offset);
    for (std::size_t i = body_offset; i < size; ++i) {
      body.push_back(static_cast<std::byte>(data[i]));
    }
  }

  try {
    auto mp = xb::mime::parse_multipart(body, boundary);
    // Exercise XOP cid: resolution.  from_multipart constructs an
    // mtom_message by reading the first part as the SOAP envelope
    // and treating subsequent parts as attachments; deoptimize then
    // walks the envelope replacing every <xop:Include cid:...> with
    // base64'd attachment bytes.  Each step is a target in its own
    // right.
    try {
      auto mtom = xb::xop::from_multipart(mp);
      auto env = xb::xop::deoptimize(mtom);
      (void)env;
    } catch (const std::exception&) {
      // A multipart that is not a valid MTOM message is not a bug.
    }
  } catch (const std::exception&) {
    // Malformed MIME is not a bug.
  }
  return 0;
}
