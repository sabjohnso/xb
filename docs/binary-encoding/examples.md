# BES Examples

xb ships four progressive examples in the `examples/` directory. Build them
with:

```sh
cmake -Dxb_BUILD_EXAMPLES=ON --preset default
cmake --build build --config Release
```

---

## Hello World

**`examples/bes-hello/`** — minimal single-message BES workflow.

### BES File

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/hello">
  <defaults byte-order="big-endian"/>

  <message name="Heartbeat" type="Heartbeat" discriminant-value="0x01">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence"     bits="32"/>
    <field name="timestamp_ns" bits="64" encoding="unsigned"/>
  </message>
</encoding>
```

### C++ Usage

```cpp
#include <wire_types.hpp>
#include <cstdint>
#include <iostream>

using namespace hello;

int main() {
    Heartbeat_owned hb(42, 1'700'000'000'000'000'000ULL);

    Heartbeat_view view(hb.buffer());
    std::cout << "msg_type:     " << unsigned(view.msg_type()) << "\n"
              << "sequence:     " << view.sequence() << "\n"
              << "timestamp_ns: " << view.timestamp_ns() << "\n"
              << "wire_size:    " << Heartbeat_view::wire_size << "\n";
}
```

**Demonstrates:** `_owned` construction, `_view` read-back, `wire_size`.

---

## Multiple Messages {: #multiple-messages }

**`examples/bes-market-data/`** — multiple message types with discriminant
dispatch.

### BES File

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/telemetry">
  <defaults byte-order="big-endian"/>

  <message name="SensorReading" type="SensorReading" discriminant-value="0x41">
    <wire-field name="msg_type" bits="8"/>
    <field name="device_id" bits="64" encoding="unsigned"/>
    <field name="channel"   bits="8"  encoding="ascii"/>
    <field name="value"     bits="32" encoding="unsigned"/>
    <field name="label"     bits="64" encoding="ascii"/>
    <field name="flags"     bits="32" encoding="unsigned"/>
  </message>

  <message name="ResetDevice" type="ResetDevice" discriminant-value="0x58">
    <wire-field name="msg_type" bits="8"/>
    <field name="device_id"  bits="64" encoding="unsigned"/>
    <field name="reason_code" bits="32" encoding="unsigned"/>
  </message>

  <message name="Alert" type="Alert" discriminant-value="0x50">
    <wire-field name="msg_type" bits="8"/>
    <field name="device_id"  bits="64" encoding="unsigned"/>
    <field name="severity"   bits="32" encoding="unsigned"/>
    <field name="code"       bits="32" encoding="unsigned"/>
    <field name="context_id" bits="64" encoding="unsigned"/>
  </message>
</encoding>
```

### C++ Usage

```cpp
#include <wire_types.hpp>
#include <iostream>

using namespace telemetry;

void dispatch_message(std::span<const std::byte> buf) {
    auto msg_type = static_cast<uint8_t>(buf[0]);
    switch (msg_type) {
    case 0x41: {
        SensorReading_view view(buf);
        std::cout << "READING " << view.label()
                  << " ch=" << view.channel()
                  << " val=" << view.value() << "\n";
        break;
    }
    case 0x58: {
        ResetDevice_view view(buf);
        std::cout << "RESET device " << view.device_id() << "\n";
        break;
    }
    case 0x50: {
        Alert_view view(buf);
        std::cout << "ALERT severity=" << view.severity()
                  << " code=" << view.code() << "\n";
        break;
    }
    }
}

int main() {
    SensorReading_owned reading(1001, 'A', 500, "TEMP    ", 0);
    ResetDevice_owned reset(1001, 200);
    Alert_owned alert(1001, 3, 4001, 9001);

    dispatch_message(reading.buffer());
    dispatch_message(reset.buffer());
    dispatch_message(alert.buffer());
}
```

**Demonstrates:** multiple message types, designated initializers,
discriminant-based dispatch, validation levels.

---

## Protocol Stack {: #protocol-stack }

**`examples/bes-protocol-stack/`** — multi-layer protocol headers with
zero-copy parsing.

### BES File

