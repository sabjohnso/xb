# xb — XML Databinding & Binary Encoding for C++

## Compiler Constraints

- Do not build for gcc with the RelWithDebInfo config (sanitizers broken).
- Do not use clang-15 with RelWithDebInfo.
- Do not use clang-11.

## Project Overview

xb is a C++20 library and CLI tool for:

1. **XML databinding** — parse XML Schema 1.1 (XSD), RELAX NG, RNC, DTD,
   and Schematron definitions; generate C++ types with serialization and
   deserialization.
2. **Binary wire encoding** — given an XSD data model and a Binary Encoding
   Specification (BES), generate zero-copy C++ view and owned types for
   arbitrary binary protocols (ITCH, CBOR, MessagePack, SBE-style, etc.).
3. **SOAP/WSDL** — SOAP 1.1/1.2 runtime, WSDL 1.1 and 2.0 parsing, service
   client/server code generation, WS-Addressing, WS-Security, MTOM/XOP.

Current version: **0.16.0**

## Key Design Decisions

- **`std::variant` for choice groups**: XSD choice, unions, substitution
  groups, and conditional type assignment all map to `std::variant`.
  Compile-time exhaustiveness via `std::visit`.
- **Low-latency friendly**: Value types, minimal heap allocation,
  cache-friendly layouts, zero-copy binary views with `constexpr` accessors.
- **Standards-conformant defaults**: `xs:decimal` → `xb::decimal` (exact),
  `xs:integer` → `xb::integer` (arbitrary-precision). Bounded types use
  fixed-width C++ types per XSD spec.
- **Configurable type map**: All XSD-to-C++ mappings overridable via XML
  config document (validated by `xb-typemap.xsd`).
- **XSD is the data model, BES is the encoding**: Same XSD type can have
  XML serialization and multiple binary encodings. Analogous to ASN.1 + ACN.

## Obstacle Check (Mandatory)

Before starting any implementation task, consult `OBSTACLES.org`. If the
task touches any issue listed there, **stop and re-examine the plan** before
proceeding. Each obstacle entry has "Affected tasks" and tags.

## Build System

- **CMake 3.12+** with **Ninja Multi-Config** generator
- C++20 standard (configurable via `xb_CXX_STANDARD`)
- Compiler flags: `-Wall -Wextra -pedantic -Werror`
- Sanitizers in RelWithDebInfo: `-fsanitize=undefined -fsanitize=address`
- Primary build preset: `build-clang-20` (clang-20, Release)

### Building

```sh
cmake --preset <preset-name>
cmake --build build-clang-20 --config Release
```

### Testing

```sh
cmake --build build-clang-20 --config Release
ctest --test-dir build-clang-20 -C Release --output-on-failure
```

Targeted build/test:
```sh
cmake --build build-clang-20 --config Release --target <target>
ctest --test-dir build-clang-20 -C Release -R <pattern> --output-on-failure
```

Tests are enabled by default (`xb_BUILD_TESTING=ON`).

## Project Layout

```
src/
  include/xb/          # Public headers (installed)
  include/xb/wire/     # Binary encoding headers (generated + hand-written)
  lib/                  # Library sources
  bin/                  # CLI executable sources
test/
  unit/                 # Unit tests (Catch2)
  feature/              # Feature/integration tests
  integration/          # End-to-end integration tests (FIXML, UBL, DocBook)
schema/                 # XSD schemas shipped with xb (xb-typemap.xsd, xb-encoding.xsd, etc.)
examples/               # Usage examples
cmake/                  # CMake dependency configuration
cmake_utilities/        # Git submodule for CMake utilities
scripts/                # Utility scripts (formatting, etc.)
```

## Key CMake Targets

| Target         | Alias        | Description                                |
|----------------|--------------|--------------------------------------------|
| `xb_header`    | `xb::header` | Interface (header-only) library            |
| `xb`           | `xb::xb`     | Static library (runtime, parsers, codegen) |
| `xb_cli`       | `xb::cli`    | CLI executable                             |
| `xb_bootstrap` | —            | Stage-1 CLI (without BES-generated types)  |

## CLI Subcommands

