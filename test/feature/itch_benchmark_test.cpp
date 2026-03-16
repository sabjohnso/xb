/// Performance validation for binary view/owned types.
///
/// Uses the ITCH-generated types to measure decode/encode latency
/// and validation level overhead via Catch2 BENCHMARK.

#include <wire_types.hpp>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace {

  constexpr std::byte
  B(unsigned char c) {
    return static_cast<std::byte>(c);
  }

  // Pre-built ITCH AddOrder message (36 bytes)
  auto
  make_add_order_buffer() -> std::array<std::byte, 36> {
    std::array<std::byte, 36> buf{};
    auto* p = buf.data();
    p[0] = B(0x41); // msg_type = 'A'
    p[1] = B(0x00);
    p[2] = B(0x01); // stock_locate = 1
    p[3] = B(0x00);
    p[4] = B(0x00); // tracking_number = 0
    // timestamp (48 bits) = 1000000000
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
    p[19] = B(0x42); // side = 'B'
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
    // price = 2850000
    p[32] = B(0x00);
    p[33] = B(0x2B);
    p[34] = B(0x7C);
    p[35] = B(0xD0);
    return buf;
  }

  // Hand-written struct for baseline comparison
  struct add_order_hand {
    std::uint8_t msg_type;
    std::uint16_t stock_locate;
    std::uint16_t tracking_number;
    std::uint64_t order_reference;
    std::uint8_t side;
    std::uint32_t shares;
    std::uint32_t price;
  };

  add_order_hand
  decode_hand(std::span<const std::byte> buf) {
    add_order_hand r;
    r.msg_type = static_cast<std::uint8_t>(buf[0]);
    std::uint16_t tmp16;
    std::memcpy(&tmp16, buf.data() + 1, 2);
    r.stock_locate = xb::wire::from_wire<std::endian::big>(tmp16);
    std::memcpy(&tmp16, buf.data() + 3, 2);
    r.tracking_number = xb::wire::from_wire<std::endian::big>(tmp16);
    std::uint64_t tmp64;
    std::memcpy(&tmp64, buf.data() + 11, 8);
    r.order_reference = xb::wire::from_wire<std::endian::big>(tmp64);
    r.side = static_cast<std::uint8_t>(buf[19]);
    std::uint32_t tmp32;
    std::memcpy(&tmp32, buf.data() + 20, 4);
    r.shares = xb::wire::from_wire<std::endian::big>(tmp32);
    std::memcpy(&tmp32, buf.data() + 32, 4);
    r.price = xb::wire::from_wire<std::endian::big>(tmp32);
    return r;
  }

} // namespace

// ---------------------------------------------------------------------------
// Decode latency
// ---------------------------------------------------------------------------

TEST_CASE("benchmark: AddOrder view decode", "[benchmark][itch]") {
  auto buf = make_add_order_buffer();
  std::span<const std::byte> span(buf);

  // Baseline: hand-written decode
  BENCHMARK("hand-written decode") {
    auto r = decode_hand(span);
    return r.price; // prevent optimization
  };

  // Generated view decode (full validation)
  BENCHMARK("view<full> decode") {
    AddOrder_view<xb::wire::validation_level::full> view(span);
    return view.price();
  };

  // Generated view decode (discriminant — no validation)
  BENCHMARK("view<discriminant> decode") {
    AddOrder_view<xb::wire::validation_level::discriminant> view(span);
    return view.price();
  };

  // Access all fields through view
  BENCHMARK("view<full> decode all fields") {
    AddOrder_view view(span);
    std::uint64_t sink = 0;
    sink += view.msg_type();
    sink += view.stock_locate();
    sink += view.tracking_number();
    sink += view.order_reference();
    sink += view.side();
    sink += view.shares();
    sink += view.stock().size();
    sink += view.price();
    return sink;
  };
}

// ---------------------------------------------------------------------------
// Encode latency
// ---------------------------------------------------------------------------

TEST_CASE("benchmark: AddOrder owned encode", "[benchmark][itch]") {
  BENCHMARK("owned encode all fields") {
    AddOrder_owned order;
    order.set_msg_type(0x41);
    order.set_stock_locate(1);
    order.set_tracking_number(0);
    order.set_order_reference(123456);
    order.set_side(0x42);
    order.set_shares(500);
    order.set_stock("MSFT");
    order.set_price(2850000);
    return order.buffer().size();
  };

  // Baseline: raw memcpy encode
  BENCHMARK("raw memcpy encode") {
    std::array<std::byte, 36> buf{};
    auto* p = buf.data();
    p[0] = static_cast<std::byte>(0x41);
    auto v16 = xb::wire::to_wire<std::endian::big>(std::uint16_t{1});
    std::memcpy(p + 1, &v16, 2);
    v16 = xb::wire::to_wire<std::endian::big>(std::uint16_t{0});
    std::memcpy(p + 3, &v16, 2);
    auto v64 = xb::wire::to_wire<std::endian::big>(std::uint64_t{123456});
    std::memcpy(p + 11, &v64, 8);
    p[19] = static_cast<std::byte>(0x42);
    auto v32 = xb::wire::to_wire<std::endian::big>(std::uint32_t{500});
    std::memcpy(p + 20, &v32, 4);
    std::memcpy(p + 24, "MSFT    ", 8);
    v32 = xb::wire::to_wire<std::endian::big>(std::uint32_t{2850000});
    std::memcpy(p + 32, &v32, 4);
    return buf.size();
  };
}

// ---------------------------------------------------------------------------
// Validation level overhead
// ---------------------------------------------------------------------------

TEST_CASE("benchmark: validation level overhead", "[benchmark][itch]") {
  auto buf = make_add_order_buffer();
  std::span<const std::byte> span(buf);

  BENCHMARK("full validation construct + read") {
    AddOrder_view<xb::wire::validation_level::full> view(span);
    return view.shares();
  };

  BENCHMARK("structural validation construct + read") {
    AddOrder_view<xb::wire::validation_level::structural> view(span);
    return view.shares();
  };

  BENCHMARK("discriminant validation construct + read") {
    AddOrder_view<xb::wire::validation_level::discriminant> view(span);
    return view.shares();
  };
}

// ---------------------------------------------------------------------------
// Endianness: compile-time verification
// ---------------------------------------------------------------------------

TEST_CASE("endianness: native order is no-op", "[benchmark][endian]") {
  // Verify from_wire<native> returns the input unchanged
  std::uint32_t val = 0x12345678;
  CHECK(xb::wire::from_wire<std::endian::native>(val) == val);
  CHECK(xb::wire::to_wire<std::endian::native>(val) == val);

  // The compiler should emit zero instructions for native-order
  // conversion. We verify the semantic guarantee here; disassembly
  // inspection is a manual step.
  BENCHMARK("from_wire<native> (should be zero-cost)") {
    return xb::wire::from_wire<std::endian::native>(val);
  };

  BENCHMARK("from_wire<big> (may need bswap on little-endian host)") {
    return xb::wire::from_wire<std::endian::big>(val);
  };
}