Defines Ethernet, IPv4, and UDP headers as frames, plus Ping/Pong
application messages:

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/protocol">
  <defaults byte-order="big-endian"/>

  <message name="EthernetHeader" type="EthernetHeader">
    <field name="dst_mac"    bits="48" encoding="unsigned"/>
    <field name="src_mac"    bits="48" encoding="unsigned"/>
    <field name="ether_type" bits="16" encoding="unsigned"/>
  </message>

  <message name="IPv4Header" type="IPv4Header">
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
  </message>

  <message name="UDPHeader" type="UDPHeader">
    <field name="src_port"  bits="16" encoding="unsigned"/>
    <field name="dst_port"  bits="16" encoding="unsigned"/>
    <field name="length"    bits="16" encoding="unsigned"/>
    <field name="checksum"  bits="16" encoding="unsigned"/>
  </message>

  <message name="Ping" type="Ping" discriminant-value="0x01">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence" bits="32" encoding="unsigned"/>
  </message>

  <message name="Pong" type="Pong" discriminant-value="0x02">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence"   bits="32" encoding="unsigned"/>
    <field name="latency_us" bits="32" encoding="unsigned"/>
  </message>
</encoding>
```

### C++ Usage

The example builds a complete Ethernet/IPv4/UDP frame, then parses it
layer-by-layer:

```cpp
// Build the frame
Ping_owned ping(1);

constexpr auto eth_sz = EthernetHeader_view::wire_size;
constexpr auto ip_sz  = IPv4Header_view::wire_size;
constexpr auto udp_sz = UDPHeader_view::wire_size;
constexpr auto app_sz = Ping_view::wire_size;

std::array<std::byte, eth_sz + ip_sz + udp_sz + app_sz> frame{};
// ... fill headers, copy payload ...

// Parse layer by layer
EthernetHeader_view eth(std::span(frame).subspan(0, eth_sz));
IPv4Header_view ip(std::span(frame).subspan(eth_sz, ip_sz));
UDPHeader_view udp(std::span(frame).subspan(eth_sz + ip_sz, udp_sz));

auto app = std::span(frame).subspan(eth_sz + ip_sz + udp_sz);
Ping_view msg(app);
```

**Demonstrates:** frame composition, layer-by-layer zero-copy parsing,
sub-byte field widths (4-bit, 3-bit, 13-bit, etc.).

---

## Variable-Length Messages {: #variable-length }

**`examples/bes-variable-length/`** — length-prefixed message streams with
different-sized messages.

### BES File

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/varlen">
  <defaults byte-order="big-endian"/>

  <message name="StreamHeader" type="StreamHeader">
    <field name="message_count" bits="16" encoding="unsigned"/>
    <field name="stream_id"     bits="32" encoding="unsigned"/>
  </message>

  <message name="MessageBlock" type="MessageBlock">
    <field name="length" bits="16" encoding="unsigned"/>
  </message>

  <message name="Heartbeat" type="Heartbeat" discriminant-value="0x01">
    <wire-field name="msg_type" bits="8"/>
    <field name="sequence" bits="32" encoding="unsigned"/>
  </message>

  <message name="Status" type="Status" discriminant-value="0x51">
    <wire-field name="msg_type" bits="8"/>
    <field name="device"     bits="64" encoding="ascii"/>
    <field name="cpu_usage"  bits="32" encoding="unsigned"/>
    <field name="mem_usage"  bits="32" encoding="unsigned"/>
    <field name="disk_read"  bits="32" encoding="unsigned"/>
    <field name="disk_write" bits="32" encoding="unsigned"/>
  </message>

  <message name="Event" type="Event" discriminant-value="0x54">
    <wire-field name="msg_type" bits="8"/>
    <field name="device"   bits="64" encoding="ascii"/>
    <field name="code"     bits="32" encoding="unsigned"/>
    <field name="severity" bits="32" encoding="unsigned"/>
    <field name="event_id" bits="64" encoding="unsigned"/>
  </message>
</encoding>
```

### C++ Usage

```cpp
void parse_stream(std::span<const std::byte> data) {
    StreamHeader_view header(data);
    auto pos = StreamHeader_view::wire_size;

    for (uint16_t i = 0; i < header.message_count(); ++i) {
        // Read length prefix
        MessageBlock_view block(data.subspan(pos));
        auto msg_len = block.length();
        pos += MessageBlock_view::wire_size;

        // Dispatch by message type
        auto msg = data.subspan(pos, msg_len);
        auto msg_type = static_cast<uint8_t>(msg[0]);

        switch (msg_type) {
        case 0x01: { Heartbeat_view hb(msg); /* ... */ break; }
        case 0x51: { Status_view s(msg);     /* ... */ break; }
        case 0x54: { Event_view e(msg);      /* ... */ break; }
        }

        pos += msg_len;
    }
}
```

**Demonstrates:** length-prefixed messages, stream parsing, heterogeneous
message sizes, stream header metadata.