| Subcommand     | Description                                      |
|----------------|--------------------------------------------------|
| `generate`     | Generate C++ types and serialization from XSD    |
| `generate-xsd` | Generate XSD from a BES file (synthetic schema)  |
| `sample-doc`   | Generate a sample XML document from a schema     |
| `fetch`        | Fetch XSD schemas and transitive dependencies    |
| `convert`      | Convert between schema formats (XSD/DTD/RNG/RNC) |

## Namespaces

| Namespace         | Purpose                                                     |
|-------------------|-------------------------------------------------------------|
| `xb::`            | Core: types, schema model, codegen, XML I/O                 |
| `xb::rng`         | RELAX NG pattern IR, parser, simplifier, translator         |
| `xb::dtd`         | DTD parser and model                                        |
| `xb::schematron`  | Schematron rule overlay                                     |
| `xb::soap`        | SOAP 1.1/1.2 envelope, fault, header                        |
| `xb::wsdl`        | WSDL 1.1 model and parser                                   |
| `xb::wsdl2`       | WSDL 2.0 model and parser                                   |
| `xb::service`     | Version-independent service IR (resolved from WSDL)         |
| `xb::wsa`         | WS-Addressing                                               |
| `xb::wss`         | WS-Security (UsernameToken, Timestamp, BinarySecurityToken) |
| `xb::wss::crypto` | Optional OpenSSL crypto (password digest, HMAC)             |
| `xb::xop`         | XML-binary Optimized Packaging (MTOM/XOP)                   |
| `xb::mime`        | MIME multipart support                                      |
| `xb::wire`        | Binary encoding types (generated from BES)                  |

## Schema Format Support

| Format          | Read          | Codegen                       |
|-----------------|---------------|-------------------------------|
| XSD 1.1         | Yes           | Yes (C++ types + ser/deser)   |
| RELAX NG (.rng) | Yes           | Yes (translated to XSD model) |
| RNC (.rnc)      | Yes           | Yes (translated to XSD model) |
| DTD             | Yes           | Yes (translated to XSD model) |
| Schematron      | Yes (overlay) | Yes (validation assertions)   |
| BES             | Yes           | Yes (binary view/owned types) |

`xb convert` converts between any pair of XSD, RNG, RNC, and DTD:

| Input ↓ · Output → | XSD | RNG | RNC | DTD |
|---------------------|-----|-----|-----|-----|
| XSD 1.1             | ·   | Yes | Yes | Yes |
| RELAX NG (.rng)     | Yes | ·   | Yes | Yes |
| RNC (.rnc)          | Yes | Yes | ·   | Yes |
| DTD                 | Yes | Yes | Yes | ·   |

## Binary Encoding Specification (BES)

BES is xb's answer to ASN.1's ACN: a separate document that maps XSD types
to binary wire layouts. Key capabilities:

- Bit-level field widths, per-field endianness, alignment, padding
- Protocol framing with composable frame stacks (Ethernet → IP → UDP → app)
- Discriminated choices, counted/delimited repeats, self-delimiting messages
- Computed fields (CRC, checksum) with byte-range specification
- Wire-only fields, length determinants, custom transforms
- CSS-like selector system for mapping XSD components to encodings
- Zero-copy `binary_view` types with `constexpr` accessors
- Three validation levels: full, structural, discriminant-only

BES schema: `schema/xb-encoding.xsd`. See `ACNComparison.org` for a
detailed feature comparison with ASN.1 + ACN.

## Conventions

- Code formatting: `.clang-format` (LLVM-based, 100-column limit)
- Run `scripts/check-format.sh` to reformat headers
- Namespace: `xb::` (config lives in `xb::Config`)
- CMake variables prefixed with `xb_`
- Export name pattern: `xb::` namespace
- Tests use Catch2 (REQUIRE/CHECK macros)
- XML parsing: `expat_reader` (streaming, forward-only)

## Files Not Tracked in Git

- `build*/` directories
- `PLAN.org`, `ISSUES.org`, `OBSTACLES.org`, `ACNComparison.org` (local planning files)
- Emacs backup files (`*~`, `#*#`)
