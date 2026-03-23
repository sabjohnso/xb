/// BSON (Binary JSON) encoding test — bsonspec.org.
///
/// Verifies that BES-generated types produce byte-exact BSON encoding
/// matching the specification.  BSON uses little-endian byte order.
///
/// Document layout:  int32 size | element* | 0x00
/// Element layout:   byte type | cstring name | value
///
/// This test constructs complete BSON documents by assembling heads
/// from BES-generated types with hand-written cstring names and
/// variable-length payloads, then verifies the result against known
/// BSON byte sequences.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

using namespace bson;

// ===========================================================================
// Helpers
// ===========================================================================

namespace {

  template <std::size_t N>
  bool
  bytes_match(std::span<const std::byte> buf,
              const std::array<std::uint8_t, N>& expected) {
    if (buf.size() < N) return false;
    for (std::size_t i = 0; i < N; ++i) {
      if (static_cast<std::uint8_t>(buf[i]) != expected[i]) return false;
    }
    return true;
  }

  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }

  void
  append_cstring(std::vector<std::byte>& buf, const std::string& s) {
    for (char c : s)
      buf.push_back(static_cast<std::byte>(c));
    buf.push_back(std::byte{0x00}); // null terminator
  }

  auto
  read_le32(std::span<const std::byte> buf, std::size_t offset)
      -> std::uint32_t {
    std::uint32_t v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
  }

  auto
  read_le64(std::span<const std::byte> buf, std::size_t offset)
      -> std::uint64_t {
    std::uint64_t v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
  }

  /// BSON type constants
  constexpr std::uint8_t BSON_DOUBLE = 0x01;
  constexpr std::uint8_t BSON_STRING = 0x02;
  constexpr std::uint8_t BSON_BINARY = 0x05;
  constexpr std::uint8_t BSON_UNDEFINED = 0x06;
  constexpr std::uint8_t BSON_OBJECTID = 0x07;
  constexpr std::uint8_t BSON_BOOLEAN = 0x08;
  constexpr std::uint8_t BSON_DATETIME = 0x09;
  constexpr std::uint8_t BSON_NULL = 0x0A;
  constexpr std::uint8_t BSON_INT32 = 0x10;
  constexpr std::uint8_t BSON_TIMESTAMP = 0x11;
  constexpr std::uint8_t BSON_INT64 = 0x12;
  constexpr std::uint8_t BSON_DECIMAL128 = 0x13;
  constexpr std::uint8_t BSON_MINKEY = 0xFF;
  constexpr std::uint8_t BSON_MAXKEY = 0x7F;

} // namespace

// ===========================================================================
// Wire sizes
// ===========================================================================

TEST_CASE("BSON wire sizes", "[bson]") {
  CHECK(DocumentHeader_owned::wire_size == 4);
  CHECK(ElementHeader_owned::wire_size == 1);
  CHECK(DoubleElement_owned::wire_size == 9);
  CHECK(StringHeader_owned::wire_size == 5);
  CHECK(BooleanElement_owned::wire_size == 2);
  CHECK(NullElement_owned::wire_size == 1);
  CHECK(Int32Element_owned::wire_size == 5);
  CHECK(Int64Element_owned::wire_size == 9);
  CHECK(DateTimeElement_owned::wire_size == 9);
  CHECK(TimestampElement_owned::wire_size == 9);
  CHECK(ObjectIdElement_owned::wire_size == 13);
  CHECK(BinaryHeader_owned::wire_size == 6);
  CHECK(Decimal128Element_owned::wire_size == 17);
}

// ===========================================================================
// Document header
// ===========================================================================

TEST_CASE("BSON document header: little-endian size", "[bson]") {
  DocumentHeader_owned hdr;
  hdr.set_size(5); // minimal empty document: 4 (size) + 1 (0x00) = 5

  auto buf = hdr.buffer();
  // Little-endian: 0x05 0x00 0x00 0x00
  CHECK(bytes_match(buf, std::array<std::uint8_t, 4>{0x05, 0x00, 0x00, 0x00}));

  DocumentHeader_view view(buf);
  CHECK(view.size() == 5);
}

// ===========================================================================
// Empty document: { }
//   Bytes: 05 00 00 00 00
// ===========================================================================

