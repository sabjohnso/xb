# Installation

## Requirements

### Required

- **C++20 compiler** — GCC 12+ or Clang 16+
- **CMake 3.21+**
- **Ninja** build system
- **Expat** XML parser library

### Optional

- **libcurl** — enables HTTP client transport for WSDL services and MTOM
- **OpenSSL** — enables WS-Security cryptographic operations (password digest,
  HMAC, X.509)
- **POSIX sockets** (Linux, macOS) — enables the built-in HTTP listener for
  hosting SOAP services (automatically detected, no extra install needed)

## Building from Source

### 1. Clone the repository

```sh
git clone https://github.com/sbj/xb.git
cd xb
```

### 2. Configure

xb ships a default CMake preset using Ninja Multi-Config. To configure with
the default preset:

```sh
cmake --preset default
```

Or configure manually to select a specific compiler:

```sh
cmake -G "Ninja Multi-Config" -B build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -pedantic -Werror" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

You can also create your own `CMakeUserPresets.json` with compiler-specific
presets that inherit from `default`.

!!! warning "Compiler restrictions"
    - Do not use GCC with `RelWithDebInfo` (sanitizers are broken).
    - Do not use Clang 15 with `RelWithDebInfo`.
    - Clang 11 is not supported.

### 3. Build

```sh
cmake --build build --config Release
```

### 4. Test

```sh
ctest --test-dir build -C Release --output-on-failure
```

### 5. Install

```sh
cmake --install build --config Release --prefix /usr/local
```

## Using xb in Your Project

xb can be consumed either as an installed package or as a CMake subproject.

### As an Installed Package

After running `cmake --install` (step 5 above), use `find_package`:

```cmake
find_package(xb REQUIRED)
target_link_libraries(my_target PRIVATE xb::xb)
```

### As a CMake Subproject

Add xb to your source tree (e.g., as a git submodule or via FetchContent)
and include it with `add_subdirectory`:

```cmake
add_subdirectory(third_party/xb)
target_link_libraries(my_target PRIVATE xb::xb)
```

Or using FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(xb
  GIT_REPOSITORY https://github.com/sbj/xb.git
  GIT_TAG        v0.16.0)
FetchContent_MakeAvailable(xb)

target_link_libraries(my_target PRIVATE xb::xb)
```

When used as a subproject, xb's tests and examples are automatically
disabled (they only build when xb is the top-level project).

### Library Targets

Both approaches expose the same targets:

- **`xb::xb`** — the full static library including parsers, code generation,
  schema conversion, XML I/O, SOAP/WSDL, and all runtime functionality.
- **`xb::header`** — an interface (header-only) library that provides only
  the type definitions (`xb::decimal`, `xb::date`, `xb::qname`,
  `any_element`, etc.) without compiled implementations. Primarily useful for
  targets that consume generated code and only need the xb value types, not
  the parsing or code generation machinery.

See [CMake Integration](cmake-integration.md) for code generation integration.
