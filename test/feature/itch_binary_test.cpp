/// ITCH 5.0 binary encoding integration test.
///
/// Demonstrates the full network stack from Ethernet frame through
/// to ITCH message fields, all using generated binary view/owned types:
///   Ethernet → IPv4 → UDP → MoldUDP64 → message blocks → ITCH messages

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

// The ITCH BES has target-namespace="http://example.com/itch"
// → C++ namespace "itch" (short-name style)
using namespace itch;

namespace {

  constexpr std::byte
  B(unsigned char c) {
    return static_cast<std::byte>(c);
  }

  // Append raw bytes from a span to a vector
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#pragma GCC diagnostic pop
#endif

} // namespace

// ===========================================================================
// Individual layer tests
// ===========================================================================

TEST_CASE("Ethernet header: round-trip", "[itch][ethernet]") {
  EthernetHeader_owned hdr;

  std::array<std::byte, 6> dst = {B(0xFF), B(0xFF), B(0xFF),
                                  B(0xFF), B(0xFF), B(0xFF)};
  std::array<std::byte, 6> src = {B(0x00), B(0x1A), B(0x2B),
                                  B(0x3C), B(0x4D), B(0x5E)};
  hdr.set_dst_mac(dst);
  hdr.set_src_mac(src);
  hdr.set_ether_type(0x0800); // IPv4

  EthernetHeader_view view(hdr.buffer());

  CHECK(view.ether_type() == 0x0800);
  CHECK(EthernetHeader_owned::wire_size == 14);

  auto dst_out = view.dst_mac();
  CHECK(dst_out[0] == B(0xFF));
  CHECK(dst_out[5] == B(0xFF));

  auto src_out = view.src_mac();
  CHECK(src_out[0] == B(0x00));
  CHECK(src_out[1] == B(0x1A));
}

TEST_CASE("IPv4 header: round-trip", "[itch][ipv4]") {
  IPv4Header_owned hdr;
  hdr.set_version_ihl(0x45); // IPv4, 20-byte header
  hdr.set_dscp_ecn(0x00);
  hdr.set_total_length(100);
  hdr.set_identification(0x1234);
  hdr.set_flags_fragment(0x4000); // Don't Fragment
  hdr.set_ttl(64);
  hdr.set_protocol(17); // UDP
  hdr.set_header_checksum(0);
  hdr.set_src_addr(0xC0A80001); // 192.168.0.1
  hdr.set_dst_addr(0xEFA00001); // 239.160.0.1 (multicast)

  IPv4Header_view view(hdr.buffer());

  CHECK(view.version_ihl() == 0x45);
  CHECK(view.total_length() == 100);
  CHECK(view.protocol() == 17);
  CHECK(view.src_addr() == 0xC0A80001);
  CHECK(view.dst_addr() == 0xEFA00001);
  CHECK(IPv4Header_owned::wire_size == 20);
}

TEST_CASE("UDP header: round-trip", "[itch][udp]") {
  UDPHeader_owned hdr;
  hdr.set_src_port(12345);
  hdr.set_dst_port(26400); // typical ITCH port
  hdr.set_length(80);
  hdr.set_checksum(0);

  UDPHeader_view view(hdr.buffer());

  CHECK(view.src_port() == 12345);
  CHECK(view.dst_port() == 26400);
  CHECK(view.length() == 80);
  CHECK(UDPHeader_owned::wire_size == 8);
}

TEST_CASE("MoldUDP64 header: round-trip", "[itch][moldudp]") {
  MoldUDP64Header_owned hdr;
  hdr.set_session("SESS000001");
  hdr.set_sequence_number(1);
  hdr.set_message_count(3);

  MoldUDP64Header_view view(hdr.buffer());

  CHECK(view.session() == "SESS000001");
  CHECK(view.sequence_number() == 1);
  CHECK(view.message_count() == 3);
  CHECK(MoldUDP64Header_owned::wire_size == 20);
}

TEST_CASE("MoldUDP64 message block header: round-trip", "[itch][moldudp]") {
  MoldUDP64MessageBlock_owned blk;
  blk.set_message_length(36);

  MoldUDP64MessageBlock_view view(blk.buffer());

  CHECK(view.message_length() == 36);
  CHECK(MoldUDP64MessageBlock_owned::wire_size == 2);
}

// ===========================================================================
// ITCH message tests
// ===========================================================================

TEST_CASE("ITCH AddOrder: owned round-trip", "[itch][binary]") {
  AddOrder_owned order;
  order.set_msg_type(0x41);
  order.set_stock_locate(1234);
  order.set_tracking_number(5678);
  order.set_order_reference(42);
  order.set_side(0x42);
  order.set_shares(100);
  order.set_stock("AAPL");
  order.set_price(1500000);

  AddOrder_view view(order.buffer());

  CHECK(view.msg_type() == 0x41);
  CHECK(view.stock_locate() == 1234);
  CHECK(view.tracking_number() == 5678);
  CHECK(view.order_reference() == 42);
  CHECK(view.side() == 0x42);
  CHECK(view.shares() == 100);
  CHECK(view.stock() == "AAPL");
  CHECK(view.price() == 1500000);
}

TEST_CASE("ITCH Trade: round-trip", "[itch][binary]") {
  Trade_owned trade;
  trade.set_msg_type(0x50);
  trade.set_stock_locate(1);
  trade.set_tracking_number(0);
  trade.set_order_reference(999);
  trade.set_side(0x42);
  trade.set_shares(200);
  trade.set_stock("GOOG");
  trade.set_price(1410000);
  trade.set_match_number(7777);

  Trade_view view(trade.buffer());

  CHECK(view.msg_type() == 0x50);
  CHECK(view.shares() == 200);
  CHECK(view.stock() == "GOOG");
  CHECK(view.price() == 1410000);
  CHECK(view.match_number() == 7777);
}

