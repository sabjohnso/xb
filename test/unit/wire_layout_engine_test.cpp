#include <xb/wire/layout_engine.hpp>

#include <xb/wire/encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb::bes;
using namespace xb::wire;

// ---------------------------------------------------------------------------
// Helpers: build BES message types for testing
// ---------------------------------------------------------------------------

namespace {

  field_type
  make_field(const std::string& name, unsigned bits) {
    field_type f;
    f.name = name;
    f.bits = bits;
    return f;
  }

  wire_field_type
  make_wire_field(const std::string& name, unsigned bits) {
    wire_field_type f;
    f.name = name;
    f.bits = bits;
    return f;
  }

  padding_field_type
  make_padding(unsigned bits) {
    padding_field_type p;
    p.bits = bits;
    return p;
  }

  padding_field_type
  make_align_to(unsigned boundary) {
    padding_field_type p;
    p.align_to = boundary;
    return p;
  }

  computed_field_type
  make_computed(const std::string& name, unsigned bits,
                const std::string& algorithm, const std::string& over) {
    computed_field_type f;
    f.name = name;
    f.bits = bits;
    f.algorithm = algorithm;
    f.over = over;
    return f;
  }

  defaults_type
  make_defaults(unsigned alignment) {
    defaults_type d;
    d.alignment = alignment;
    return d;
  }

} // namespace

// ---------------------------------------------------------------------------
// Simple fixed-layout messages
// ---------------------------------------------------------------------------

TEST_CASE("layout: simple three-field message", "[wire][layout]") {
  message_type msg;
  msg.name = "Order";
  msg.choice.push_back(make_field("price", 64));
  msg.choice.push_back(make_field("quantity", 32));
  msg.choice.push_back(make_field("side", 8));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 104);
  CHECK(layout.message_name == "Order");

  CHECK(layout.fields[0].name == "price");
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[0].width_bits == 64);
  CHECK(layout.fields[0].category == field_category::data);

  CHECK(layout.fields[1].name == "quantity");
  CHECK(layout.fields[1].offset_bits == 64);
  CHECK(layout.fields[1].width_bits == 32);

  CHECK(layout.fields[2].name == "side");
  CHECK(layout.fields[2].offset_bits == 96);
  CHECK(layout.fields[2].width_bits == 8);
}

// ---------------------------------------------------------------------------
// Alignment and padding
// ---------------------------------------------------------------------------

TEST_CASE("layout: align_to rounds offset up", "[wire][layout]") {
  message_type msg;
  msg.name = "Aligned";
  msg.choice.push_back(make_field("flags", 8));
  msg.choice.push_back(make_align_to(32)); // align to 32-bit boundary
  msg.choice.push_back(make_field("value", 32));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.is_fixed);

  // flags at 0, padding at 8, value at 32
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[0].width_bits == 8);

  CHECK(layout.fields[1].category == field_category::padding);
  CHECK(layout.fields[1].offset_bits == 8);
  CHECK(layout.fields[1].width_bits == 24); // 32 - 8

  CHECK(layout.fields[2].name == "value");
  CHECK(layout.fields[2].offset_bits == 32);
}

TEST_CASE("layout: align_to at boundary is zero-width padding",
          "[wire][layout]") {
  message_type msg;
  msg.name = "AlreadyAligned";
  msg.choice.push_back(make_field("a", 32));
  msg.choice.push_back(make_align_to(32));
  msg.choice.push_back(make_field("b", 32));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 3);
  // Padding is zero-width
  CHECK(layout.fields[1].category == field_category::padding);
  CHECK(layout.fields[1].width_bits == 0);
  CHECK(layout.fields[2].offset_bits == 32);
}

TEST_CASE("layout: explicit padding bits", "[wire][layout]") {
  message_type msg;
  msg.name = "Padded";
  msg.choice.push_back(make_field("a", 16));
  msg.choice.push_back(make_padding(16));
  msg.choice.push_back(make_field("b", 32));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 64);
  CHECK(layout.fields[1].category == field_category::padding);
  CHECK(layout.fields[1].offset_bits == 16);
  CHECK(layout.fields[1].width_bits == 16);
  CHECK(layout.fields[2].offset_bits == 32);
}

// ---------------------------------------------------------------------------
// Sub-byte bit packing
// ---------------------------------------------------------------------------

TEST_CASE("layout: sub-byte fields pack correctly", "[wire][layout]") {
  // alignment=0 disables automatic alignment for bit-packed messages
  defaults_type no_align;
  no_align.alignment = 0;

  message_type msg;
  msg.name = "BitPacked";
  msg.choice.push_back(make_field("flags", 3));
  msg.choice.push_back(make_field("version", 5));
  msg.choice.push_back(make_field("length", 16));

  auto layout = compute_layout(msg, no_align);

  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 24); // 3 + 5 + 16

  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[0].width_bits == 3);
  CHECK(layout.fields[1].offset_bits == 3);
  CHECK(layout.fields[1].width_bits == 5);
  CHECK(layout.fields[2].offset_bits == 8);
  CHECK(layout.fields[2].width_bits == 16);
}

