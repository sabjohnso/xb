/// Fuchsia Trace Format (FXT) binary encoding test.
///
/// FXT uses 64-bit words, little-endian byte order, LSB-first bit packing.
/// This test verifies that BES-generated types produce the correct binary
/// layout matching the FXT specification.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

using namespace fxt;

namespace {

  /// Read a little-endian uint64_t from a byte span at the given offset.
  auto
  read_le64(std::span<const std::byte> buf, std::size_t offset)
      -> std::uint64_t {
    std::uint64_t v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
  }

} // namespace

// ===========================================================================
// RecordHeader — basic bit-field packing
// ===========================================================================

TEST_CASE("FXT RecordHeader: bit layout matches spec", "[fxt]") {
  RecordHeader_owned hdr;
  hdr.set_record_type(1);      // [0..3]  = 0x1
  hdr.set_record_size(2);      // [4..15] = 0x002
  hdr.set_type_specific(0);    // [16..31]
  hdr.set_type_specific_hi(0); // [32..63]

  auto buf = hdr.buffer();
  auto word = read_le64(buf, 0);

  // In LSB-first little-endian layout:
  // bits [0..3]  = record_type = 1
  // bits [4..15] = record_size = 2
  CHECK((word & 0xF) == 1);
  CHECK(((word >> 4) & 0xFFF) == 2);
}

TEST_CASE("FXT RecordHeader: round-trip", "[fxt]") {
  RecordHeader_owned hdr;
  hdr.set_record_type(0xA);
  hdr.set_record_size(0x123);
  hdr.set_type_specific(0xBEEF);
  hdr.set_type_specific_hi(0xDEADCAFE);

  RecordHeader_view view(hdr.buffer());
  CHECK(view.record_type() == 0xA);
  CHECK(view.record_size() == 0x123);
  CHECK(view.type_specific() == 0xBEEF);
  CHECK(view.type_specific_hi() == 0xDEADCAFE);
}

// ===========================================================================
// InitRecord — initialization record (type 1)
// ===========================================================================

