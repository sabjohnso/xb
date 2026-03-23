/// BES Hello World — construct and read a binary Heartbeat message.
///
/// Demonstrates the three generated types for a BES message:
///   _owned  — self-contained, manages its own buffer
///   _view   — zero-copy read-only view over an existing buffer
///   wire_size — compile-time constant for the message size in bytes

#include <wire_types.hpp>

#include <cstdint>
#include <iostream>

auto
main() -> int {
  using namespace hello;

  // 1. Construct an owned message using the aggregate constructor.
  //    The wire-field msg_type is auto-set from the discriminant-value.
  Heartbeat_owned hb(42, 1'700'000'000'000'000'000ULL);

  // 2. Create a read-only view over the same buffer.
  Heartbeat_view view(hb.buffer());

  // 3. Read fields back through the view.
  std::cout << "msg_type:     0x" << std::hex
            << static_cast<int>(view.msg_type()) << '\n'
            << std::dec << "sequence:     " << view.sequence() << '\n'
            << "timestamp_ns: " << view.timestamp_ns() << '\n'
            << "wire_size:    " << Heartbeat_owned::wire_size << " bytes\n";

  return 0;
}
