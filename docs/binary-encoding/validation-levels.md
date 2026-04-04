# Validation Levels

View types accept a template parameter controlling how much validation is
performed when the view is constructed from a buffer. This lets you trade
safety for speed on hot paths.

## Three Levels

```cpp
namespace xb::wire {
    enum class validation_level {
        full,           // validates everything
        structural,     // validates buffer size only
        discriminant    // no validation (fastest)
    };
}
```

### `full` (Default)

Validates buffer size, discriminant fields, and field constraints:

```cpp
// Implicit — full validation is the default
SensorReading_view view(buffer);

// Explicit
SensorReading_view<xb::wire::validation_level::full> view(buffer);
```

Throws if:

- Buffer is too small for `wire_size`
- Discriminant field doesn't match `discriminant-value`
- Field constraints are violated

**Use for:** untrusted input, network boundaries, debugging.

### `structural`

Validates buffer size only — no discriminant or constraint checks:

```cpp
SensorReading_view<xb::wire::validation_level::structural> view(buffer);
```

Throws if:

- Buffer is too small for `wire_size`

**Use for:** trusted input where you've already dispatched by message type
but want bounds safety.

### `discriminant`

No validation at all:

```cpp
SensorReading_view<xb::wire::validation_level::discriminant> view(buffer);
```

Never throws (undefined behavior if buffer is too small).

**Use for:** hot-path decoding when the buffer is known-good — you've
already validated it upstream or the data comes from a trusted source.

## Choosing a Level

| Scenario | Recommended Level |
|----------|-------------------|
| Parsing untrusted network data | `full` |
| After message dispatch (type already verified) | `structural` |
| Known-good buffer on hot path | `discriminant` |
| Unit tests | `full` |
| Benchmarking | `discriminant` |

## CMake Configuration

Set the default validation level for all generated types:

```cmake
xb_add_library(
  TARGET my_types
  ENCODING protocol.bes.xml
  BINARY_ONLY
  VALIDATION_LEVEL structural)
```

Individual view instantiations can still override:

```cpp
// Override the default for this specific use
SensorReading_view<xb::wire::validation_level::full> safe_view(untrusted_buffer);
```

## CLI Configuration

```sh
xb generate --encoding protocol.bes.xml --validation-level structural --binary-only
```

## Example: Dispatch with Validation

A common pattern is `full` validation at the dispatch point, then
`discriminant` for the specific message type:

```cpp
void handle_message(std::span<const std::byte> buf) {
    if (buf.empty()) return;

    auto msg_type = static_cast<uint8_t>(buf[0]);

    switch (msg_type) {
    case 0x41: {
        // full validation at the boundary
        SensorReading_view<xb::wire::validation_level::full> view(buf);
        process_reading(view);
        break;
    }
    case 0x58: {
        ResetDevice_view<xb::wire::validation_level::full> view(buf);
        process_reset(view);
        break;
    }
    }
}
```
