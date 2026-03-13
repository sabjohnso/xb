#include <encoding.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <variant>

using namespace xb::bes;

// ---------------------------------------------------------------------------
// Enumerations: distinct values
// ---------------------------------------------------------------------------

TEST_CASE("byte_order_type has three values", "[wire][encoding_model]") {
  CHECK(byte_order_type::big_endian != byte_order_type::little_endian);
  CHECK(byte_order_type::little_endian != byte_order_type::native);
  CHECK(byte_order_type::big_endian != byte_order_type::native);
}

TEST_CASE("bit_order_type has two values", "[wire][encoding_model]") {
  CHECK(bit_order_type::msb_first != bit_order_type::lsb_first);
}

TEST_CASE("primitive_encoding_type covers all wire encodings",
          "[wire][encoding_model]") {
  CHECK(primitive_encoding_type::unsigned_ !=
        primitive_encoding_type::twos_complement);
  CHECK(primitive_encoding_type::bcd != primitive_encoding_type::ascii_decimal);
  CHECK(primitive_encoding_type::ieee754 !=
        primitive_encoding_type::fixed_point);
  CHECK(primitive_encoding_type::ascii != primitive_encoding_type::utf8);
  CHECK(primitive_encoding_type::raw != primitive_encoding_type::boolean);
  CHECK(primitive_encoding_type::enum_integer !=
        primitive_encoding_type::enum_char);
  CHECK(primitive_encoding_type::bitset != primitive_encoding_type::epoch);
  CHECK(primitive_encoding_type::epoch_millis !=
        primitive_encoding_type::epoch_nanos);
}

TEST_CASE("padding_type has three values", "[wire][encoding_model]") {
  CHECK(padding_type::space != padding_type::null);
  CHECK(padding_type::null != padding_type::zero);
}

TEST_CASE("charset_type has four values", "[wire][encoding_model]") {
  CHECK(charset_type::ascii != charset_type::utf8);
  CHECK(charset_type::utf16_be != charset_type::utf16_le);
}

TEST_CASE("length_includes_type has two values", "[wire][encoding_model]") {
  CHECK(length_includes_type::header != length_includes_type::payload);
}

TEST_CASE("length_unit_type has two values", "[wire][encoding_model]") {
  CHECK(length_unit_type::bits != length_unit_type::bytes);
}

TEST_CASE("enum_encode_by_type has two values", "[wire][encoding_model]") {
  CHECK(enum_encode_by_type::value != enum_encode_by_type::ordinal);
}

// ---------------------------------------------------------------------------
// Enum string round-trip
// ---------------------------------------------------------------------------

TEST_CASE("byte_order_type round-trips through string",
          "[wire][encoding_model]") {
  CHECK(byte_order_type_from_string(to_string(byte_order_type::big_endian)) ==
        byte_order_type::big_endian);
  CHECK(byte_order_type_from_string(to_string(
            byte_order_type::little_endian)) == byte_order_type::little_endian);
  CHECK(byte_order_type_from_string(to_string(byte_order_type::native)) ==
        byte_order_type::native);
}

TEST_CASE("primitive_encoding_type round-trips through string",
          "[wire][encoding_model]") {
  CHECK(primitive_encoding_type_from_string(
            to_string(primitive_encoding_type::unsigned_)) ==
        primitive_encoding_type::unsigned_);
  CHECK(primitive_encoding_type_from_string(
            to_string(primitive_encoding_type::twos_complement)) ==
        primitive_encoding_type::twos_complement);
  CHECK(primitive_encoding_type_from_string(
            to_string(primitive_encoding_type::bcd_datetime)) ==
        primitive_encoding_type::bcd_datetime);
}

// ---------------------------------------------------------------------------
// defaults_type
// ---------------------------------------------------------------------------

TEST_CASE("defaults_type default-constructed has alignment=8",
          "[wire][encoding_model]") {
  defaults_type d;
  // XSD defaults (big-endian, msb-first, etc.) are applied during XML
  // parsing, not in the C++ default constructor.  Only alignment has a
  // compile-time default.
  CHECK(!d.byte_order.has_value());
  CHECK(!d.bit_order.has_value());
  CHECK(d.alignment.has_value());
  CHECK(*d.alignment == 8);
  CHECK(!d.string_padding.has_value());
  CHECK(!d.string_encoding.has_value());
}

