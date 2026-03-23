/// BES Protocol Stack — multi-layer binary protocol parsing using
/// generated view types.
///
/// Demonstrates:
///   - Building a complete Ethernet → IPv4 → UDP → application frame
///   - Layer-by-layer parsing with generated view types
///   - Zero-copy access at each protocol layer

#include <wire_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace protocol;

namespace {

  void
  append(std::vector<std::byte>& buf, std::span<const std::byte> data) {
    buf.insert(buf.end(), data.begin(), data.end());
  }

} // namespace

auto
main() -> int {
  // --- Build an application-level Ping message ---
  //     The aggregate constructor auto-sets the wire-field msg_type
  //     from the discriminant-value (0x01).

  Ping_owned ping(1, 1'700'000'000'000'000'000ULL);

  // --- Build UDP layer ---

  auto app_payload = ping.buffer();
  auto udp_total = static_cast<std::uint16_t>(UDPHeader_owned::wire_size +
                                              app_payload.size());

  UDPHeader_owned udp;
  udp.set_src_port(12345);
  udp.set_dst_port(9999);
  udp.set_length(udp_total);
  udp.set_checksum(0);

  // --- Build IPv4 layer ---

  auto ip_total =
      static_cast<std::uint16_t>(IPv4Header_owned::wire_size + udp_total);

  IPv4Header_owned ip;
  ip.set_version_ihl(0x45); // IPv4, 20-byte header
  ip.set_dscp_ecn(0x00);
  ip.set_total_length(ip_total);
  ip.set_identification(0x0001);
  ip.set_flags_fragment(0x4000); // Don't Fragment
  ip.set_ttl(64);
  ip.set_protocol(17); // UDP
  ip.set_header_checksum(0);
  ip.set_src_addr(0xC0A80001); // 192.168.0.1
  ip.set_dst_addr(0xC0A80002); // 192.168.0.2

  // --- Build Ethernet layer ---

  std::array<std::byte, 6> dst_mac = {std::byte{0xFF}, std::byte{0xFF},
                                      std::byte{0xFF}, std::byte{0xFF},
                                      std::byte{0xFF}, std::byte{0xFF}};
  std::array<std::byte, 6> src_mac = {std::byte{0x00}, std::byte{0x1A},
                                      std::byte{0x2B}, std::byte{0x3C},
                                      std::byte{0x4D}, std::byte{0x5E}};

  EthernetHeader_owned eth;
  eth.set_dst_mac(dst_mac);
  eth.set_src_mac(src_mac);
  eth.set_ether_type(0x0800); // IPv4

  // --- Assemble the complete frame ---

  std::vector<std::byte> frame;
  append(frame, eth.buffer());
  append(frame, ip.buffer());
  append(frame, udp.buffer());
  append(frame, app_payload);

  std::cout << "Frame size: " << frame.size() << " bytes\n\n";

  // --- Parse layer by layer using generated view types ---
  //
  // Each view is a zero-copy overlay on the frame buffer.
  // No data is copied — field accessors read directly from the
  // buffer, performing byte-swapping inline.

  std::size_t offset = 0;

  // Layer 1: Ethernet
  std::cout << "=== Ethernet ===\n";
  EthernetHeader_view eth_view(std::span<const std::byte>(frame).subspan(
      offset, EthernetHeader_owned::wire_size));
  std::cout << "EtherType: 0x" << std::hex << eth_view.ether_type() << std::dec
            << '\n';
  offset += EthernetHeader_owned::wire_size;

  // Layer 2: IPv4
  std::cout << "\n=== IPv4 ===\n";
  IPv4Header_view ip_view(std::span<const std::byte>(frame).subspan(
      offset, IPv4Header_owned::wire_size));
  std::cout << "Protocol:     " << static_cast<int>(ip_view.protocol()) << '\n'
            << "Total length: " << ip_view.total_length() << '\n'
            << "TTL:          " << static_cast<int>(ip_view.ttl()) << '\n'
            << "Src:          0x" << std::hex << ip_view.src_addr() << std::dec
            << '\n'
            << "Dst:          0x" << std::hex << ip_view.dst_addr() << std::dec
            << '\n';
  offset += IPv4Header_owned::wire_size;

  // Layer 3: UDP
  std::cout << "\n=== UDP ===\n";
  UDPHeader_view udp_view(std::span<const std::byte>(frame).subspan(
      offset, UDPHeader_owned::wire_size));
  std::cout << "Src port: " << udp_view.src_port() << '\n'
            << "Dst port: " << udp_view.dst_port() << '\n'
            << "Length:   " << udp_view.length() << '\n';
  offset += UDPHeader_owned::wire_size;

  // Layer 4: Application message
  std::cout << "\n=== Application ===\n";
  auto payload = std::span<const std::byte>(frame).subspan(offset);
  auto msg_type = static_cast<std::uint8_t>(payload[0]);

  switch (msg_type) {
    case 0x01: {
      Ping_view view(payload);
      std::cout << "Ping: seq=" << view.sequence()
                << " ts=" << view.timestamp_ns() << '\n';
      break;
    }
    case 0x02: {
      Pong_view view(payload);
      std::cout << "Pong: seq=" << view.sequence()
                << " ts=" << view.timestamp_ns() << '\n';
      break;
    }
    default:
      std::cout << "Unknown: type=0x" << std::hex << static_cast<int>(msg_type)
                << std::dec << '\n';
  }

  // --- Wire sizes are compile-time constants ---

  std::cout << "\n=== Wire Sizes ===\n"
            << "EthernetHeader: " << EthernetHeader_owned::wire_size
            << " bytes\n"
            << "IPv4Header:     " << IPv4Header_owned::wire_size << " bytes\n"
            << "UDPHeader:      " << UDPHeader_owned::wire_size << " bytes\n"
            << "Ping:           " << Ping_owned::wire_size << " bytes\n"
            << "Pong:           " << Pong_owned::wire_size << " bytes\n";

  return 0;
}
