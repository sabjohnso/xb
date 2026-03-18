/// BES-only code generation test.
///
/// Verifies that binary view/owned types can be generated from a BES file
/// without providing a separate XSD schema.  The synthetic XSD types are
/// inferred from the BES field definitions.
///
/// When the BES has a target-namespace, generated types are wrapped in a
/// C++ namespace derived from the URI.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

// The ITCH BES has target-namespace="http://example.com/itch"
// → C++ namespace "itch" (short-name style, last URI segment)
using namespace itch;

// ===========================================================================
// AddOrder — round-trip via owned + view
// ===========================================================================

TEST_CASE("BES-only: AddOrder round-trip", "[bes_only]") {
  AddOrder_owned order;
  order.set_stock_locate(42);
  order.set_tracking_number(7);
  order.set_timestamp(1234567890ULL);
  order.set_order_reference(9999ULL);
  order.set_side('B');
  order.set_shares(100);
  order.set_stock("AAPL    ");
  order.set_price(15025);

  AddOrder_view view(order.buffer());
  CHECK(view.stock_locate() == 42);
  CHECK(view.tracking_number() == 7);
  CHECK(view.timestamp() == 1234567890ULL);
  CHECK(view.order_reference() == 9999ULL);
  CHECK(view.side() == 'B');
  CHECK(view.shares() == 100);
  CHECK(view.stock() == std::string_view("AAPL"));
  CHECK(view.price() == 15025);
}

// ===========================================================================
// Trade — round-trip via owned + view
// ===========================================================================

TEST_CASE("BES-only: Trade round-trip", "[bes_only]") {
  Trade_owned trade;
  trade.set_stock_locate(1);
  trade.set_tracking_number(2);
  trade.set_timestamp(5555ULL);
  trade.set_order_reference(8888ULL);
  trade.set_side('S');
  trade.set_shares(200);
  trade.set_stock("MSFT    ");
  trade.set_price(28050);
  trade.set_match_number(12345ULL);

  Trade_view view(trade.buffer());
  CHECK(view.stock_locate() == 1);
  CHECK(view.shares() == 200);
  CHECK(view.stock() == std::string_view("MSFT"));
  CHECK(view.match_number() == 12345ULL);
}

// ===========================================================================
// Wire size constants
// ===========================================================================

TEST_CASE("BES-only: wire sizes are correct", "[bes_only]") {
  CHECK(AddOrder_owned::wire_size == 36);
  CHECK(Trade_owned::wire_size == 44);
}
