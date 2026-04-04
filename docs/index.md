# xb

**XML databinding and binary encoding for C++20.**

xb generates C++ types and serialization code from XML Schema definitions,
providing a type-safe interface for reading and writing XML documents. It also
includes a declarative Binary Encoding Specification (BES) format for mapping
XSD data types to wire-level binary layouts.

---

## What xb Does

<div class="grid cards" markdown>

-   **XML Schema Code Generation**

    Parse XSD, RELAX NG, RNC, or DTD schemas and generate C++ structs, enums,
    and serialization routines with full round-trip fidelity.

    [Quick Start &rarr;](getting-started/quick-start.md)

-   **Binary Encoding**

    Map XSD types to wire-level binary layouts with zero-copy views, bit-level
    field widths, and protocol framing — targeting binary protocols,
    telemetry streams, and low-latency messaging.

    [BES Overview &rarr;](binary-encoding/overview.md)

-   **Web Services**

    Full WSDL 1.1/2.0 parsing with SOAP 1.1/1.2 runtime, WS-Addressing,
    WS-Security, and MTOM/XOP support. Generate client stubs and server
    skeletons from WSDL definitions.

    [Web Services &rarr;](web-services/soap.md)

-   **Schema Conversion**

    Convert freely between XSD, RELAX NG, RNC, and DTD formats. Visualize
    content models as railroad diagrams.

    [Conversion Guide &rarr;](guides/schema-conversion.md)

</div>

---

## Key Features

### Schema Support

| Format          | Read | Code Generation |
|-----------------|------|-----------------|
| XML Schema 1.1  | Yes  | C++ types + serialization/deserialization |
| RELAX NG (.rng) | Yes  | Via translation to XSD model |
| RNC (.rnc)      | Yes  | Via translation to XSD model |
| DTD             | Yes  | Via translation to XSD model |
| Schematron      | Yes  | Validation assertions (overlay) |
| BES             | Yes  | Binary view/owned types |

### Schema Conversion

Any pair of XSD, RELAX NG, RNC, and DTD can be converted:

| Input | XSD | RNG | RNC | DTD |
|-------|-----|-----|-----|-----|
| XSD   | --  | Yes | Yes | Yes |
| RNG   | Yes | --  | Yes | Yes |
| RNC   | Yes | Yes | --  | Yes |
| DTD   | Yes | Yes | Yes | --  |

### Design Highlights

- **`std::variant` for choice groups** — compile-time exhaustiveness via
  `std::visit` instead of pointer-based discriminated unions
- **Low-latency friendly** — value types, minimal heap allocation,
  cache-friendly layouts, zero-copy binary views with `constexpr` accessors
- **Standards-conformant defaults** — `xs:decimal` maps to `xb::decimal`
  (exact, arbitrary-precision), `xs:integer` to `xb::integer`
- **Configurable type mapping** — all XSD-to-C++ mappings overridable via XML
  configuration document

---

## Quick Example

Given an XSD schema, xb generates C++ types you can use immediately:

```cpp
#include <generated_types.hpp>

// Deserialize from XML
auto reader = xb::expat_reader(input_stream);
auto order = purchase_order::read(reader);

// Access typed fields
std::cout << order.ship_to.name << "\n";
std::cout << order.items.size() << " items\n";

// Serialize back to XML
auto writer = xb::ostream_writer(output_stream);
purchase_order::write(writer, order);
```

For binary protocols, BES generates zero-copy view types:

```cpp
#include <wire_types.hpp>

// Zero-copy read from a network buffer
SensorReading_view view(buffer);
auto device = view.device_id();  // std::string_view, no copy
auto value  = view.value();      // uint32_t, byte-swapped inline
auto flags  = view.flags();      // uint32_t
```

---

## Getting Started

1. [Install xb](getting-started/installation.md) — requirements and build instructions
2. [Quick Start](getting-started/quick-start.md) — generate your first C++ types from XSD
3. [CMake Integration](getting-started/cmake-integration.md) — integrate xb into your build