TEST_CASE("defaults_type round-trips explicit values through XML",
          "[wire][encoding_model]") {
  std::string xml = R"(<defaults xmlns="http://xb.dev/encoding"
    byte-order="little-endian" bit-order="lsb-first"
    alignment="32" string-padding="null" string-encoding="utf8"/>)";
  xb::expat_reader reader(xml);
  reader.read();
  auto d = read_defaults_type(reader);
  CHECK(d.byte_order == byte_order_type::little_endian);
  CHECK(d.bit_order == bit_order_type::lsb_first);
  CHECK(d.alignment.has_value());
  CHECK(*d.alignment == 32);
  CHECK(d.string_padding == padding_type::null);
  CHECK(d.string_encoding == charset_type::utf8);
}

// ---------------------------------------------------------------------------
// import_type
// ---------------------------------------------------------------------------

TEST_CASE("import_type stores href", "[wire][encoding_model]") {
  import_type imp;
  imp.href = "base-sbe.xml";
  CHECK(imp.href == "base-sbe.xml");
}

// ---------------------------------------------------------------------------
// selector_type
// ---------------------------------------------------------------------------

TEST_CASE("selector_type default-constructed has no values",
          "[wire][encoding_model]") {
  selector_type s;
  CHECK(!s.qname.has_value());
  CHECK(!s.path.has_value());
  CHECK(!s.type.has_value());
  CHECK(!s.namespace_.has_value());
  CHECK(!s.attribute.has_value());
}

// ---------------------------------------------------------------------------
// present_when_type
// ---------------------------------------------------------------------------

TEST_CASE("present_when_type with expression", "[wire][encoding_model]") {
  present_when_type pw;
  pw.expr = "incompat_flags & 0x01";
  CHECK(pw.expr == "incompat_flags & 0x01");
  CHECK(!pw.field.has_value());
  CHECK(!pw.field_equals.has_value());
}

// ---------------------------------------------------------------------------
// field_type
// ---------------------------------------------------------------------------

TEST_CASE("field_type stores name and bits", "[wire][encoding_model]") {
  field_type f;
  f.name = "stock_locate";
  f.bits = 16;
  f.encoding = primitive_encoding_type::unsigned_;
  CHECK(f.name == "stock_locate");
  CHECK(f.bits == 16);
  CHECK(f.encoding == primitive_encoding_type::unsigned_);
  CHECK(!f.byte_order.has_value());
  CHECK(!f.offset.has_value());
  CHECK(!f.null_value.has_value());
  CHECK(!f.size.has_value());
  CHECK(!f.present_when.has_value());
}

// ---------------------------------------------------------------------------
// padding_field_type
// ---------------------------------------------------------------------------

TEST_CASE("padding_field_type defaults", "[wire][encoding_model]") {
  padding_field_type p;
  CHECK(!p.bits.has_value());
  CHECK(!p.align_to.has_value());
  CHECK(p.fill == "0x00");
}

// ---------------------------------------------------------------------------
// wire_field_type
// ---------------------------------------------------------------------------

TEST_CASE("wire_field_type stores name and bits", "[wire][encoding_model]") {
  wire_field_type w;
  w.name = "reserved";
  w.bits = 8;
  w.value = "0x00";
  CHECK(w.name == "reserved");
  CHECK(w.bits == 8);
  CHECK(w.value == "0x00");
}

// ---------------------------------------------------------------------------
// length_type
// ---------------------------------------------------------------------------

TEST_CASE("length_type defaults", "[wire][encoding_model]") {
  length_type l;
  l.field = "total_length";
  CHECK(l.field == "total_length");
  // XSD defaults (payload, bytes) are applied during XML parsing
  CHECK(!l.includes.has_value());
  CHECK(!l.unit.has_value());
  CHECK(!l.adjust.has_value());
}

// ---------------------------------------------------------------------------
// computed_field_type
// ---------------------------------------------------------------------------

TEST_CASE("computed_field_type stores algorithm and range",
          "[wire][encoding_model]") {
  computed_field_type cf;
  cf.name = "crc";
  cf.bits = 16;
  cf.algorithm = "crc16-ccitt";
  cf.over = "header_start..payload_end";
  cf.seed_expr = "crc_extra";
  CHECK(cf.algorithm == "crc16-ccitt");
  CHECK(cf.over == "header_start..payload_end");
}

// ---------------------------------------------------------------------------
// frame_stack_type
// ---------------------------------------------------------------------------

