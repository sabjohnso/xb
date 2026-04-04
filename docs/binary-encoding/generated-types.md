# Generated Types

For each BES message, xb generates three C++ class types that provide
different trade-offs between convenience and performance.

## `Message_view`

A read-only, zero-copy view over an existing buffer:

```cpp
class Heartbeat_view {
public:
    explicit constexpr Heartbeat_view(std::span<const std::byte> buf);

    static constexpr std::size_t wire_size = 13;

    constexpr auto msg_type() const -> std::uint8_t;
    constexpr auto sequence() const -> std::uint32_t;
    constexpr auto timestamp_ns() const -> std::uint64_t;
};
```

**Characteristics:**

- No memory allocation — reads directly from the provided buffer
- All accessors are `constexpr` — can be used at compile time
- Handles byte-swapping and bit extraction inline
- Validates the buffer at construction time (configurable via
  [validation levels](validation-levels.md))

**Usage:**

```cpp
// View over a network buffer
std::span<const std::byte> buffer = receive_packet();
Heartbeat_view view(buffer);

auto seq = view.sequence();           // byte-swapped uint32_t
auto ts  = view.timestamp_ns();       // byte-swapped uint64_t
```

## `Message_mutable_view`

A mutable view for in-place field modification:

```cpp
class Heartbeat_mutable_view {
public:
    explicit constexpr Heartbeat_mutable_view(std::span<std::byte> buf);

    constexpr void set_sequence(std::uint32_t v);
    constexpr void set_timestamp_ns(std::uint64_t v);

    // Also has all read accessors from _view
    constexpr auto sequence() const -> std::uint32_t;
};
```

**Usage:**

```cpp
std::array<std::byte, Heartbeat_view::wire_size> buffer{};
Heartbeat_mutable_view mv(buffer);
mv.set_sequence(42);
mv.set_timestamp_ns(now());
```

## `Message_owned`

A self-contained type that manages its own buffer:

```cpp
class Heartbeat_owned {
public:
    // Aggregate constructor (wire-fields auto-populated)
    Heartbeat_owned(std::uint32_t sequence, std::uint64_t timestamp_ns);

    // Access the underlying buffer
    auto buffer() const -> std::span<const std::byte>;
    auto buffer() -> std::span<std::byte>;

    // Field accessors (delegates to view)
    constexpr auto sequence() const -> std::uint32_t;
    constexpr auto timestamp_ns() const -> std::uint64_t;
};
```

**Characteristics:**

- Owns a `std::array<std::byte, wire_size>` buffer
- Constructor takes only data fields — wire-fields (like `msg_type`) are
  set automatically from the message's `discriminant-value`
- Converts implicitly to `_view` and `_mutable_view`

**Usage:**

```cpp
// Construct with data field values
Heartbeat_owned hb(42, 1'700'000'000'000'000'000ULL);

// Wire-field set automatically
assert(hb.msg_type() == 0x01);

// Get a view for passing to functions
Heartbeat_view view(hb.buffer());

// Get the raw bytes for sending
send_packet(hb.buffer());
```

## Wire Fields

Wire-only fields (`<wire-field>`) exist on the wire but not in the XSD type.
They are:

- **Readable** on all view types via named accessors
- **Not constructor parameters** on `_owned` — they are auto-populated from
  `discriminant-value` or defaults
- Common uses: message type tags, protocol version bytes, reserved/padding
  fields

```xml
<message name="Alert" discriminant-value="0x50">
  <wire-field name="msg_type" bits="8"/>    <!-- auto-set to 0x50 -->
  <wire-field name="version" bits="8"/>     <!-- default: 0 -->
  <field name="alert_id" bits="64" encoding="unsigned"/>
</message>
```

```cpp
// Only data fields in the constructor
Alert_owned alert(12345ULL);

alert.msg_type();  // 0x50 (from discriminant-value)
alert.version();   // 0 (default)
alert.alert_id();  // 12345
```

## Compile-Time Wire Size

All view types expose a `static constexpr std::size_t wire_size` that gives
the total message size in bytes at compile time:

```cpp
static_assert(Heartbeat_view::wire_size == 13);
static_assert(SensorReading_view::wire_size == 22);

// Use for buffer allocation
std::array<std::byte, Heartbeat_view::wire_size> buffer{};
```

## Converting Between Types

```cpp
// owned → view
Heartbeat_owned owned(42, ts);
Heartbeat_view view(owned.buffer());

// owned → mutable_view
Heartbeat_mutable_view mv(owned.buffer());
mv.set_sequence(43);

// raw buffer → view
auto view2 = Heartbeat_view(raw_buffer);
```