TEST_CASE("FXT InitRecord: bit layout", "[fxt]") {
  InitRecord_owned init;
  init.set_record_type(1);
  init.set_record_size(2);
  init.set_reserved(0);
  init.set_ticks_per_second(1'000'000'000ULL); // 1 GHz

  auto buf = init.buffer();
  auto word0 = read_le64(buf, 0);
  auto word1 = read_le64(buf, 8);

  CHECK((word0 & 0xF) == 1);          // type
  CHECK(((word0 >> 4) & 0xFFF) == 2); // size
  CHECK(word1 == 1'000'000'000ULL);   // ticks_per_second
}

TEST_CASE("FXT InitRecord: round-trip", "[fxt]") {
  InitRecord_owned init;
  init.set_record_type(1);
  init.set_record_size(2);
  init.set_reserved(0);
  init.set_ticks_per_second(1'000'000'000ULL);

  InitRecord_view view(init.buffer());
  CHECK(view.record_type() == 1);
  CHECK(view.record_size() == 2);
  CHECK(view.ticks_per_second() == 1'000'000'000ULL);
}

// ===========================================================================
// StringRecord — string registration (type 2)
// ===========================================================================

TEST_CASE("FXT StringRecord: bit layout", "[fxt]") {
  StringRecord_owned rec;
  rec.set_record_type(2);
  rec.set_record_size(3);
  rec.set_string_index(42);
  rec.set_reserved1(0);
  rec.set_string_length(11);
  rec.set_reserved2(0);

  auto buf = rec.buffer();
  auto word = read_le64(buf, 0);

  CHECK((word & 0xF) == 2);             // type
  CHECK(((word >> 4) & 0xFFF) == 3);    // size
  CHECK(((word >> 16) & 0x7FFF) == 42); // string_index [16..30]
  CHECK(((word >> 31) & 0x1) == 0);     // reserved1 [31]
  CHECK(((word >> 32) & 0x7FFF) == 11); // string_length [32..46]
}

TEST_CASE("FXT StringRecord: round-trip", "[fxt]") {
  StringRecord_owned rec;
  rec.set_record_type(2);
  rec.set_record_size(3);
  rec.set_string_index(1000);
  rec.set_reserved1(0);
  rec.set_string_length(255);
  rec.set_reserved2(0);

  StringRecord_view view(rec.buffer());
  CHECK(view.record_type() == 2);
  CHECK(view.record_size() == 3);
  CHECK(view.string_index() == 1000);
  CHECK(view.string_length() == 255);
}

// ===========================================================================
// ThreadRecord — thread registration (type 3)
// ===========================================================================

TEST_CASE("FXT ThreadRecord: bit layout", "[fxt]") {
  ThreadRecord_owned rec;
  rec.set_record_type(3);
  rec.set_record_size(3);
  rec.set_thread_index(7);
  rec.set_reserved(0);
  rec.set_process_koid(1234);
  rec.set_thread_koid(5678);

  auto buf = rec.buffer();
  auto word0 = read_le64(buf, 0);
  auto word1 = read_le64(buf, 8);
  auto word2 = read_le64(buf, 16);

  CHECK((word0 & 0xF) == 3);          // type
  CHECK(((word0 >> 4) & 0xFFF) == 3); // size
  CHECK(((word0 >> 16) & 0xFF) == 7); // thread_index [16..23]
  CHECK(word1 == 1234);               // process_koid
  CHECK(word2 == 5678);               // thread_koid
}

// ===========================================================================
// EventRecord — timestamped event (type 4)
// ===========================================================================

TEST_CASE("FXT EventRecord: bit layout", "[fxt]") {
  EventRecord_owned rec;
  rec.set_record_type(4);
  rec.set_record_size(3);
  rec.set_event_type(0); // instant event
  rec.set_argument_count(0);
  rec.set_thread_ref(1);   // indexed thread ref
  rec.set_category_ref(2); // indexed string ref
  rec.set_name_ref(3);     // indexed string ref
  rec.set_timestamp(999'999ULL);

  auto buf = rec.buffer();
  auto word0 = read_le64(buf, 0);
  auto word1 = read_le64(buf, 8);

  CHECK((word0 & 0xF) == 4);            // type [0..3]
  CHECK(((word0 >> 4) & 0xFFF) == 3);   // size [4..15]
  CHECK(((word0 >> 16) & 0xF) == 0);    // event_type [16..19]
  CHECK(((word0 >> 20) & 0xF) == 0);    // argument_count [20..23]
  CHECK(((word0 >> 24) & 0xFF) == 1);   // thread_ref [24..31]
  CHECK(((word0 >> 32) & 0xFFFF) == 2); // category_ref [32..47]
  CHECK(((word0 >> 48) & 0xFFFF) == 3); // name_ref [48..63]
  CHECK(word1 == 999'999ULL);           // timestamp
}

TEST_CASE("FXT EventRecord: round-trip", "[fxt]") {
  EventRecord_owned rec;
  rec.set_record_type(4);
  rec.set_record_size(3);
  rec.set_event_type(2); // duration begin
  rec.set_argument_count(5);
  rec.set_thread_ref(42);
  rec.set_category_ref(100);
  rec.set_name_ref(200);
  rec.set_timestamp(1'700'000'000'000ULL);

  EventRecord_view view(rec.buffer());
  CHECK(view.record_type() == 4);
  CHECK(view.record_size() == 3);
  CHECK(view.event_type() == 2);
  CHECK(view.argument_count() == 5);
  CHECK(view.thread_ref() == 42);
  CHECK(view.category_ref() == 100);
  CHECK(view.name_ref() == 200);
  CHECK(view.timestamp() == 1'700'000'000'000ULL);
}

// ===========================================================================
// Wire sizes
// ===========================================================================

TEST_CASE("FXT wire sizes", "[fxt]") {
  CHECK(RecordHeader_owned::wire_size == 8);
  CHECK(InitRecord_owned::wire_size == 16);
  CHECK(StringRecord_owned::wire_size == 8);
  CHECK(ThreadRecord_owned::wire_size == 24);
  CHECK(EventRecord_owned::wire_size == 16);
}
