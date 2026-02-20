# xb — XML Databinding for C++

## Note
The gcc sanitizers are not working, so don't build for gcc with the
RelWithDebInfo config.

Don't use clang-15 with the RelWithDebInfo config.

Also, don't use the clang-11 compilers.


## Project Overview

xb is a C++20 library for XML databinding: serialization, deserialization,
and C++ code generation from XML Schema 1.1 (XSD) definitions. It is an
alternative to Code Synthesis XSD with different design decisions.

## Key Design Decisions

- **`std::variant` for choice groups**: XSD choice (discriminated unions) are
  represented as `std::variant` rather than pointer-per-alternative as in
  Code Synthesis XSD. This provides value semantics, compile-time exhaustiveness
  checking via `std::visit`, and eliminates nullable-pointer ambiguity.
- **Low-latency friendly types**: Generated types should be suitable for
  low-latency programming — prefer value types, avoid heap allocations on
  the hot path, favor cache-friendly layouts, and minimize indirection.
- **Standards-conformant defaults**: `xs:decimal` maps to `xb::decimal`
  (exact, arbitrary-precision) and `xs:integer` maps to `xb::integer`
  (arbitrary-precision). Bounded types (`xs:long`, `xs:int`, etc.) use
  fixed-width C++ types per the XSD spec.
- **Configurable type map**: All built-in XSD-to-C++ type mappings can be
  overridden via an XML configuration document (validated by `xb-typemap.xsd`).
  Users can substitute any default with a custom C++ type.

## Obstacle Check (Mandatory)

Before starting any implementation task, consult `OBSTACLES.org`. If the
task touches any issue listed there, **stop and re-examine the plan for
that task** before proceeding. Each obstacle entry has an "Affected tasks"
section and tags to help identify overlap.

## Future Goals

- WSDL and SOAP support.

## Build System

- **CMake 3.12+** with **Ninja Multi-Config** generator
- C++20 standard (configurable via `xb_CXX_STANDARD`)
- Compiler flags: `-Wall -Wextra -pedantic -Werror`
- Sanitizers enabled in RelWithDebInfo: `-fsanitize=undefined -fsanitize=address`

### Building

```sh
cmake --preset <preset-name>
cmake --build build --config Release
```

### Testing

```sh
cmake --build build --config Release
ctest --test-dir build -C Release
```

Tests are enabled by default (`xb_BUILD_TESTING=ON`).

## Project Layout

```
src/
  include/xb/       # Public headers (installed)
  lib/               # Library sources
  bin/               # Executable sources
test/
  unit/              # Unit tests
  feature/           # Feature/integration tests
examples/            # Usage examples
cmake/               # CMake dependency configuration
cmake_utilities/     # Git submodule for CMake utilities
scripts/             # Utility scripts (formatting, etc.)
```

## Key CMake Targets

- `xb_header` (alias: `xb::header`) — Interface (header-only) library target

## Conventions

- Code formatting: `.clang-format` (LLVM-based, 100 column limit)
- Run `scripts/check-format.sh` to reformat headers
- Namespace: `xb::` (config lives in `xb::Config`)
- CMake variables are prefixed with `xb_`
- Export name pattern: library targets use `xb::` namespace

## Files Not Tracked in Git

- `build*/` directories
- `PLAN.org`, `ISSUES.org`, `OBSTACLES.org` (local planning files)
- Emacs backup files (`*~`, `#*#`)