TEST_CASE("BSON empty document", "[bson]") {
  std::vector<std::byte> doc;

  DocumentHeader_owned hdr;
  hdr.set_size(5);
  append(doc, hdr.buffer());
  doc.push_back(std::byte{0x00}); // document terminator

  CHECK(doc.size() == 5);
  CHECK(bytes_match(doc,
                    std::array<std::uint8_t, 5>{0x05, 0x00, 0x00, 0x00, 0x00}));
}

// ===========================================================================
// Element round-trips
// ===========================================================================

TEST_CASE("BSON double element", "[bson]") {
  DoubleElement_owned elem;
  elem.set_type_id(BSON_DOUBLE);

  double val = 3.14;
  std::uint64_t bits;
  std::memcpy(&bits, &val, sizeof(bits));
  elem.set_value(bits);

  DoubleElement_view view(elem.buffer());
  CHECK(view.type_id() == BSON_DOUBLE);

  std::uint64_t out_bits = view.value();
  double out_val;
  std::memcpy(&out_val, &out_bits, sizeof(out_val));
  CHECK(out_val == 3.14);

  // Verify little-endian: first byte is type 0x01
  CHECK(static_cast<std::uint8_t>(elem.buffer()[0]) == 0x01);
  // Next 8 bytes are IEEE 754 double for 3.14 in little-endian
  CHECK(read_le64(elem.buffer(), 1) == bits);
}

TEST_CASE("BSON boolean element", "[bson]") {
  BooleanElement_owned elem;
  elem.set_type_id(BSON_BOOLEAN);

  SECTION("false → 0x08 0x00") {
    elem.set_value(0x00);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 2>{0x08, 0x00}));
  }

  SECTION("true → 0x08 0x01") {
    elem.set_value(0x01);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 2>{0x08, 0x01}));
  }
}

TEST_CASE("BSON int32 element", "[bson]") {
  Int32Element_owned elem;
  elem.set_type_id(BSON_INT32);

  SECTION("0 → little-endian") {
    elem.set_value(0);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 5>{
                                         0x10, 0x00, 0x00, 0x00, 0x00}));
  }

  SECTION("1 → 0x10 01 00 00 00") {
    elem.set_value(1);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 5>{
                                         0x10, 0x01, 0x00, 0x00, 0x00}));
  }

  SECTION("256 → 0x10 00 01 00 00") {
    elem.set_value(256);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 5>{
                                         0x10, 0x00, 0x01, 0x00, 0x00}));
  }

  SECTION("-1 → 0x10 ff ff ff ff") {
    std::int32_t sval = -1;
    std::uint32_t bits;
    std::memcpy(&bits, &sval, sizeof(bits));
    elem.set_value(bits);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 5>{
                                         0x10, 0xff, 0xff, 0xff, 0xff}));
  }

  SECTION("round-trip") {
    elem.set_value(123456);
    Int32Element_view view(elem.buffer());
    CHECK(view.type_id() == BSON_INT32);
    CHECK(view.value() == 123456);
  }
}

TEST_CASE("BSON int64 element", "[bson]") {
  Int64Element_owned elem;
  elem.set_type_id(BSON_INT64);
  elem.set_value(1000000000000ULL);

  Int64Element_view view(elem.buffer());
  CHECK(view.type_id() == BSON_INT64);
  CHECK(view.value() == 1000000000000ULL);

  // Little-endian check
  CHECK(static_cast<std::uint8_t>(elem.buffer()[0]) == 0x12);
  CHECK(read_le64(elem.buffer(), 1) == 1000000000000ULL);
}

TEST_CASE("BSON timestamp element", "[bson]") {
  TimestampElement_owned elem;
  elem.set_type_id(BSON_TIMESTAMP);
  elem.set_increment(42);
  elem.set_timestamp(1700000000);

  TimestampElement_view view(elem.buffer());
  CHECK(view.type_id() == BSON_TIMESTAMP);
  CHECK(view.increment() == 42);
  CHECK(view.timestamp() == 1700000000);

  // Verify layout: type(1) + increment(4 LE) + timestamp(4 LE)
  CHECK(static_cast<std::uint8_t>(elem.buffer()[0]) == 0x11);
  CHECK(read_le32(elem.buffer(), 1) == 42);
  CHECK(read_le32(elem.buffer(), 5) == 1700000000);
}