// ---------------------------------------------------------------------------
// Explicit field offset
// ---------------------------------------------------------------------------

TEST_CASE("layout: explicit offset creates implicit gap", "[wire][layout]") {
  message_type msg;
  msg.name = "ExplicitOffset";
  msg.choice.push_back(make_field("a", 8));

  field_type f;
  f.name = "b";
  f.bits = 32;
  f.offset = 64; // explicit: skip to bit 64
  msg.choice.push_back(std::move(f));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 96); // 64 + 32
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[1].name == "b");
  CHECK(layout.fields[1].offset_bits == 64);
}

// ---------------------------------------------------------------------------
// Default alignment from defaults
// ---------------------------------------------------------------------------

TEST_CASE("layout: default alignment applied between fields",
          "[wire][layout]") {
  // default alignment = 16 bits
  auto defaults = make_defaults(16);

  message_type msg;
  msg.name = "DefaultAligned";
  msg.choice.push_back(make_field("a", 8));
  msg.choice.push_back(make_field("b", 8));

  auto layout = compute_layout(msg, defaults);

  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.is_fixed);
  // 'a' at 0 (8 bits), then aligned to 16, 'b' at 16
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[1].offset_bits == 16);
  CHECK(layout.total_bits == 24); // 16 + 8
}

// ---------------------------------------------------------------------------
// Wire-only fields
// ---------------------------------------------------------------------------

TEST_CASE("layout: wire-only fields occupy space and are categorized",
          "[wire][layout]") {
  message_type msg;
  msg.name = "WithWireField";
  msg.choice.push_back(make_wire_field("msg_type", 16));
  msg.choice.push_back(make_field("payload", 64));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.is_fixed);
  CHECK(layout.fields[0].name == "msg_type");
  CHECK(layout.fields[0].category == field_category::wire_only);
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[0].width_bits == 16);
  CHECK(layout.fields[1].offset_bits == 16);
  CHECK(layout.fields[1].category == field_category::data);
}

// ---------------------------------------------------------------------------
// Computed fields
// ---------------------------------------------------------------------------

TEST_CASE("layout: computed fields occupy space", "[wire][layout]") {
  message_type msg;
  msg.name = "WithChecksum";
  msg.choice.push_back(make_field("data", 64));
  msg.choice.push_back(make_computed("checksum", 32, "crc32", "start..end"));

  auto layout = compute_layout(msg, defaults_type{});

  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 96);
  CHECK(layout.fields[1].name == "checksum");
  CHECK(layout.fields[1].category == field_category::computed);
  CHECK(layout.fields[1].offset_bits == 64);
  CHECK(layout.fields[1].width_bits == 32);
}

// ---------------------------------------------------------------------------
// Save-position markers
// ---------------------------------------------------------------------------

TEST_CASE("layout: save_position records offset as marker", "[wire][layout]") {
  message_type msg;
  msg.name = "WithMarkers";
  msg.choice.push_back(make_field("header", 32));

  save_position_type sp;
  sp.name = "payload_start";
  msg.choice.push_back(std::move(sp));

  msg.choice.push_back(make_field("data", 64));

  save_position_type sp2;
  sp2.name = "payload_end";
  msg.choice.push_back(std::move(sp2));

  auto layout = compute_layout(msg, defaults_type{});

  CHECK(layout.fields.size() == 2); // markers are not fields
  REQUIRE(layout.markers.size() == 2);
  CHECK(layout.markers[0].name == "payload_start");
  CHECK(layout.markers[0].offset_bits == 32);
  CHECK(layout.markers[1].name == "payload_end");
  CHECK(layout.markers[1].offset_bits == 96);
}

// ---------------------------------------------------------------------------
// Constants have no wire presence
// ---------------------------------------------------------------------------

TEST_CASE("layout: constants have zero width", "[wire][layout]") {
  message_type msg;
  msg.name = "WithConstant";
  msg.choice.push_back(make_field("a", 16));

  constant_type c;
  c.name = "protocol_version";
  c.value = "2";
  msg.choice.push_back(std::move(c));

  msg.choice.push_back(make_field("b", 16));

  auto layout = compute_layout(msg, defaults_type{});

  // constant is in the field list but zero-width
  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.fields[1].name == "protocol_version");
  CHECK(layout.fields[1].category == field_category::constant);
  CHECK(layout.fields[1].width_bits == 0);
  CHECK(layout.fields[1].offset_bits == 16);
  CHECK(layout.fields[2].offset_bits == 16); // b immediately after a
}

