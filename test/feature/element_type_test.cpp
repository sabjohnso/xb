/// BES element-type test: repeat with typed element references.
///
/// Verifies that <repeat count-field="N" element-type="SensorReading"/>
/// generates a typed iteration API on the parent message view.
///
/// SensorReading is a fixed-size (8 byte) message.  SensorStream has
/// a header (stream_id + count) followed by count × SensorReading.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

using namespace element_type_test;

// ===========================================================================
// SensorReading: the element type
// ===========================================================================

TEST_CASE("element-type: SensorReading wire_size", "[element_type]") {
  CHECK(SensorReading_owned::wire_size == 8);
}

TEST_CASE("element-type: SensorReading round-trip", "[element_type]") {
  SensorReading_owned r;
  r.set_sensor_id(42);
  r.set_timestamp(1000000);
  r.set_value(9876);

  SensorReading_view view(r.buffer());
  CHECK(view.sensor_id() == 42);
  CHECK(view.timestamp() == 1000000);
  CHECK(view.value() == 9876);
}

// ===========================================================================
// SensorStream: header + repeated elements
// ===========================================================================

namespace {

  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }

  /// Build a SensorStream buffer with the given readings.
  auto
  build_stream(std::uint16_t stream_id,
               std::vector<SensorReading_owned>& readings)
      -> std::vector<std::byte> {
    std::vector<std::byte> buf;

    // Header: stream_id(16) + count(16)
    SensorStream_owned hdr;
    hdr.set_stream_id(stream_id);
    hdr.set_count(static_cast<std::uint16_t>(readings.size()));
    append(buf, hdr.buffer());

    // Append each reading
    for (auto& r : readings) {
      append(buf, r.buffer());
    }

    return buf;
  }

} // namespace

TEST_CASE("element-type: SensorStream header fields", "[element_type]") {
  std::vector<SensorReading_owned> readings;
  auto buf = build_stream(1, readings);

  SensorStream_view view(buf);
  CHECK(view.stream_id() == 1);
  CHECK(view.count() == 0);
}

TEST_CASE("element-type: SensorStream indexed element access",
          "[element_type]") {
  std::vector<SensorReading_owned> readings;

  SensorReading_owned r1;
  r1.set_sensor_id(1);
  r1.set_timestamp(100);
  r1.set_value(10);
  readings.push_back(std::move(r1));

  SensorReading_owned r2;
  r2.set_sensor_id(2);
  r2.set_timestamp(200);
  r2.set_value(20);
  readings.push_back(std::move(r2));

  SensorReading_owned r3;
  r3.set_sensor_id(3);
  r3.set_timestamp(300);
  r3.set_value(30);
  readings.push_back(std::move(r3));

  auto buf = build_stream(42, readings);

  SensorStream_view view(buf);
  CHECK(view.stream_id() == 42);
  CHECK(view.count() == 3);

  // Indexed access to each element
  auto elem0 = view.element(0);
  CHECK(elem0.sensor_id() == 1);
  CHECK(elem0.timestamp() == 100);
  CHECK(elem0.value() == 10);

  auto elem1 = view.element(1);
  CHECK(elem1.sensor_id() == 2);
  CHECK(elem1.timestamp() == 200);
  CHECK(elem1.value() == 20);

  auto elem2 = view.element(2);
  CHECK(elem2.sensor_id() == 3);
  CHECK(elem2.timestamp() == 300);
  CHECK(elem2.value() == 30);
}

TEST_CASE("element-type: SensorStream wire layout", "[element_type]") {
  std::vector<SensorReading_owned> readings;

  SensorReading_owned r;
  r.set_sensor_id(0x0102);
  r.set_timestamp(0x03040506);
  r.set_value(0x0708);
  readings.push_back(std::move(r));

  auto buf = build_stream(0xAABB, readings);

  // Header: stream_id(2 bytes) + count(2 bytes) = 4 bytes
  // Element: sensor_id(2) + timestamp(4) + value(2) = 8 bytes
  // Total: 12 bytes
  CHECK(buf.size() == 12);

  // Big-endian wire layout verification
  CHECK(static_cast<std::uint8_t>(buf[0]) == 0xAA);
  CHECK(static_cast<std::uint8_t>(buf[1]) == 0xBB);
  CHECK(static_cast<std::uint8_t>(buf[2]) == 0x00);
  CHECK(static_cast<std::uint8_t>(buf[3]) == 0x01); // count = 1
  // Element at offset 4
  CHECK(static_cast<std::uint8_t>(buf[4]) == 0x01);
  CHECK(static_cast<std::uint8_t>(buf[5]) == 0x02); // sensor_id
  CHECK(static_cast<std::uint8_t>(buf[6]) == 0x03);
  CHECK(static_cast<std::uint8_t>(buf[7]) == 0x04);
  CHECK(static_cast<std::uint8_t>(buf[8]) == 0x05);
  CHECK(static_cast<std::uint8_t>(buf[9]) == 0x06); // timestamp
  CHECK(static_cast<std::uint8_t>(buf[10]) == 0x07);
  CHECK(static_cast<std::uint8_t>(buf[11]) == 0x08); // value
}
