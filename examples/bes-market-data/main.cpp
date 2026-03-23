/// BES Market Data — multiple message types, discriminant dispatch,
/// designated initializers, and validation levels.
///
/// Demonstrates:
///   - Multiple message types from a single BES file
///   - Designated-initializer factory for concise construction
///   - Discriminant-based message dispatch from a raw buffer
///   - Validation levels: full, structural, discriminant

#include <wire_types.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

using namespace market;

namespace {

  /// Dispatch a raw message buffer by its first-byte discriminant.
  void
  dispatch_message(std::span<const std::byte> buf) {
    auto msg_type = static_cast<std::uint8_t>(buf[0]);

    switch (msg_type) {
      case 0x41: { // AddOrder
        AddOrder_view view(buf);
        std::cout << "AddOrder: " << view.symbol() << " " << view.side() << " "
                  << view.quantity() << " @ " << view.price()
                  << "  (order_id=" << view.order_id() << ")\n";
        break;
      }
      case 0x58: { // CancelOrder
        CancelOrder_view view(buf);
        std::cout << "CancelOrder: order_id=" << view.order_id()
                  << " qty=" << view.quantity() << '\n';
        break;
      }
      case 0x50: { // Trade
        Trade_view view(buf);
        std::cout << "Trade: " << view.symbol() << " " << view.side() << " "
                  << view.quantity() << " @ " << view.price()
                  << "  (match_id=" << view.match_id() << ")\n";
        break;
      }
      default:
        std::cout << "Unknown message type: 0x" << std::hex
                  << static_cast<int>(msg_type) << std::dec << '\n';
    }
  }

} // namespace

auto
main() -> int {
  // --- Construct messages using designated initializers ---

  auto order = AddOrder_owned({
      .order_id = 1001,
      .side = "B",
      .quantity = 500,
      .symbol = "AAPL",
      .price = 17500,
  });

  auto cancel = CancelOrder_owned({
      .order_id = 1001,
      .quantity = 200,
  });

  auto trade = Trade_owned({
      .order_id = 1001,
      .side = "B",
      .quantity = 300,
      .symbol = "AAPL",
      .price = 17500,
      .match_id = 9999,
  });

  // --- Simulate a message stream ---

  std::cout << "=== Message Stream ===\n";
  dispatch_message(order.buffer());
  dispatch_message(cancel.buffer());
  dispatch_message(trade.buffer());

  // --- Demonstrate validation levels ---

  std::cout << "\n=== Validation Levels ===\n";

  auto buf = order.buffer();

  // full: validates size + discriminant + field constraints (default)
  AddOrder_view<xb::wire::validation_level::full> full_view(buf);
  std::cout << "full:          price=" << full_view.price() << '\n';

  // structural: validates size only (no discriminant check)
  AddOrder_view<xb::wire::validation_level::structural> struct_view(buf);
  std::cout << "structural:    price=" << struct_view.price() << '\n';

  // discriminant: no validation at all — fastest, trust the caller
  AddOrder_view<xb::wire::validation_level::discriminant> disc_view(buf);
  std::cout << "discriminant:  price=" << disc_view.price() << '\n';

  // --- Wire sizes are compile-time constants ---

  std::cout << "\n=== Wire Sizes ===\n"
            << "AddOrder:     " << AddOrder_owned::wire_size << " bytes\n"
            << "CancelOrder:  " << CancelOrder_owned::wire_size << " bytes\n"
            << "Trade:        " << Trade_owned::wire_size << " bytes\n";

  return 0;
}
