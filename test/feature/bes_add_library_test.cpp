/// Test that xb_add_library works with BES-only mode.
///
/// Verifies that xb_add_library(ENCODING ... BINARY_ONLY) generates binary
/// view/owned types and links the xb runtime automatically — no manual
/// xb_generate_cpp + target_link_libraries needed.

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

using namespace itch;

TEST_CASE("xb_add_library BES-only: AddOrder round-trip", "[bes_add_library]") {
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

TEST_CASE("xb_add_library BES-only: Trade round-trip", "[bes_add_library]") {
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
