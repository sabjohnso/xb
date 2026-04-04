# CMake Integration

xb provides two CMake functions for integrating code generation into your
build.

## `xb_add_library` (Recommended)

The high-level function that generates types and creates a library target in
one call:

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  MODE HEADER_ONLY)

target_link_libraries(my_app PRIVATE my_types)
```

The runtime library (`xb::xb` or `xb::header`) is linked transitively — your
consuming target only needs to link `my_types`.

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `TARGET`  | Yes | Name of the library target to create |
| `SCHEMAS` | No* | XSD schema files to process |
| `ENCODING` | No* | BES file for binary encoding |
| `MODE` | No | `HEADER_ONLY` (single .hpp), `FILE_PER_TYPE`, or `SPLIT` (default) |
| `BINARY_ONLY` | No | Suppress XML type generation (BES-only mode) |
| `TYPE_MAP` | No | Custom type mapping file |
| `NAMESPACE_MAP` | No | Namespace URI-to-C++ mappings |
| `VALIDATION_LEVEL` | No | Binary type validation: `full`, `structural`, `discriminant` |
| `TYPE_STYLE` | No | Naming style for types |
| `FIELD_STYLE` | No | Naming style for fields |
| `ENUM_STYLE` | No | Naming style for enum values |

*At least one of `SCHEMAS` or `ENCODING` is required.

### Output Modes

=== "Header Only"

    Generates a single `.hpp` file with all types inline:

    ```cmake
    xb_add_library(
      TARGET my_types
      SCHEMAS schema.xsd
      MODE HEADER_ONLY)
    ```

=== "File Per Type"

    Generates one header per type, plus a combined include header:

    ```cmake
    xb_add_library(
      TARGET my_types
      SCHEMAS schema.xsd
      MODE FILE_PER_TYPE)
    ```

=== "Split (Default)"

    Generates separate `.hpp` and `.cpp` files:

    ```cmake
    xb_add_library(
      TARGET my_types
      SCHEMAS schema.xsd
      MODE SPLIT)
    ```

### XML + Binary Encoding

When you have both an XSD schema and a BES file:

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/encoding.bes.xml)
```

This generates both XML serialization types and binary view/owned types.

### BES-Only Mode

When the binary format is the specification and no separate XSD exists:

```cmake
xb_add_library(
  TARGET my_types
  ENCODING ${CMAKE_CURRENT_SOURCE_DIR}/protocol.bes.xml
  MODE HEADER_ONLY
  BINARY_ONLY)
```

xb infers XSD types from field properties and generates a synthetic schema
internally.

## `xb_generate_cpp` (Low-Level)

For more control over the generation process:

```cmake
xb_generate_cpp(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
  MODE HEADER_ONLY)

# You manage linking yourself
target_include_directories(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)
target_link_libraries(my_app PRIVATE my_types xb::header)
```

### Additional Parameters

| Parameter | Description |
|-----------|-------------|
| `OUTPUT_DIR` | Where to write generated files |
| `XSD_OUTPUT` | Write the synthetic XSD to this path (BES-only mode) |

## Namespace Mapping

Map XML namespace URIs to C++ namespaces:

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS schema.xsd
  NAMESPACE_MAP
    "http://example.com/orders=orders"
    "http://example.com/common=common")
```

Without explicit mapping, xb derives C++ namespace names from the last URI
path segment (configurable via `--namespace-style`).

## Custom Type Mapping

Override the default XSD-to-C++ type mappings:

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS schema.xsd
  TYPE_MAP ${CMAKE_CURRENT_SOURCE_DIR}/my-typemap.xml)
```

See [Type Mapping](../guides/type-mapping.md) for the typemap file format.

## Examples

The `examples/` directory contains complete CMake projects demonstrating
various configurations:

| Example | Configuration |
|---------|--------------|
| `xsd-addressbook/` | XSD code generation, XML serialization round-trip |
| `soap-envelope/` | SOAP 1.2 envelope, headers, pipeline, fault detection |
| `wsdl-client/` | WSDL-style client with mock transport |
| `bes-hello/` | Minimal BES, `HEADER_ONLY`, `BINARY_ONLY` |
| `bes-market-data/` | Multiple message types, discriminant dispatch |
| `bes-protocol-stack/` | Protocol framing with frame stacks |
| `bes-variable-length/` | Length-prefixed message streams |

Build examples with:

```sh
cmake -Dxb_BUILD_EXAMPLES=ON --preset default
cmake --build build --config Release
```
