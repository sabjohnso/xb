/// BES self-delimiting repeat test.
///
/// Verifies that <repeat element-type="TaggedMessage"/> (no count-field)
/// generates a fill-remaining iteration API using compute_size to
/// determine each element's byte length from its discriminant.
///
/// TaggedMessage has three alternatives of different sizes (5, 7, 4 bytes).

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace self_delimit_test;

// ===========================================================================
// Helpers
// ===========================================================================

namespace {

  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }

  /// Append only the self-delimiting portion of a TaggedMessage.
  void
  append_msg(std::vector<std::byte>& buf, const TaggedMessage_owned& msg) {
    auto full = msg.buffer();
    auto sz = TaggedMessage_view<>::compute_size(full);
    buf.insert(buf.end(), full.begin(),
               full.begin() + static_cast<std::ptrdiff_t>(sz));
  }

  auto
  make_heartbeat(std::uint32_t seq) -> TaggedMessage_owned {
    TaggedMessage_owned msg;
    msg.set_tag(1);
    msg.set_sequence(seq);
    return msg;
  }

  auto
  make_temperature(std::uint16_t sensor, std::uint32_t val)
      -> TaggedMessage_owned {
    TaggedMessage_owned msg;
    msg.set_tag(2);
    msg.set_sensor_id(sensor);
    msg.set_value(val);
    return msg;
  }

  auto
  make_alert(std::uint8_t level, std::uint16_t code) -> TaggedMessage_owned {
    TaggedMessage_owned msg;
    msg.set_tag(3);
    msg.set_level(level);
    msg.set_code(code);
    return msg;
  }

} // namespace

// ===========================================================================
// TaggedMessage: compute_size
// ===========================================================================

TEST_CASE("self-delimit: TaggedMessage compute_size", "[self_delimit]") {
  auto hb = make_heartbeat(42);
  CHECK(TaggedMessage_view<>::compute_size(hb.buffer()) == 5);

  auto temp = make_temperature(1, 100);
  CHECK(TaggedMessage_view<>::compute_size(temp.buffer()) == 7);

  auto alert = make_alert(5, 0x1234);
  CHECK(TaggedMessage_view<>::compute_size(alert.buffer()) == 4);
}

// ===========================================================================
// MessageStream: fill-remaining iteration
// ===========================================================================

TEST_CASE("self-delimit: MessageStream iteration", "[self_delimit]") {
  // Build stream: header + heartbeat + temperature + alert
  std::vector<std::byte> buf;

  // Header: stream_id = 42
  MessageStream_owned hdr;
  hdr.set_stream_id(42);
  append(buf, hdr.buffer());

  auto hb = make_heartbeat(100);
  append_msg(buf, hb);

  auto temp = make_temperature(7, 25000);
  append_msg(buf, temp);

  auto alert = make_alert(3, 0xABCD);
  append_msg(buf, alert);

  // Total: 2 (header) + 5 + 7 + 4 = 18 bytes
  CHECK(buf.size() == 18);

  MessageStream_view view(buf);
  CHECK(view.stream_id() == 42);

  // Iterate using for_each_element
  std::vector<std::uint8_t> tags;
  view.for_each_element(
      [&](TaggedMessage_view<> elem) { tags.push_back(elem.tag()); });

  REQUIRE(tags.size() == 3);
  CHECK(tags[0] == 1); // heartbeat
  CHECK(tags[1] == 2); // temperature
  CHECK(tags[2] == 3); // alert
}

TEST_CASE("self-delimit: MessageStream element values", "[self_delimit]") {
  std::vector<std::byte> buf;

  MessageStream_owned hdr;
  hdr.set_stream_id(1);
  append(buf, hdr.buffer());

  auto hb = make_heartbeat(999);
  append_msg(buf, hb);

  auto temp = make_temperature(42, 12345);
  append_msg(buf, temp);

  MessageStream_view view(buf);

  std::size_t count = 0;
  view.for_each_element([&](TaggedMessage_view<> elem) {
    if (count == 0) {
      CHECK(elem.tag() == 1);
      CHECK(elem.sequence() == 999);
    } else if (count == 1) {
      CHECK(elem.tag() == 2);
      CHECK(elem.sensor_id() == 42);
      CHECK(elem.value() == 12345);
    }
    ++count;
  });

  CHECK(count == 2);
}

TEST_CASE("self-delimit: empty stream", "[self_delimit]") {
  std::vector<std::byte> buf;

  MessageStream_owned hdr;
  hdr.set_stream_id(0);
  append(buf, hdr.buffer());

  MessageStream_view view(buf);
  CHECK(view.stream_id() == 0);

  std::size_t count = 0;
  view.for_each_element([&](TaggedMessage_view<>) { ++count; });
  CHECK(count == 0);
}