TEST_CASE("BSON datetime element", "[bson]") {
  DateTimeElement_owned elem;
  elem.set_type_id(BSON_DATETIME);
  elem.set_value(1700000000000ULL); // ms since epoch

  DateTimeElement_view view(elem.buffer());
  CHECK(view.type_id() == BSON_DATETIME);
  CHECK(view.value() == 1700000000000ULL);
}

TEST_CASE("BSON null/undefined/minkey/maxkey", "[bson]") {
  SECTION("null → 0x0A") {
    NullElement_owned elem;
    elem.set_type_id(BSON_NULL);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 1>{0x0A}));
  }

  SECTION("undefined → 0x06") {
    NullElement_owned elem;
    elem.set_type_id(BSON_UNDEFINED);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 1>{0x06}));
  }

  SECTION("MinKey → 0xFF") {
    NullElement_owned elem;
    elem.set_type_id(BSON_MINKEY);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 1>{0xFF}));
  }

  SECTION("MaxKey → 0x7F") {
    NullElement_owned elem;
    elem.set_type_id(BSON_MAXKEY);
    CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 1>{0x7F}));
  }
}

TEST_CASE("BSON objectid element", "[bson]") {
  ObjectIdElement_owned elem;
  elem.set_type_id(BSON_OBJECTID);

  std::array<std::byte, 12> oid = {
      std::byte{0x50}, std::byte{0x7f}, std::byte{0x1f}, std::byte{0x77},
      std::byte{0xbc}, std::byte{0xf8}, std::byte{0x6c}, std::byte{0xd7},
      std::byte{0x99}, std::byte{0x43}, std::byte{0x90}, std::byte{0x11}};
  elem.set_oid(oid);

  ObjectIdElement_view view(elem.buffer());
  CHECK(view.type_id() == BSON_OBJECTID);

  auto out_oid = view.oid();
  for (std::size_t i = 0; i < 12; ++i) {
    CHECK(out_oid[i] == oid[i]);
  }
}

TEST_CASE("BSON binary header", "[bson]") {
  BinaryHeader_owned elem;
  elem.set_type_id(BSON_BINARY);
  elem.set_length(100);
  elem.set_subtype(0x00); // generic

  BinaryHeader_view view(elem.buffer());
  CHECK(view.type_id() == BSON_BINARY);
  CHECK(view.length() == 100);
  CHECK(view.subtype() == 0x00);

  // Layout: 0x05 | 64 00 00 00 | 00
  CHECK(bytes_match(elem.buffer(), std::array<std::uint8_t, 6>{
                                       0x05, 0x64, 0x00, 0x00, 0x00, 0x00}));
}

TEST_CASE("BSON string header", "[bson]") {
  StringHeader_owned elem;
  elem.set_type_id(BSON_STRING);
  elem.set_length(6); // "hello" + null = 6 bytes

  StringHeader_view view(elem.buffer());
  CHECK(view.type_id() == BSON_STRING);
  CHECK(view.length() == 6);

  // Layout: 0x02 | 06 00 00 00
  CHECK(bytes_match(elem.buffer(),
                    std::array<std::uint8_t, 5>{0x02, 0x06, 0x00, 0x00, 0x00}));
}

// ===========================================================================
// Complete document assembly: {"x": 1}
//   Layout: size(4) + type(1) + "x\0"(2) + int32(4) + terminator(1) = 12
//   Expected BSON:
//     0c 00 00 00       (document size = 12)
//     10                (type: int32)
//     78 00             (cstring "x\0")
//     01 00 00 00       (int32 value = 1, little-endian)
//     00                (document terminator)
// ===========================================================================

