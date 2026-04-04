# Binary Encoding Overview

xb includes a declarative **Binary Encoding Specification (BES)** format for
mapping XSD data types to wire-level binary layouts. BES targets
high-performance use cases — binary protocols, telemetry streams, and
low-latency messaging — where direct buffer access matters more than XML
serialization.

## The Concept

BES is to xb what ACN is to ASN.1: a separate document that describes how
abstract data types map to concrete wire encodings.

![BES flow: XSD + BES = Generated C++](../images/bes-flow-light.svg#only-light)
![BES flow: XSD + BES = Generated C++](../images/bes-flow-dark.svg#only-dark)

The same XSD type can have XML serialization *and* multiple binary encodings.
The XSD defines what the data is; the BES defines how it sits on the wire.

## Key Capabilities

- **Bit-level field widths** — fields can be any number of bits, not just
  byte-aligned
- **Per-field endianness** — big-endian and little-endian in the same message
- **Primitive encodings** — unsigned, two's complement, BCD, IEEE 754,
  fixed-point, ASCII, UTF-8/UTF-16, epoch timestamps, bitsets
- **Protocol framing** — composable frame stacks (Ethernet → IP → UDP → app)
- **Message definitions** — discriminant-based dispatch, repeating groups,
  choices, computed fields, conditional presence
- **CSS-like selectors** — map XSD components to encodings by qname, path,
  type, or namespace
- **Import cascading** — BES documents can import and override other BES
  documents
- **Zero-copy access** — generated `_view` types read directly from the
  buffer with `constexpr` accessors

## Generated Types

For each BES message, xb generates three C++ classes:

| Type | Description |
|------|-------------|
| `Message_view` | Read-only view over `std::span<const std::byte>` — zero-copy field access with byte-swapping and bit extraction inline |
| `Message_mutable_view` | Mutable view with setters for in-place field modification |
| `Message_owned` | Self-contained type that manages its own buffer, with an aggregate constructor |

All view types provide a compile-time `wire_size` constant.

## BES-Only Workflow

BES files can be used **without a separate XSD schema**. xb infers XSD types
from field properties (width, encoding) and generates a synthetic schema
internally. This is useful when the binary format *is* the specification.

```sh
# Generate binary types from a BES file alone
xb generate --encoding protocol.bes.xml --output-dir out/ --binary-only

# Optionally, export the inferred XSD
xb generate-xsd --encoding protocol.bes.xml --output protocol.xsd
```

See [BES-Only Workflow](bes-only-workflow.md) for details.

## Quick Example

**1. Define a message (`heartbeat.bes.xml`):**

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

**2. Use the generated types:**

```cpp
#include <wire_types.hpp>
using namespace hello;

// Construct with aggregate initializer
Heartbeat_owned hb(42, 1'700'000'000'000'000'000ULL);

// Zero-copy view
Heartbeat_view view(hb.buffer());
view.msg_type();     // 0x01 (auto-set from discriminant-value)
view.sequence();     // 42
view.timestamp_ns(); // 1700000000000000000

// Compile-time wire size
static_assert(Heartbeat_view::wire_size == 13);
```

## Next Steps

- [BES Format](bes-format.md) — XML elements and attributes reference
- [Generated Types](generated-types.md) — `_view`, `_mutable_view`, `_owned`
  in detail
- [Validation Levels](validation-levels.md) — control runtime checks
- [Protocol Framing](protocol-framing.md) — multi-layer protocol parsing
- [Examples](examples.md) — four progressive working examples
