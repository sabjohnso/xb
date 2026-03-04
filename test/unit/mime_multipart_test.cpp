#include <xb/mime_multipart.hpp>

#include <catch2/catch_test_macros.hpp>

namespace mime = xb::mime;

// Helper: make bytes from string
static std::vector<std::byte>
to_bytes(const std::string& s) {
  std::vector<std::byte> result;
  for (char c : s) {
    result.push_back(static_cast<std::byte>(c));
  }
  return result;
}

// Helper: bytes to string
static std::string
to_string(const std::vector<std::byte>& bytes) {
  std::string result;
  for (auto b : bytes) {
    result.push_back(static_cast<char>(b));
  }
  return result;
}

// -- Type defaults ------------------------------------------------------------

TEST_CASE("mime: mime_part default construction", "[mime]") {
  mime::mime_part part;
  CHECK(part.content_type.empty());
  CHECK(part.content_id.empty());
  CHECK(part.content_transfer_encoding.empty());
  CHECK(part.body.empty());
}

TEST_CASE("mime: mime_part equality", "[mime]") {
  mime::mime_part a;
  a.content_type = "text/xml";
  a.content_id = "root";
  a.body = to_bytes("<data/>");

  mime::mime_part b = a;
  CHECK(a == b);

  b.content_type = "application/octet-stream";
  CHECK_FALSE(a == b);
}

TEST_CASE("mime: multipart_message default construction", "[mime]") {
  mime::multipart_message msg;
  CHECK(msg.boundary.empty());
  CHECK(msg.parts.empty());
}

TEST_CASE("mime: multipart_message equality", "[mime]") {
  mime::multipart_message a;
  a.boundary = "test-boundary";
  a.parts.push_back(
      mime::mime_part{"text/xml", "root", "8bit", to_bytes("<data/>")});

  mime::multipart_message b = a;
  CHECK(a == b);

  b.boundary = "other-boundary";
  CHECK_FALSE(a == b);
}

// -- generate_boundary --------------------------------------------------------

TEST_CASE("mime: generate_boundary is non-empty", "[mime]") {
  auto b = mime::generate_boundary();
  CHECK_FALSE(b.empty());
}

TEST_CASE("mime: generate_boundary differs on successive calls", "[mime]") {
  auto a = mime::generate_boundary();
  auto b = mime::generate_boundary();
  CHECK(a != b);
}

// -- serialize / parse --------------------------------------------------------

TEST_CASE("mime: serialize single text part", "[mime]") {
  mime::multipart_message msg;
  msg.boundary = "test-boundary";
  msg.parts.push_back(
      mime::mime_part{"text/xml", "root", "8bit", to_bytes("<data/>")});

  auto bytes = mime::serialize_multipart(msg);
  auto text = to_string(bytes);

  CHECK(text.find("--test-boundary") != std::string::npos);
  CHECK(text.find("Content-Type: text/xml") != std::string::npos);
  CHECK(text.find("Content-ID: <root>") != std::string::npos);
  CHECK(text.find("<data/>") != std::string::npos);
  CHECK(text.find("--test-boundary--") != std::string::npos);
}

TEST_CASE("mime: serialize multiple parts", "[mime]") {
  mime::multipart_message msg;
  msg.boundary = "myboundary";
  msg.parts.push_back(
      mime::mime_part{"text/xml", "part0", "8bit", to_bytes("<root/>")});
  msg.parts.push_back(mime::mime_part{"application/octet-stream", "part1",
                                      "binary", to_bytes("BINARYDATA")});

  auto bytes = mime::serialize_multipart(msg);
  auto text = to_string(bytes);

  // Count boundary occurrences (2 part boundaries + 1 closing)
  std::size_t count = 0;
  std::string needle = "--myboundary";
  auto pos = text.find(needle);
  while (pos != std::string::npos) {
    ++count;
    pos = text.find(needle, pos + 1);
  }
  CHECK(count == 3); // 2 part delimiters + 1 closing (which starts with --)
}

