# xb

XML databinding for C++20 — serialization, deserialization, and C++ code
generation from [XML Schema 1.1](https://www.w3.org/TR/xmlschema11-1/)
definitions.

## Overview

xb generates C++ types and serialization code from XML Schema 1.1
definitions, providing a type-safe interface for reading and writing XML
documents. It is an alternative to
[Code Synthesis XSD](https://www.codesynthesis.com/products/xsd/) with
different design decisions, notably the use of `std::variant` for
XSD choice groups instead of pointer-based discriminated unions and a
focus on generating types suitable for low-latency programming. Future
goals include WSDL and SOAP support.

## Requirements

- C++20 compiler (GCC 10+ or Clang 11+)
- CMake 3.12+
- Ninja build system

## Building

```sh
cmake --preset <preset-name>
cmake --build build --config Release
```

Available presets are defined in `CMakeUserPresets.json` for various GCC and
Clang versions.

## Testing

```sh
ctest --test-dir build -C Release
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
  bin/           Executable sources
test/
  unit/          Unit tests
  feature/       Feature tests
examples/        Usage examples
cmake/           CMake dependency configuration
scripts/         Utility scripts
```

## License

TBD
