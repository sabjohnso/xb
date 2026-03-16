/// ITCH 5.0 binary encoding integration test.
///
/// Generates binary view/owned types from itch.xsd + itch.bes.xml
/// via xb_generate_cpp(ENCODING ...), then verifies:
/// 1. Generated types compile
/// 2. Owned type round-trips through view
/// 3. Hand-crafted bytes decode to expected values (golden file)

#include <wire_types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

  constexpr std::byte
  B(unsigned char c) {
    return static_cast<std::byte>(c);
  }

} // namespace

// ---------------------------------------------------------------------------
// AddOrder: owned round-trip
// ---------------------------------------------------------------------------

TEST_CASE("ITCH AddOrder: owned round-trip", "[itch][binary]") {
  AddOrder_owned order;

  order.set_msg_type(0x41); // 'A'
  order.set_stock_locate(1234);
  order.set_tracking_number(5678);
  // timestamp is 48-bit — stored in a uint64, only lower 48 bits used
  // The layout engine gives it 48 bits starting at offset 40.
  // With default 8-bit alignment, fields are byte-aligned.
  // Let's check what the layout produces for 48-bit fields.
  // Actually, 48 bits is not a standard width (8/16/32/64), so it
  // will use extract_bits/insert_bits. Let's set a value that fits.

  order.set_order_reference(42);
  order.set_side(0x42); // 'B' = Buy
  order.set_shares(100);
  order.set_stock("AAPL");
  order.set_price(1500000); // $150.0000

  // Read back through const view
  AddOrder_view view(order.buffer());

  CHECK(view.msg_type() == 0x41);
  CHECK(view.stock_locate() == 1234);
  CHECK(view.tracking_number() == 5678);
  CHECK(view.order_reference() == 42);
  CHECK(view.side() == 0x42);
  CHECK(view.shares() == 100);
  CHECK(view.stock().substr(0, 4) == "AAPL");
  CHECK(view.price() == 1500000);
}

// ---------------------------------------------------------------------------
// AddOrder: golden-file decode
// ---------------------------------------------------------------------------

TEST_CASE("ITCH AddOrder: golden-file decode", "[itch][binary]") {
  // Hand-crafted ITCH Add Order message (36 bytes):
  //   offset  0: uint8  msg_type = 'A' (0x41)
  //   offset  1: uint16 stock_locate = 1 (0x0001)
  //   offset  3: uint16 tracking_number = 0 (0x0000)
  //   offset  5: 48-bit timestamp = 1000000000 (0x3B9ACA00, 6 bytes)
  //   offset 11: uint64 order_reference = 123456 (0x1E240)
  //   offset 19: uint8  side = 'B' (0x42)
  //   offset 20: uint32 shares = 500 (0x1F4)
  //   offset 24: 8-byte ASCII stock = "MSFT    "
  //   offset 32: uint32 price = 2850000 (0x2B7CD0)
  std::array<std::byte, 36> buf{};
  auto* p = buf.data();

  p[0] = B(0x41); // msg_type = 'A'

  // stock_locate = 1
  p[1] = B(0x00);
  p[2] = B(0x01);

  // tracking_number = 0
  p[3] = B(0x00);
  p[4] = B(0x00);

  // timestamp = 1000000000 in 6 bytes (big-endian, upper 2 bytes zero)
  p[5] = B(0x00);
  p[6] = B(0x00);
  p[7] = B(0x3B);
  p[8] = B(0x9A);
  p[9] = B(0xCA);
  p[10] = B(0x00);

  // order_reference = 123456
  p[11] = B(0x00);
  p[12] = B(0x00);
  p[13] = B(0x00);
  p[14] = B(0x00);
  p[15] = B(0x00);
  p[16] = B(0x01);
  p[17] = B(0xE2);
  p[18] = B(0x40);

  // side = 'B'
  p[19] = B(0x42);

  // shares = 500
  p[20] = B(0x00);
  p[21] = B(0x00);
  p[22] = B(0x01);
  p[23] = B(0xF4);

  // stock = "MSFT    "
  p[24] = B('M');
  p[25] = B('S');
  p[26] = B('F');
  p[27] = B('T');
  p[28] = B(' ');
  p[29] = B(' ');
  p[30] = B(' ');
  p[31] = B(' ');

  // price = 2850000 = 0x002B7CD0
  p[32] = B(0x00);
  p[33] = B(0x2B);
  p[34] = B(0x7C);
  p[35] = B(0xD0);

  std::span<const std::byte> span(buf);
  AddOrder_view view(span);

  CHECK(view.msg_type() == 0x41);
  CHECK(view.stock_locate() == 1);
  CHECK(view.tracking_number() == 0);
  CHECK(view.order_reference() == 123456);
  CHECK(view.side() == 0x42);
  CHECK(view.shares() == 500);
  CHECK(view.stock() == "MSFT    ");
  CHECK(view.price() == 2850000);
}

// ---------------------------------------------------------------------------
// Trade: compile check + basic round-trip
// ---------------------------------------------------------------------------

TEST_CASE("ITCH Trade: basic round-trip", "[itch][binary]") {
  Trade_owned trade;

  trade.set_msg_type(0x50); // 'P'
  trade.set_stock_locate(1);
  trade.set_tracking_number(0);
  trade.set_order_reference(999);
  trade.set_side(0x42);
  trade.set_shares(200);
  trade.set_stock("GOOG");
  trade.set_price(1410000);
  trade.set_match_number(7777);

  Trade_view view(trade.buffer());

  CHECK(view.msg_type() == 0x50);
  CHECK(view.stock_locate() == 1);
  CHECK(view.shares() == 200);
  CHECK(view.stock().substr(0, 4) == "GOOG");
  CHECK(view.price() == 1410000);
  CHECK(view.match_number() == 7777);
}

// ---------------------------------------------------------------------------
// Concept satisfaction
// ---------------------------------------------------------------------------

TEST_CASE("ITCH AddOrder: types satisfy concept", "[itch][binary]") {
  static_assert(AddOrder_message<AddOrder_view<>>);
  static_assert(AddOrder_message<AddOrder_owned>);
  SUCCEED();
}
