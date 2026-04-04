# Protocol Framing

BES supports multi-layer protocol parsing through **frame definitions** and
**frame stacks**. This models real-world protocols where messages are wrapped
in multiple layers of headers (e.g., Ethernet → IPv4 → UDP → application).

## Defining Frames

A frame describes a protocol layer's header:

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/protocol">
  <defaults byte-order="big-endian"/>

  <frame name="EthernetHeader">
    <field name="dst_mac"    bits="48" encoding="unsigned"/>
    <field name="src_mac"    bits="48" encoding="unsigned"/>
    <field name="ether_type" bits="16" encoding="unsigned"/>
  </frame>

  <frame name="IPv4Header">
    <field name="version"     bits="4"  encoding="unsigned"/>
    <field name="ihl"         bits="4"  encoding="unsigned"/>
    <field name="dscp"        bits="6"  encoding="unsigned"/>
    <field name="ecn"         bits="2"  encoding="unsigned"/>
    <field name="total_len"   bits="16" encoding="unsigned"/>
    <field name="id"          bits="16" encoding="unsigned"/>
    <field name="flags"       bits="3"  encoding="unsigned"/>
    <field name="frag_offset" bits="13" encoding="unsigned"/>
    <field name="ttl"         bits="8"  encoding="unsigned"/>
    <field name="protocol"    bits="8"  encoding="unsigned"/>
    <field name="checksum"    bits="16" encoding="unsigned"/>
    <field name="src_ip"      bits="32" encoding="unsigned"/>
    <field name="dst_ip"      bits="32" encoding="unsigned"/>
  </frame>

  <frame name="UDPHeader">
    <field name="src_port"  bits="16" encoding="unsigned"/>
    <field name="dst_port"  bits="16" encoding="unsigned"/>
    <field name="length"    bits="16" encoding="unsigned"/>
    <field name="checksum"  bits="16" encoding="unsigned"/>
  </frame>
</encoding>
```

## Application Messages

Define application-level messages alongside the frames:

```xml
  <message name="Ping" type="Ping" discriminant-value="0x01">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence" bits="32" encoding="unsigned"/>
  </message>

  <message name="Pong" type="Pong" discriminant-value="0x02">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence" bits="32" encoding="unsigned"/>
    <field name="latency_us" bits="32" encoding="unsigned"/>
  </message>
```

## Layer-by-Layer Parsing

The generated view types allow you to peel off headers one layer at a time,
with zero-copy access at each layer:

```cpp
#include <wire_types.hpp>
using namespace protocol;

void parse_packet(std::span<const std::byte> raw) {
    // Layer 1: Ethernet
    EthernetHeader_view eth(raw);
    auto payload = raw.subspan(EthernetHeader_view::wire_size);

    // Layer 2: IPv4
    IPv4Header_view ip(payload);
    auto ip_payload = payload.subspan(IPv4Header_view::wire_size);

    // Layer 3: UDP
    UDPHeader_view udp(ip_payload);
    auto app_data = ip_payload.subspan(UDPHeader_view::wire_size);

    // Layer 4: Application message dispatch
    auto msg_type = static_cast<uint8_t>(app_data[0]);
    switch (msg_type) {
    case 0x01: {
        Ping_view ping(app_data);
        handle_ping(ping.sequence());
        break;
    }
    case 0x02: {
        Pong_view pong(app_data);
        handle_pong(pong.sequence(), pong.latency_us());
        break;
    }
    }
}
```

## Building Frames

Construct layered frames from the inside out using `_owned` types:

```cpp
// Application message
Ping_owned ping(42);

// Calculate sizes
constexpr auto eth_size = EthernetHeader_view::wire_size;
constexpr auto ip_size  = IPv4Header_view::wire_size;
constexpr auto udp_size = UDPHeader_view::wire_size;
constexpr auto app_size = Ping_view::wire_size;
constexpr auto total    = eth_size + ip_size + udp_size + app_size;

// Assemble the frame
std::array<std::byte, total> frame{};
auto buf = std::span(frame);

EthernetHeader_mutable_view eth(buf.subspan(0, eth_size));
eth.set_ether_type(0x0800);  // IPv4

IPv4Header_mutable_view ip(buf.subspan(eth_size, ip_size));
ip.set_version(4);
ip.set_protocol(17);  // UDP
ip.set_total_len(ip_size + udp_size + app_size);

UDPHeader_mutable_view udp(buf.subspan(eth_size + ip_size, udp_size));
udp.set_length(udp_size + app_size);

// Copy application payload
std::ranges::copy(ping.buffer(),
                  buf.subspan(eth_size + ip_size + udp_size).begin());
```

## Complete Example

See [`examples/bes-protocol-stack/`](examples.md#protocol-stack) for a
complete working example with Ethernet, IPv4, UDP, and application message
layers.
