# xb

XML databinding for C++20 — serialization, deserialization, and C++ code
generation from [XML Schema 1.1](https://www.w3.org/TR/xmlschema11-1/)
definitions, with full WSDL and SOAP support.

## Overview

xb generates C++ types and serialization code from XML Schema definitions,
providing a type-safe interface for reading and writing XML documents. It is
an alternative to
[Code Synthesis XSD](https://www.codesynthesis.com/products/xsd/) with
different design decisions:

- **`std::variant` for XSD choice groups** instead of pointer-based
  discriminated unions — gives value semantics and compile-time
  exhaustiveness checking via `std::visit`.
- **Low-latency friendly types** — value types, minimal heap allocations,
  cache-friendly layouts.
- **Configurable type mapping** — all XSD-to-C++ type mappings can be
  overridden via an XML configuration document. Standards-conformant defaults
  map `xs:decimal` to `xb::decimal` (exact, arbitrary-precision) and
  `xs:integer` to `xb::integer`.

## Features

### Schema Support

- **XML Schema 1.1** — full parser with code generation to C++ structs,
  enums, and serialization routines
- **RELAX NG** — XML and Compact Syntax (.rnc) parsers with translation to
  XSD
- **DTD** — parser with translation to XSD
- **Schematron** — constraint parser with XSD overlay support
- **XPath** — expression parsing and evaluation

### Web Services

- **WSDL 1.1 and 2.0** — parsing, resolution to a version-independent
  service IR, and code generation of client stubs and server skeletons
- **SOAP 1.1 and 1.2** — envelope, header, and fault
  serialization/deserialization
- **WS-Addressing** — message addressing properties, endpoint references,
  correlation
- **WS-Security** — username tokens, X.509 tokens, cryptographic operations
  (requires OpenSSL)
- **MTOM/XOP** — binary-optimized XML packaging with MIME multipart support
- **HTTP transport** — libcurl-based transport with TLS, client certificates,
  timeouts, and redirect handling (requires libcurl)

### Binary Encoding (BES)

xb includes a declarative Binary Encoding Specification (BES) format for
mapping XSD data types to wire-level binary layouts. BES targets
high-performance use cases — market data feeds, binary protocols, and
low-latency messaging — where direct buffer access matters more than XML
serialization.

A BES document (`*.bes.xml`, validated by `schema/xb-encoding.xsd`) describes:

- **Field layouts** — width in bits, byte order, bit order, alignment, padding
- **Primitive encodings** — unsigned, two's complement, BCD, IEEE 754,
  fixed-point, ASCII, UTF-8/UTF-16, epoch timestamps, bitsets, and more
- **Protocol framing** — named frame layers (Ethernet, IPv4, UDP, etc.)
  composed into frame stacks for multi-layer protocol parsing
- **Message definitions** — application messages with discriminant-based
  dispatch, repeating groups, choices, computed fields, and conditional
  presence
- **Selector matching** — CSS-like specificity rules (qname, path, type,
  namespace) for binding encoding rules to XSD components
- **Import cascading** — BES documents can import and override other BES
  documents

#### Generated Types

For each BES message, xb generates three C++ classes:

- **`_view`** — a `constexpr` read-only view over a `std::span<const std::byte>`
  buffer, with zero-copy field accessors that handle byte-swapping and bit
  extraction inline
- **`_mutable_view`** — a mutable view with setters for in-place field
  modification
- **`_owned`** — a self-contained type that manages its own buffer, with an
  aggregate constructor and conversion to view types

For frame stacks, xb generates frame parsers that peel protocol headers and
dispatch to the correct message type based on discriminant fields.

#### BES-Only Workflow

BES files can be used without a separate XSD schema. xb infers XSD types from
field properties (width, encoding) and generates a synthetic schema
internally. This is useful when the binary format *is* the specification.

```sh
# Generate binary types from a BES file alone
xb generate --encoding protocol.bes.xml --output-dir out/ --binary-only

# Generate an XSD schema from a BES file
xb generate-xsd --encoding protocol.bes.xml --output protocol.xsd
```

#### Quick Start

1. Write a BES file (`heartbeat.bes.xml`):

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

2. Add a CMake target using `xb_add_library`:

```cmake
xb_add_library(
  TARGET hello_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/heartbeat.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE hello_types)
```

3. Use the generated types in C++:

```cpp
#include <wire_types.hpp>

using namespace hello;

int main() {
  // Aggregate constructor auto-sets the wire-field msg_type
  Heartbeat_owned hb(42, 1'700'000'000'000'000'000ULL);

  // Zero-copy read-only view over the buffer
  Heartbeat_view view(hb.buffer());

  view.msg_type();     // 0x01 (from discriminant-value)
  view.sequence();     // 42
  view.timestamp_ns(); // 1700000000000000000
}
```

See `examples/bes-hello/` for the complete working example.

#### Example: Generated Code

Given a BES message definition:

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/protocol">
  <defaults byte-order="big-endian"/>

  <message name="AddOrder" discriminant-value="0x41">
    <wire-field name="msg_type" bits="8"/>
    <field name="stock_locate" bits="16"/>
    <field name="tracking_number" bits="16"/>
    <field name="timestamp" bits="48" encoding="unsigned"/>
    <field name="order_ref" bits="64" encoding="unsigned"/>
    <field name="side" bits="8" encoding="ascii"/>
    <field name="shares" bits="32" encoding="unsigned"/>
    <field name="stock" bits="64" encoding="ascii"/>
    <field name="price" bits="32" encoding="unsigned"/>
  </message>
</encoding>
```

xb generates:

```cpp
class AddOrder_view {
public:
  explicit constexpr AddOrder_view(std::span<const std::byte> buf);
  static constexpr std::size_t wire_size = 36;

  constexpr auto stock_locate() const -> std::uint16_t;
  constexpr auto tracking_number() const -> std::uint16_t;
  constexpr auto timestamp() const -> std::uint64_t;
  constexpr auto order_ref() const -> std::uint64_t;
  constexpr auto side() const -> char;
  constexpr auto shares() const -> std::uint32_t;
  constexpr auto stock() const -> std::string_view;
  constexpr auto price() const -> std::uint32_t;
};
```

#### Validation Levels

View types accept a template parameter controlling how much validation is
performed at construction time:

```cpp
// full (default): validates buffer size + discriminant + field constraints
AddOrder_view<xb::wire::validation_level::full> view(buf);

// structural: validates buffer size only (no discriminant check)
AddOrder_view<xb::wire::validation_level::structural> view(buf);

// discriminant: no validation — fastest, trust the caller
AddOrder_view<xb::wire::validation_level::discriminant> view(buf);
```

Use `full` for untrusted input, `discriminant` for hot-path decoding when
the buffer is known-good. See `examples/bes-market-data/` for a working
example.

#### CMake Integration

xb provides two CMake functions for BES code generation:

**`xb_add_library`** (recommended) — generates types and links the xb
runtime in one call:

```cmake
xb_add_library(
  TARGET my_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/my_protocol.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

# Just link — includes and runtime are propagated transitively
target_link_libraries(my_app PRIVATE my_types)
```

**`xb_generate_cpp`** — lower-level, for when you need more control:

```cmake
xb_generate_cpp(
  TARGET my_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/my_protocol.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
  MODE HEADER_ONLY
  BINARY_ONLY
  VALIDATION_LEVEL full
  XSD_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/my_protocol.xsd)

target_link_libraries(my_app PRIVATE my_types xb::header)
```

Both functions accept `ENCODING` + `BINARY_ONLY` for BES-only mode (no
`SCHEMAS` required), or `SCHEMAS` + `ENCODING` together when the BES
overlays an existing XSD schema.

#### BES Examples

Three progressive examples are provided in `examples/`:

| Example | Demonstrates |
|---------|-------------|
| `examples/bes-hello/` | Minimal single-message BES — owned construction, view readback, wire_size |
| `examples/bes-market-data/` | Multiple message types, discriminant dispatch, designated initializers, validation levels |
| `examples/bes-protocol-stack/` | Multi-layer protocol headers (Ethernet/IPv4/UDP), zero-copy layer-by-layer parsing |
| `examples/bes-variable-length/` | Length-prefixed message stream with different-sized messages, stream parsing |

Build them with `cmake -Dxb_BUILD_EXAMPLES=ON` (on by default for
top-level builds).

### Built-in XSD Types

Arbitrary-precision `xb::decimal` and `xb::integer`, plus `xb::duration`,
`xb::date`, `xb::time`, `xb::date_time`, `xb::qname`, and all bounded
numeric types mapped to fixed-width C++ types per the XSD specification.

## Requirements

- C++20 compiler (GCC 12+ or Clang 16+)
- CMake 3.21+
- Ninja build system
- [Expat](https://libexpat.github.io/) XML parser

Optional:

- libcurl — enables HTTP transport and MTOM
- OpenSSL — enables WS-Security cryptographic operations

## Building

```sh
cmake --preset <preset-name>
cmake --build build --config Release
```

Available presets: `gcc-12`, `gcc-13`, `clang-16`, `clang-17`, `clang-18`,
`clang-20` (defined in `CMakeUserPresets.json`).

## Testing

```sh
ctest --test-dir build -C Release --output-on-failure
```

## Installation

```sh
cmake --install build --config Release --prefix /usr/local
```

Once installed, downstream projects can use xb via CMake:

```cmake
find_package(xb REQUIRED)
target_link_libraries(my_target PRIVATE xb::header)
```

## Project Layout

```
src/
  include/xb/    Public headers
  lib/           Library sources
  bin/           CLI tool sources
test/
  unit/          Unit tests
  feature/       Feature/integration tests
examples/        BES and schema usage examples
cmake/           CMake dependency configuration
scripts/         Utility scripts
```

## License

TBD
