/// BES Variable-Length Message Stream — length-prefixed messages of
/// different types and sizes.
///
/// Demonstrates:
///   - Messages of different wire sizes in a single stream
///   - Length-prefixed framing (StreamHeader + per-message MessageBlock)
///   - Discriminant-based dispatch over variable-length payloads
///   - Building and parsing a complete message stream

#include <wire_types.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace varlen;

namespace {

  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }

  /// Parse a variable-length message stream.
  ///
  /// Wire format:
  ///   StreamHeader (4 bytes): message_count, stream_id
  ///   For each message:
  ///     MessageBlock (2 bytes): message_length
  ///     Body (message_length bytes): msg_type + payload
  void
  parse_stream(std::span<const std::byte> stream) {
    std::size_t offset = 0;

    // Read stream header
    if (stream.size() < offset + StreamHeader_owned::wire_size) {
      std::cerr << "Stream truncated at header\n";
      return;
    }
    StreamHeader_view header(
        stream.subspan(offset, StreamHeader_owned::wire_size));
    offset += StreamHeader_owned::wire_size;

    std::cout << "Stream " << header.stream_id() << ": "
              << header.message_count() << " messages\n\n";

    // Iterate over length-prefixed messages
    for (std::uint16_t i = 0; i < header.message_count(); ++i) {
      // Read message block (length prefix)
      if (stream.size() < offset + MessageBlock_owned::wire_size) {
        std::cerr << "Stream truncated at block " << i << "\n";
        return;
      }
      MessageBlock_view block(
          stream.subspan(offset, MessageBlock_owned::wire_size));
      offset += MessageBlock_owned::wire_size;

      auto msg_len = block.message_length();

      // Read message body
      if (stream.size() < offset + msg_len) {
        std::cerr << "Stream truncated at message " << i << "\n";
        return;
      }
      auto body = stream.subspan(offset, msg_len);
      offset += msg_len;

      // Dispatch by discriminant (first byte)
      auto msg_type = static_cast<std::uint8_t>(body[0]);

      switch (msg_type) {
        case 0x01: {
          Heartbeat_view hb(body);
          std::cout << "[" << i << "] Heartbeat: seq=" << hb.sequence() << '\n';
          break;
        }
        case 0x51: {
          Quote_view q(body);
          std::cout << "[" << i << "] Quote: " << q.symbol()
                    << " bid=" << q.bid() << " ask=" << q.ask()
                    << " bid_size=" << q.bid_size()
                    << " ask_size=" << q.ask_size() << '\n';
          break;
        }
        case 0x54: {
          Trade_view t(body);
          std::cout << "[" << i << "] Trade: " << t.symbol()
                    << " price=" << t.price() << " qty=" << t.quantity()
                    << " match_id=" << t.match_id() << '\n';
          break;
        }
        default:
          std::cout << "[" << i << "] Unknown: type=0x" << std::hex
                    << static_cast<int>(msg_type) << std::dec << '\n';
      }
    }
  }

} // namespace

auto
main() -> int {
  // --- Build messages of different sizes ---

  Heartbeat_owned hb(1);
  Quote_owned quote(/*symbol=*/"AAPL", /*bid=*/17500, /*ask=*/17525,
                    /*bid_size=*/100, /*ask_size=*/200);
  Trade_owned trade(/*symbol=*/"GOOG", /*price=*/28000, /*quantity=*/50,
                    /*match_id=*/99999);

  std::cout << "=== Wire Sizes ===\n"
            << "Heartbeat: " << Heartbeat_owned::wire_size << " bytes\n"
            << "Quote:     " << Quote_owned::wire_size << " bytes\n"
            << "Trade:     " << Trade_owned::wire_size << " bytes\n\n";

  // --- Assemble a length-prefixed message stream ---

  std::vector<std::byte> stream;

  // Stream header: 3 messages, stream ID 42
  StreamHeader_owned header;
  header.set_message_count(3);
  header.set_stream_id(42);
  append(stream, header.buffer());

  // Message 1: Heartbeat (5 bytes)
  MessageBlock_owned blk1;
  blk1.set_message_length(static_cast<std::uint16_t>(hb.buffer().size()));
  append(stream, blk1.buffer());
  append(stream, hb.buffer());

  // Message 2: Quote (25 bytes)
  MessageBlock_owned blk2;
  blk2.set_message_length(static_cast<std::uint16_t>(quote.buffer().size()));
  append(stream, blk2.buffer());
  append(stream, quote.buffer());

  // Message 3: Trade (21 bytes)
  MessageBlock_owned blk3;
  blk3.set_message_length(static_cast<std::uint16_t>(trade.buffer().size()));
  append(stream, blk3.buffer());
  append(stream, trade.buffer());

  std::cout << "Total stream: " << stream.size() << " bytes\n\n";

  // --- Parse the stream back ---

  std::cout << "=== Parsing Stream ===\n";
  parse_stream(stream);

  return 0;
}