TEST_CASE("mime: round-trip serialize then parse", "[mime]") {
  mime::multipart_message original;
  original.boundary = "roundtrip-boundary";
  original.parts.push_back(
      mime::mime_part{"text/xml", "root", "8bit", to_bytes("<envelope/>")});
  original.parts.push_back(mime::mime_part{"application/octet-stream", "att1",
                                           "binary", to_bytes("BINARYDATA")});

  auto bytes = mime::serialize_multipart(original);
  auto parsed = mime::parse_multipart(bytes, original.boundary);

  REQUIRE(parsed.parts.size() == 2);

  CHECK(parsed.parts[0].content_type == "text/xml");
  CHECK(parsed.parts[0].content_id == "root");
  CHECK(parsed.parts[0].content_transfer_encoding == "8bit");
  CHECK(to_string(parsed.parts[0].body) == "<envelope/>");

  CHECK(parsed.parts[1].content_type == "application/octet-stream");
  CHECK(parsed.parts[1].content_id == "att1");
  CHECK(parsed.parts[1].content_transfer_encoding == "binary");
  CHECK(to_string(parsed.parts[1].body) == "BINARYDATA");
}

TEST_CASE("mime: binary content preservation", "[mime]") {
  // Create binary data with all byte values 0x00-0xFF
  std::vector<std::byte> binary_data;
  for (int i = 0; i < 256; ++i) {
    binary_data.push_back(static_cast<std::byte>(i));
  }

  mime::multipart_message msg;
  msg.boundary = "binary-test";
  msg.parts.push_back(mime::mime_part{"application/octet-stream", "bin",
                                      "binary", binary_data});

  auto bytes = mime::serialize_multipart(msg);
  auto parsed = mime::parse_multipart(bytes, msg.boundary);

  REQUIRE(parsed.parts.size() == 1);
  CHECK(parsed.parts[0].body == binary_data);
}

// -- extract_boundary ---------------------------------------------------------

TEST_CASE("mime: extract_boundary from Content-Type header", "[mime]") {
  auto boundary =
      mime::extract_boundary("multipart/related; boundary=\"abc123\"");
  CHECK(boundary == "abc123");
}

TEST_CASE("mime: extract_boundary unquoted", "[mime]") {
  auto boundary = mime::extract_boundary("multipart/related; boundary=abc123");
  CHECK(boundary == "abc123");
}

TEST_CASE("mime: extract_boundary missing", "[mime]") {
  auto boundary = mime::extract_boundary("text/xml");
  CHECK(boundary.empty());
}

// -- mtom_content_type --------------------------------------------------------

TEST_CASE("mime: mtom_content_type produces valid header", "[mime]") {
  auto ct = mime::mtom_content_type("myboundary", "root-part");

  CHECK(ct.find("multipart/related") != std::string::npos);
  CHECK(ct.find("application/xop+xml") != std::string::npos);
  CHECK(ct.find("boundary=\"myboundary\"") != std::string::npos);
  CHECK(ct.find("start=\"<root-part>\"") != std::string::npos);
}

// -- Content-ID angle bracket handling ----------------------------------------

TEST_CASE("mime: content-id with angle brackets round-trips", "[mime]") {
  mime::multipart_message msg;
  msg.boundary = "cid-test";
  msg.parts.push_back(
      mime::mime_part{"text/xml", "root@example.com", "8bit", to_bytes("hi")});

  auto bytes = mime::serialize_multipart(msg);
  auto text = to_string(bytes);

  // Serialized form should have angle brackets
  CHECK(text.find("Content-ID: <root@example.com>") != std::string::npos);

  // Parse should strip angle brackets
  auto parsed = mime::parse_multipart(bytes, msg.boundary);
  REQUIRE(parsed.parts.size() == 1);
  CHECK(parsed.parts[0].content_id == "root@example.com");
}
