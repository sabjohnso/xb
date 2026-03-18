/// BES backward-compatibility test: no namespace.
///
/// Verifies that when a BES file has no target-namespace, generated types
/// remain in the global namespace (no breaking change for existing users).

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

// Types should be in the global namespace — no "using namespace" needed.

TEST_CASE("BES global namespace: SimpleOrder round-trip", "[bes_global_ns]") {
  SimpleOrder_owned order;
  order.set_stock_locate(42);
  order.set_shares(100);
  order.set_stock("AAPL    ");
  order.set_price(15025);

  SimpleOrder_view view(order.buffer());
  CHECK(view.stock_locate() == 42);
  CHECK(view.shares() == 100);
  CHECK(view.stock() == std::string_view("AAPL    "));
  CHECK(view.price() == 15025);
}