TEST_CASE("ITCH AddOrder: types satisfy concept", "[itch][binary]") {
  static_assert(AddOrder_message<AddOrder_view<>>);
  static_assert(AddOrder_message<AddOrder_owned>);
  SUCCEED();
}

// ===========================================================================
// Full stack: Ethernet → IPv4 → UDP → MoldUDP64 → ITCH messages
// Uses the generated parse_itch_udp() frame parser
// ===========================================================================

TEST_CASE("Full stack: build and parse via generated frame parser",
          "[itch][fullstack]") {
  // --- Build ITCH messages ---

  AddOrder_owned add_order;
  add_order.set_msg_type(0x41);
  add_order.set_stock_locate(1);
  add_order.set_tracking_number(0);
  add_order.set_order_reference(100001);
  add_order.set_side(0x42);
  add_order.set_shares(500);
  add_order.set_stock("AAPL");
  add_order.set_price(1750000);

  Trade_owned trade;
  trade.set_msg_type(0x50);
  trade.set_stock_locate(1);
  trade.set_tracking_number(0);
  trade.set_order_reference(100001);
  trade.set_side(0x42);
  trade.set_shares(200);
  trade.set_stock("AAPL");
  trade.set_price(1750000);
  trade.set_match_number(999);

  auto add_buf = add_order.buffer();
  auto trade_buf = trade.buffer();

  // --- Build MoldUDP64 payload ---

  std::vector<std::byte> mold_payload;

  MoldUDP64Header_owned mold_hdr;
  mold_hdr.set_session("ABCDEFGHIJ");
  mold_hdr.set_sequence_number(42);
  mold_hdr.set_message_count(2);
  append(mold_payload, mold_hdr.buffer());

  MoldUDP64MessageBlock_owned blk1;
  blk1.set_message_length(static_cast<std::uint16_t>(add_buf.size()));
  append(mold_payload, blk1.buffer());
  append(mold_payload, add_buf);

  MoldUDP64MessageBlock_owned blk2;
  blk2.set_message_length(static_cast<std::uint16_t>(trade_buf.size()));
  append(mold_payload, blk2.buffer());
  append(mold_payload, trade_buf);

  auto udp_payload_len = static_cast<std::uint16_t>(mold_payload.size());

  // --- Build UDP ---

  UDPHeader_owned udp_hdr;
  udp_hdr.set_src_port(12345);
  udp_hdr.set_dst_port(26400);
  udp_hdr.set_length(
      static_cast<std::uint16_t>(UDPHeader_owned::wire_size + udp_payload_len));
  udp_hdr.set_checksum(0);

  // --- Build IPv4 ---

  auto ip_total_len =
      static_cast<std::uint16_t>(IPv4Header_owned::wire_size +
                                 UDPHeader_owned::wire_size + udp_payload_len);

  IPv4Header_owned ip_hdr;
  ip_hdr.set_version_ihl(0x45);
  ip_hdr.set_dscp_ecn(0x00);
  ip_hdr.set_total_length(ip_total_len);
  ip_hdr.set_identification(0x0001);
  ip_hdr.set_flags_fragment(0x4000);
  ip_hdr.set_ttl(64);
  ip_hdr.set_protocol(17);
  ip_hdr.set_header_checksum(0);
  ip_hdr.set_src_addr(0xC0A80001);
  ip_hdr.set_dst_addr(0xEFA00001);

  // --- Build Ethernet ---

  std::array<std::byte, 6> dst_mac = {B(0x01), B(0x00), B(0x5E),
                                      B(0x60), B(0x00), B(0x01)};
  std::array<std::byte, 6> src_mac = {B(0x00), B(0x1A), B(0x2B),
                                      B(0x3C), B(0x4D), B(0x5E)};

  EthernetHeader_owned eth_hdr;
  eth_hdr.set_dst_mac(dst_mac);
  eth_hdr.set_src_mac(src_mac);
  eth_hdr.set_ether_type(0x0800);

  // --- Assemble complete frame ---

  std::vector<std::byte> frame;
  append(frame, eth_hdr.buffer());
  append(frame, ip_hdr.buffer());
  append(frame, udp_hdr.buffer());
  frame.insert(frame.end(), mold_payload.begin(), mold_payload.end());

  // === Parse using the generated frame parser ===

  std::vector<std::uint8_t> msg_types;
  std::vector<std::uint32_t> prices;

  parse_itch_udp(frame, [&](std::span<const std::byte> msg_body) {
    auto msg_type = static_cast<std::uint8_t>(msg_body[0]);
    msg_types.push_back(msg_type);

    if (msg_type == 0x41) {
      AddOrder_view view(msg_body);
      CHECK(view.stock_locate() == 1);
      CHECK(view.shares() == 500);
      CHECK(view.stock() == "AAPL");
      prices.push_back(view.price());
    } else if (msg_type == 0x50) {
      Trade_view view(msg_body);
      CHECK(view.stock_locate() == 1);
      CHECK(view.shares() == 200);
      CHECK(view.match_number() == 999);
      prices.push_back(view.price());
    } else {
      FAIL("Unknown message type: " << static_cast<int>(msg_type));
    }
  });

  // Verify both messages were delivered
  REQUIRE(msg_types.size() == 2);
  CHECK(msg_types[0] == 0x41); // AddOrder
  CHECK(msg_types[1] == 0x50); // Trade
  CHECK(prices[0] == 1750000);
  CHECK(prices[1] == 1750000);
}
