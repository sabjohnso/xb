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
cmake/           CMake dependency configuration
scripts/         Utility scripts
```

## License

TBD
