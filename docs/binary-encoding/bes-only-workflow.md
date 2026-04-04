# BES-Only Workflow

BES files can be used without a separate XSD schema. xb infers XSD types from
field properties (width, encoding) and generates a synthetic schema internally.
This is useful when the binary format *is* the specification.

## When to Use BES-Only

- You have a binary protocol spec but no XSD
- The wire format is the source of truth
- You only need binary types, not XML serialization

## CLI Usage

```sh
# Generate binary types only
xb generate --encoding protocol.bes.xml --output-dir out/ --binary-only

# Generate as header-only
xb generate --encoding protocol.bes.xml --output-dir out/ \
    --binary-only --header-only
```

## CMake Usage

```cmake
xb_add_library(
  TARGET protocol_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/protocol.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)

target_link_libraries(my_app PRIVATE protocol_types)
```

Note: no `SCHEMAS` parameter — `ENCODING` + `BINARY_ONLY` is sufficient.

## Exporting the Inferred Schema

You can export the synthetic XSD that xb derives from a BES file:

```sh
xb generate-xsd --encoding protocol.bes.xml --output protocol.xsd
```

The generated XSD contains:

- One `xs:complexType` per message
- Element types derived from field widths and encodings (e.g., 32-bit
  unsigned → `xs:unsignedInt`)
- Facet constraints matching the binary range

This is useful for:

- Documentation — see the logical data model
- Interop — share the schema with non-BES tools
- Dual use — later add XML serialization on top of the same types

### CMake

```cmake
xb_generate_cpp(
  TARGET my_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/protocol.bes.xml
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
  MODE HEADER_ONLY
  BINARY_ONLY
  XSD_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/protocol.xsd)
```

## Type Inference Rules

xb maps BES field properties to XSD types:

| Field Width | Encoding | Inferred XSD Type |
|-------------|----------|-------------------|
| 1 bit | unsigned | `xs:boolean` |
| 8 bits | unsigned | `xs:unsignedByte` |
| 16 bits | unsigned | `xs:unsignedShort` |
| 32 bits | unsigned | `xs:unsignedInt` |
| 64 bits | unsigned | `xs:unsignedLong` |
| 8 bits | twos-complement | `xs:byte` |
| 16 bits | twos-complement | `xs:short` |
| 32 bits | twos-complement | `xs:int` |
| 64 bits | twos-complement | `xs:long` |
| 32 bits | ieee754 | `xs:float` |
| 64 bits | ieee754 | `xs:double` |
| any | ascii / utf-8 | `xs:string` |

## Example

A standalone BES file defining a telemetry protocol:

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/telemetry">
  <defaults byte-order="big-endian"/>

  <message name="SensorReading" discriminant-value="0x41">
    <wire-field name="msg_type" bits="8"/>
    <field name="device_id"  bits="64" encoding="unsigned"/>
    <field name="channel"    bits="8"  encoding="ascii"/>
    <field name="value"      bits="32" encoding="unsigned"/>
    <field name="label"      bits="64" encoding="ascii"/>
    <field name="flags"      bits="32" encoding="unsigned"/>
  </message>

  <message name="ResetDevice" discriminant-value="0x58">
    <wire-field name="msg_type" bits="8"/>
    <field name="device_id"  bits="64" encoding="unsigned"/>
    <field name="reason_code" bits="32" encoding="unsigned"/>
  </message>
</encoding>
```

```cmake
xb_add_library(
  TARGET telemetry_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/telemetry.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)
```

```cpp
#include <wire_types.hpp>
using namespace telemetry;

SensorReading_owned reading(1001, 'A', 500, "TEMP    ", 0);
send(reading.buffer());
```