TEST_CASE("frame_stack_type stores ordered layers", "[wire][encoding_model]") {
  frame_stack_type fs;
  fs.name = "itch-multicast";

  layer_type l1;
  l1.ref = "ethernet";
  l1.match = "ether_type";
  l1.value = "0x0800";

  layer_type l2;
  l2.ref = "udp";

  fs.layer.push_back(l1);
  fs.layer.push_back(l2);

  CHECK(fs.name == "itch-multicast");
  CHECK(fs.layer.size() == 2);
  CHECK(fs.layer[0].ref == "ethernet");
  CHECK(!fs.layer[1].match.has_value());
}

// ---------------------------------------------------------------------------
// message_type
// ---------------------------------------------------------------------------

TEST_CASE("message_type stores full message definition",
          "[wire][encoding_model]") {
  message_type msg;
  msg.name = "AddOrder";
  msg.type = "itch:AddOrderMessage";
  msg.discriminant_value = "'A'";
  msg.framing = "itch_frame";

  CHECK(msg.name == "AddOrder");
  CHECK(msg.type == "itch:AddOrderMessage");
  CHECK(msg.discriminant_value == "'A'");
  CHECK(!msg.selector.has_value());
  CHECK(!msg.extensions.has_value());
}

// ---------------------------------------------------------------------------
// encoding_type (top-level)
// ---------------------------------------------------------------------------

TEST_CASE("encoding_type default-constructed is empty",
          "[wire][encoding_model]") {
  encoding_type spec;
  CHECK(!spec.target_namespace.has_value());
  CHECK(!spec.target_schema.has_value());
  CHECK(spec.import.empty());
  CHECK(spec.choice.empty());
}

TEST_CASE("encoding_type stores top-level components",
          "[wire][encoding_model]") {
  encoding_type spec;
  spec.target_namespace = "http://example.com/itch";

  spec.import.push_back({"base.xml"});

  framing_type fr;
  fr.name = "itch_frame";
  spec.choice.push_back(std::move(fr));

  message_type msg;
  msg.name = "AddOrder";
  spec.choice.push_back(std::move(msg));

  CHECK(spec.target_namespace == "http://example.com/itch");
  CHECK(spec.import.size() == 1);
  CHECK(spec.choice.size() == 2);
  CHECK(std::holds_alternative<framing_type>(spec.choice[0]));
  CHECK(std::holds_alternative<message_type>(spec.choice[1]));
}

// ---------------------------------------------------------------------------
// XML round-trip: write then read back
// ---------------------------------------------------------------------------

TEST_CASE("encoding_type round-trips through XML", "[wire][encoding_model]") {
  // Build a minimal encoding spec
  encoding_type spec;
  spec.target_namespace = "http://example.com/test";

  defaults_type defs;
  defs.byte_order = byte_order_type::little_endian;
  spec.defaults = defs;

  message_type msg;
  msg.name = "TestMsg";
  msg.discriminant_value = "42";

  field_type f;
  f.name = "value";
  f.bits = 32;
  f.encoding = primitive_encoding_type::unsigned_;
  msg.choice.push_back(f);

  spec.choice.push_back(std::move(msg));

  // Serialize to XML
  std::ostringstream oss;
  {
    xb::ostream_writer writer(oss);
    writer.start_element(xb::qname{"http://xb.dev/encoding", "encoding"});
    writer.namespace_declaration("", "http://xb.dev/encoding");
    write_encoding_type(spec, writer);
    writer.end_element();
  }

  std::string xml = oss.str();
  CHECK(!xml.empty());

  // Deserialize from XML
  xb::expat_reader reader(xml);
  reader.read(); // advance to root element
  auto parsed = read_encoding_type(reader);

  CHECK(parsed.target_namespace == "http://example.com/test");
  CHECK(parsed.defaults.has_value());
  CHECK(parsed.defaults->byte_order == byte_order_type::little_endian);
  CHECK(parsed.choice.size() == 1);
  CHECK(std::holds_alternative<message_type>(parsed.choice[0]));

  auto& parsed_msg = std::get<message_type>(parsed.choice[0]);
  CHECK(parsed_msg.name == "TestMsg");
  CHECK(parsed_msg.discriminant_value == "42");
  CHECK(parsed_msg.choice.size() == 1);
  CHECK(std::holds_alternative<field_type>(parsed_msg.choice[0]));

  auto& parsed_field = std::get<field_type>(parsed_msg.choice[0]);
  CHECK(parsed_field.name == "value");
  CHECK(parsed_field.bits == 32);
  CHECK(parsed_field.encoding == primitive_encoding_type::unsigned_);
}