TEST_CASE("BSON complete document: {\"x\": 1}", "[bson]") {
  std::vector<std::byte> doc;

  // Placeholder for document header (fill size later)
  DocumentHeader_owned hdr;
  append(doc, hdr.buffer());

  // Element: int32 "x" = 1
  doc.push_back(static_cast<std::byte>(BSON_INT32)); // type
  append_cstring(doc, "x");                          // key

  std::int32_t val = 1;
  std::uint32_t val_le;
  std::memcpy(&val_le, &val, sizeof(val_le));
  for (std::size_t i = 0; i < 4; ++i) {
    doc.push_back(static_cast<std::byte>((val_le >> (i * 8)) & 0xFF));
  }

  // Document terminator
  doc.push_back(std::byte{0x00});

  // Fill in the document size
  auto total = static_cast<std::uint32_t>(doc.size());
  std::memcpy(doc.data(), &total, sizeof(total));

  CHECK(doc.size() == 12);
  CHECK(bytes_match(doc, std::array<std::uint8_t, 12>{
                             0x0c, 0x00, 0x00, 0x00, // size = 12
                             0x10,                   // type = int32
                             0x78, 0x00,             // key = "x\0"
                             0x01, 0x00, 0x00, 0x00, // value = 1
                             0x00                    // terminator
                         }));

  // Verify we can read the header back
  DocumentHeader_view doc_view(doc);
  CHECK(doc_view.size() == 12);
}

// ===========================================================================
// Complete document: {"hello": "world"}
//   Expected BSON:
//     16 00 00 00       (document size = 22)
//     02                (type: string)
//     68 65 6c 6c 6f 00 (cstring "hello\0")
//     06 00 00 00       (string length = 6, includes trailing null)
//     77 6f 72 6c 64 00 (UTF-8 "world\0")
//     00                (document terminator)
// ===========================================================================

TEST_CASE("BSON complete document: {\"hello\": \"world\"}", "[bson]") {
  std::vector<std::byte> doc;

  DocumentHeader_owned hdr;
  append(doc, hdr.buffer());

  // String element
  doc.push_back(static_cast<std::byte>(BSON_STRING));
  append_cstring(doc, "hello"); // key

  // String value: length (including null) + data + null
  std::string val = "world";
  auto slen = static_cast<std::uint32_t>(val.size() + 1); // +1 for null
  for (std::size_t i = 0; i < 4; ++i) {
    doc.push_back(static_cast<std::byte>((slen >> (i * 8)) & 0xFF));
  }
  for (char c : val)
    doc.push_back(static_cast<std::byte>(c));
  doc.push_back(std::byte{0x00}); // string null terminator

  doc.push_back(std::byte{0x00}); // document terminator

  // Fill in size
  auto total = static_cast<std::uint32_t>(doc.size());
  std::memcpy(doc.data(), &total, sizeof(total));

  CHECK(doc.size() == 22);
  CHECK(
      bytes_match(doc, std::array<std::uint8_t, 22>{
                           0x16, 0x00, 0x00, 0x00,             // size = 22
                           0x02,                               // type = string
                           0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00, // key "hello\0"
                           0x06, 0x00, 0x00, 0x00, // string length = 6
                           0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00, // "world\0"
                           0x00                                // terminator
                       }));

  DocumentHeader_view doc_view(doc);
  CHECK(doc_view.size() == 22);
}

// ===========================================================================
// Complete document: {"a": true, "b": false, "c": null}
// ===========================================================================

TEST_CASE("BSON complete document: booleans and null", "[bson]") {
  std::vector<std::byte> doc;

  DocumentHeader_owned hdr;
  append(doc, hdr.buffer());

  // "a": true
  doc.push_back(static_cast<std::byte>(BSON_BOOLEAN));
  append_cstring(doc, "a");
  doc.push_back(std::byte{0x01});

  // "b": false
  doc.push_back(static_cast<std::byte>(BSON_BOOLEAN));
  append_cstring(doc, "b");
  doc.push_back(std::byte{0x00});

  // "c": null
  doc.push_back(static_cast<std::byte>(BSON_NULL));
  append_cstring(doc, "c");
  // null has no value payload

  doc.push_back(std::byte{0x00}); // terminator

  auto total = static_cast<std::uint32_t>(doc.size());
  std::memcpy(doc.data(), &total, sizeof(total));

  // size(4) + [0x08 "a\0" 0x01](4) + [0x08 "b\0" 0x00](4) +
  // [0x0A "c\0"](3) + terminator(1) = 16
  CHECK(doc.size() == 16);
  DocumentHeader_view doc_view(doc);
  CHECK(doc_view.size() == 16);

  // Verify element types at known offsets
  // offset 4: type "a", 5: 'a', 6: '\0', 7: true value
  CHECK(static_cast<std::uint8_t>(doc[4]) == BSON_BOOLEAN); // "a" type
  CHECK(static_cast<std::uint8_t>(doc[7]) == 0x01);         // true
  // offset 8: type "b", 9: 'b', 10: '\0', 11: false value
  CHECK(static_cast<std::uint8_t>(doc[8]) == BSON_BOOLEAN); // "b" type
  CHECK(static_cast<std::uint8_t>(doc[11]) == 0x00);        // false
  // offset 12: type "c", 13: 'c', 14: '\0' (no value for null)
  CHECK(static_cast<std::uint8_t>(doc[12]) == BSON_NULL); // "c" type
}

