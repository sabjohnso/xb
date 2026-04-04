# Architecture

## High-Level Overview

![xb architecture overview](../images/architecture-light.svg#only-light)
![xb architecture overview](../images/architecture-dark.svg#only-dark)

## Layers

### XML I/O Layer

The foundation is a streaming XML interface:

- **`xml_reader`** — abstract base class for forward-only XML event reading
  (`start_element`, `end_element`, `characters`)
- **`expat_reader`** — concrete implementation using the Expat library
- **`xinclude_reader`** — decorator that transparently resolves XInclude
  directives
- **`xml_writer`** — abstract base class for XML output
- **`ostream_writer`** — concrete writer to `std::ostream`
- **`any_element`** — flexible DOM-like tree for elements that don't have a
  fixed schema (SOAP headers, extensibility points)

### Schema Parsers

Each schema format has a dedicated parser that produces a format-specific IR:

| Parser | Input | Output |
|--------|-------|--------|
| `schema_parser` | XSD 1.1 | `schema` (the canonical IR) |
| `rng_xml_parser` | RELAX NG XML | `rng::pattern` → translated to `schema` |
| `rng_compact_parser` | RNC | `rng::pattern` → translated to `schema` |
| `dtd_parser` | DTD | `dtd::document` → translated to `schema` |
| `schematron_parser` | Schematron | `schematron::schema` (overlay) |
| `wsdl_parser` | WSDL 1.1 | `wsdl::document` |
| `wsdl2_parser` | WSDL 2.0 | `wsdl2::description` |

RELAX NG, RNC, and DTD are translated to the XSD internal model so that
a single code generation pipeline handles all formats.

### Resolvers

Resolvers take parsed models and produce resolved, ready-to-generate output:

- **`wsdl_resolver`** — resolves `wsdl::document` → `service::service_description`
- **`wsdl2_resolver`** — resolves `wsdl2::description` → `service::service_description`
  (same target IR, enabling version-independent codegen)
- **`bes_resolver`** — resolves BES encoding definitions against XSD types

### Code Generation

Multiple code generators target different outputs:

| Generator | Input | Output |
|-----------|-------|--------|
| `codegen` | `schema_set` + `type_map` | `cpp_file` IR → C++ headers/sources |
| `wsdl_codegen` | `service_description` | Client stubs + server skeletons |
| `binary_codegen` | BES + XSD | `_view`, `_mutable_view`, `_owned` types |
| `xsd_writer` | `schema` | XSD XML output |
| `rng_writer` | `schema` | RELAX NG XML output |
| `dtd_writer` | `schema` | DTD output |
| `doc_generator` | `schema` | Sample XML documents |
| `railroad` | `schema` | SVG railroad diagrams |

### SOAP / Web Services Runtime

A separate subsystem for SOAP messaging:

- **`xb::soap`** — envelope, header, fault model and serialization
- **`xb::wsa`** — WS-Addressing headers
- **`xb::wss`** — WS-Security tokens and crypto
- **`xb::xop`** / **`xb::mime`** — MTOM/XOP binary optimization
- **`xb::service`** — version-independent service IR and transport abstraction

## Namespaces

| Namespace | Purpose |
|-----------|---------|
| `xb` | Core: types, schema model, codegen, XML I/O |
| `xb::rng` | RELAX NG pattern IR, parser, simplifier, translator |
| `xb::dtd` | DTD parser and model |
| `xb::schematron` | Schematron rule overlay |
| `xb::soap` | SOAP 1.1/1.2 envelope, fault, header |
| `xb::wsdl` | WSDL 1.1 model and parser |
| `xb::wsdl2` | WSDL 2.0 model and parser |
| `xb::service` | Version-independent service IR |
| `xb::wsa` | WS-Addressing |
| `xb::wss` | WS-Security |
| `xb::wss::crypto` | OpenSSL cryptographic operations |
| `xb::xop` | XML-binary Optimized Packaging |
| `xb::mime` | MIME multipart support |
| `xb::wire` | Binary encoding types |
| `xb::bes` | Binary encoding specification model |
| `xb::railroad` | Railroad diagram IR and SVG rendering |

## Project Layout

```
src/
  include/xb/          Public headers (installed)
  include/xb/wire/     Binary encoding headers
  lib/                  Library sources
  bin/                  CLI executable sources
test/
  unit/                 Unit tests (Catch2)
  feature/              Feature/integration tests
  integration/          End-to-end integration tests
schema/                 XSD schemas shipped with xb
examples/               Usage examples
cmake/                  CMake dependency configuration
cmake_utilities/        CMake utility submodule
scripts/                Utility scripts
```

## Build Targets

| Target | Alias | Description |
|--------|-------|-------------|
| `xb_header` | `xb::header` | Interface (header-only) library |
| `xb` | `xb::xb` | Static library (runtime, parsers, codegen) |
| `xb_cli` | `xb::cli` | CLI executable |
| `xb_bootstrap` | -- | Stage-1 CLI (without BES-generated types) |

The bootstrap target exists because xb uses its own BES code generation to
build wire types. `xb_bootstrap` is a minimal build that can generate those
types, which are then compiled into the full `xb` target.