// ---------------------------------------------------------------------------
// Metadata items (length, discriminant) don't occupy space
// ---------------------------------------------------------------------------

TEST_CASE("layout: length_type and discriminant_type are metadata only",
          "[wire][layout]") {
  message_type msg;
  msg.name = "WithMetadata";
  msg.choice.push_back(make_wire_field("len", 16));

  length_type lt;
  lt.field = "len";
  msg.choice.push_back(std::move(lt));

  msg.choice.push_back(make_wire_field("msg_type", 8));

  discriminant_type dt;
  dt.field = "msg_type";
  msg.choice.push_back(std::move(dt));

  msg.choice.push_back(make_field("data", 32));

  auto layout = compute_layout(msg, defaults_type{});

  // Only the actual fields appear, not the metadata annotations
  REQUIRE(layout.fields.size() == 3);
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 56);
  CHECK(layout.fields[0].name == "len");
  CHECK(layout.fields[0].offset_bits == 0);
  CHECK(layout.fields[1].name == "msg_type");
  CHECK(layout.fields[1].offset_bits == 16);
  CHECK(layout.fields[2].name == "data");
  CHECK(layout.fields[2].offset_bits == 24);
}

// ---------------------------------------------------------------------------
// Variable-length: group makes layout non-fixed
// ---------------------------------------------------------------------------

TEST_CASE("layout: group makes subsequent offsets variable", "[wire][layout]") {
  message_type msg;
  msg.name = "WithGroup";
  msg.choice.push_back(make_field("header", 32));

  auto grp = std::make_unique<group_type>();
  grp->name = "entries";
  grp->count_field = "num_entries";
  grp->choice.push_back(make_field("entry_price", 64));
  grp->choice.push_back(make_field("entry_qty", 32));
  msg.choice.push_back(std::move(grp));

  msg.choice.push_back(make_field("footer", 16));

  auto layout = compute_layout(msg, defaults_type{});

  CHECK_FALSE(layout.is_fixed);

  // header has fixed offset
  REQUIRE(layout.fields.size() >= 2);
  CHECK(layout.fields[0].name == "header");
  CHECK(layout.fields[0].offset_bits.has_value());
  CHECK(layout.fields[0].offset_bits == 0);

  // footer offset is variable (after the group)
  auto footer =
      std::find_if(layout.fields.begin(), layout.fields.end(),
                   [](const resolved_field& f) { return f.name == "footer"; });
  REQUIRE(footer != layout.fields.end());
  CHECK_FALSE(footer->offset_bits.has_value());
}

// ---------------------------------------------------------------------------
// Variable-length: choice makes layout non-fixed
// ---------------------------------------------------------------------------

TEST_CASE("layout: choice makes subsequent offsets variable",
          "[wire][layout]") {
  message_type msg;
  msg.name = "WithChoice";
  msg.choice.push_back(make_field("disc", 8));

  auto ch = std::make_unique<choice_type>();
  ch->name = "payload";
  ch->discriminant_field = "disc";
  // alternatives with different sizes make layout variable
  auto alt1 = std::make_unique<alternative_type>();
  alt1->name = "small";
  alt1->discriminant_value = "1";
  alt1->choice.push_back(make_field("val", 8));
  ch->alternative.push_back(std::move(alt1));

  auto alt2 = std::make_unique<alternative_type>();
  alt2->name = "large";
  alt2->discriminant_value = "2";
  alt2->choice.push_back(make_field("val", 64));
  ch->alternative.push_back(std::move(alt2));

  msg.choice.push_back(std::move(ch));
  msg.choice.push_back(make_field("trailer", 8));

  auto layout = compute_layout(msg, defaults_type{});

  CHECK_FALSE(layout.is_fixed);

  // disc has fixed offset
  CHECK(layout.fields[0].name == "disc");
  CHECK(layout.fields[0].offset_bits == 0);

  // trailer offset is variable
  auto trailer =
      std::find_if(layout.fields.begin(), layout.fields.end(),
                   [](const resolved_field& f) { return f.name == "trailer"; });
  REQUIRE(trailer != layout.fields.end());
  CHECK_FALSE(trailer->offset_bits.has_value());
}

// ---------------------------------------------------------------------------
// Empty message
// ---------------------------------------------------------------------------

TEST_CASE("layout: empty message", "[wire][layout]") {
  message_type msg;
  msg.name = "Empty";

  auto layout = compute_layout(msg, defaults_type{});

  CHECK(layout.fields.empty());
  CHECK(layout.markers.empty());
  CHECK(layout.is_fixed);
  CHECK(layout.total_bits == 0);
}