// ===========================================================================
// Complete document: {"pi": 3.14159}
// ===========================================================================

TEST_CASE("BSON complete document: {\"pi\": 3.14159}", "[bson]") {
  std::vector<std::byte> doc;

  DocumentHeader_owned hdr;
  append(doc, hdr.buffer());

  // Double element for "pi"
  doc.push_back(static_cast<std::byte>(BSON_DOUBLE));
  append_cstring(doc, "pi");

  double val = 3.14159;
  std::uint64_t bits;
  std::memcpy(&bits, &val, sizeof(bits));
  for (std::size_t i = 0; i < 8; ++i) {
    doc.push_back(static_cast<std::byte>((bits >> (i * 8)) & 0xFF));
  }

  doc.push_back(std::byte{0x00}); // terminator

  auto total = static_cast<std::uint32_t>(doc.size());
  std::memcpy(doc.data(), &total, sizeof(total));

  // size(4) + type(1) + "pi\0"(3) + double(8) + terminator(1) = 17
  CHECK(doc.size() == 17);
  DocumentHeader_view doc_view(doc);
  CHECK(doc_view.size() == 17);

  // Read back the double value from known offset
  // offset 4: type, 5: 'p', 6: 'i', 7: '\0', 8-15: double
  CHECK(read_le64(doc, 8) == bits);
}

// ===========================================================================
// Binary subtypes
// ===========================================================================

TEST_CASE("BSON binary subtypes", "[bson]") {
  SECTION("generic (0x00)") {
    BinaryHeader_owned h;
    h.set_type_id(BSON_BINARY);
    h.set_length(10);
    h.set_subtype(0x00);
    BinaryHeader_view view(h.buffer());
    CHECK(view.subtype() == 0x00);
  }

  SECTION("UUID (0x04)") {
    BinaryHeader_owned h;
    h.set_type_id(BSON_BINARY);
    h.set_length(16);
    h.set_subtype(0x04);
    BinaryHeader_view view(h.buffer());
    CHECK(view.subtype() == 0x04);
    CHECK(view.length() == 16);
  }

  SECTION("user-defined (0x80)") {
    BinaryHeader_owned h;
    h.set_type_id(BSON_BINARY);
    h.set_length(0);
    h.set_subtype(0x80);
    BinaryHeader_view view(h.buffer());
    CHECK(view.subtype() == 0x80);
  }
}

// ===========================================================================
// Decimal128 element
// ===========================================================================

TEST_CASE("BSON decimal128 element", "[bson]") {
  Decimal128Element_owned elem;
  elem.set_type_id(BSON_DECIMAL128);

  // Decimal128 for 1.0: low=0x0000000000000001, high=0x3040000000000000
  std::array<std::byte, 16> dec128 = {
      // little-endian: low word first
      std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x40}, std::byte{0x30}};
  elem.set_value(dec128);

  ObjectIdElement_view oid_check(elem.buffer()); // just to verify type
  CHECK(static_cast<std::uint8_t>(elem.buffer()[0]) == BSON_DECIMAL128);

  Decimal128Element_view view(elem.buffer());
  CHECK(view.type_id() == BSON_DECIMAL128);

  auto out = view.value();
  for (std::size_t i = 0; i < 16; ++i) {
    CHECK(out[i] == dec128[i]);
  }
}
