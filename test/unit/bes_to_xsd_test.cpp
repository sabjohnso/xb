#include <catch2/catch_test_macros.hpp>

#include <xb/wire/bes_to_xsd.hpp>
#include <xb/wire/encoding.hpp>
#include <xb/wire/encoding_resolver.hpp>

// =========================================================================
// Helpers
// =========================================================================

namespace {

  xb::bes::field_type
  make_field(std::string name, unsigned bits,
             std::optional<xb::bes::primitive_encoding_type> enc = std::nullopt,
             std::optional<std::string> null_val = std::nullopt) {
    xb::bes::field_type f;
    f.name = std::move(name);
    f.bits = bits;
    f.encoding = enc;
    f.null_value = std::move(null_val);
    return f;
  }

  xb::bes::wire_field_type
  make_wire_field(std::string name, unsigned bits) {
    xb::bes::wire_field_type wf;
    wf.name = std::move(name);
    wf.bits = bits;
    return wf;
  }

  void
  add_field(xb::bes::message_type& msg, xb::bes::field_type f) {
    msg.choice.emplace_back(std::move(f));
  }

  void
  add_wire_field(xb::bes::message_type& msg, xb::bes::wire_field_type wf) {
    msg.choice.emplace_back(std::move(wf));
  }

  void
  add_constant(xb::bes::message_type& msg, xb::bes::constant_type ct) {
    msg.choice.emplace_back(std::move(ct));
  }

} // namespace

// =========================================================================
// infer_xsd_type — unsigned integers
// =========================================================================

TEST_CASE("infer_xsd_type: unsigned 8-bit → xs:unsignedByte", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(8, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.local_name == "unsignedByte");
  CHECK(result.base_type == "xs:unsignedByte");
  CHECK_FALSE(result.facets.max_inclusive.has_value());
}

TEST_CASE("infer_xsd_type: unsigned 16-bit → xs:unsignedShort",
          "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(16, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.local_name == "unsignedShort");
  CHECK(result.base_type == "xs:unsignedShort");
}

TEST_CASE("infer_xsd_type: unsigned 32-bit → xs:unsignedInt", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(32, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.local_name == "unsignedInt");
  CHECK(result.base_type == "xs:unsignedInt");
}

TEST_CASE("infer_xsd_type: unsigned 64-bit → xs:unsignedLong", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(64, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.local_name == "unsignedLong");
  CHECK(result.base_type == "xs:unsignedLong");
}

// =========================================================================
// infer_xsd_type — non-standard unsigned widths
// =========================================================================

TEST_CASE("infer_xsd_type: unsigned 12-bit → xs:unsignedShort with "
          "maxInclusive=4095",
          "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(12, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.base_type == "xs:unsignedShort");
  REQUIRE(result.facets.max_inclusive.has_value());
  CHECK(*result.facets.max_inclusive == "4095");
}

TEST_CASE("infer_xsd_type: unsigned 48-bit → xs:unsignedLong with "
          "maxInclusive",
          "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(48, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.base_type == "xs:unsignedLong");
  REQUIRE(result.facets.max_inclusive.has_value());
  CHECK(*result.facets.max_inclusive == "281474976710655"); // 2^48 - 1
}

TEST_CASE("infer_xsd_type: unsigned 1-bit → xs:unsignedByte with "
          "maxInclusive=1",
          "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(1, xb::bes::primitive_encoding_type::unsigned_);
  CHECK(result.base_type == "xs:unsignedByte");
  REQUIRE(result.facets.max_inclusive.has_value());
  CHECK(*result.facets.max_inclusive == "1");
}

// =========================================================================
// infer_xsd_type — signed (twos-complement) integers
// =========================================================================

TEST_CASE("infer_xsd_type: twos_complement 8-bit → xs:byte", "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(
      8, xb::bes::primitive_encoding_type::twos_complement);
  CHECK(result.local_name == "byte");
  CHECK(result.base_type == "xs:byte");
  CHECK_FALSE(result.facets.min_inclusive.has_value());
  CHECK_FALSE(result.facets.max_inclusive.has_value());
}

TEST_CASE("infer_xsd_type: twos_complement 16-bit → xs:short", "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(
      16, xb::bes::primitive_encoding_type::twos_complement);
  CHECK(result.base_type == "xs:short");
}

TEST_CASE("infer_xsd_type: twos_complement 32-bit → xs:int", "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(
      32, xb::bes::primitive_encoding_type::twos_complement);
  CHECK(result.base_type == "xs:int");
}

TEST_CASE("infer_xsd_type: twos_complement 64-bit → xs:long", "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(
      64, xb::bes::primitive_encoding_type::twos_complement);
  CHECK(result.base_type == "xs:long");
}

TEST_CASE("infer_xsd_type: twos_complement 12-bit → xs:short with "
          "min/max",
          "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(
      12, xb::bes::primitive_encoding_type::twos_complement);
  CHECK(result.base_type == "xs:short");
  REQUIRE(result.facets.min_inclusive.has_value());
  REQUIRE(result.facets.max_inclusive.has_value());
  CHECK(*result.facets.min_inclusive == "-2048"); // -(2^11)
  CHECK(*result.facets.max_inclusive == "2047");  // 2^11 - 1
}

// =========================================================================
// infer_xsd_type — other encodings
// =========================================================================

TEST_CASE("infer_xsd_type: ascii → xs:string with maxLength", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(64, xb::bes::primitive_encoding_type::ascii);
  CHECK(result.base_type == "xs:string");
  REQUIRE(result.facets.max_length.has_value());
  CHECK(*result.facets.max_length == 8); // 64 / 8
}

TEST_CASE("infer_xsd_type: utf8 → xs:string with maxLength", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(80, xb::bes::primitive_encoding_type::utf8);
  CHECK(result.base_type == "xs:string");
  REQUIRE(result.facets.max_length.has_value());
  CHECK(*result.facets.max_length == 10); // 80 / 8
}

TEST_CASE("infer_xsd_type: raw → xs:hexBinary with length", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(48, xb::bes::primitive_encoding_type::raw);
  CHECK(result.base_type == "xs:hexBinary");
  REQUIRE(result.facets.length.has_value());
  CHECK(*result.facets.length == 6); // 48 / 8
}

TEST_CASE("infer_xsd_type: ieee754 32-bit → xs:float", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(32, xb::bes::primitive_encoding_type::ieee754);
  CHECK(result.base_type == "xs:float");
}

TEST_CASE("infer_xsd_type: ieee754 64-bit → xs:double", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(64, xb::bes::primitive_encoding_type::ieee754);
  CHECK(result.base_type == "xs:double");
}

TEST_CASE("infer_xsd_type: boolean → xs:boolean", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(1, xb::bes::primitive_encoding_type::boolean);
  CHECK(result.base_type == "xs:boolean");
}

TEST_CASE("infer_xsd_type: epoch → xs:unsignedLong", "[bes_to_xsd]") {
  auto result =
      xb::wire::infer_xsd_type(64, xb::bes::primitive_encoding_type::epoch);
  CHECK(result.base_type == "xs:unsignedLong");
}

// =========================================================================
// infer_xsd_type — no encoding (inferred from bits)
// =========================================================================

TEST_CASE("infer_xsd_type: no encoding, 8-bit → xs:unsignedByte",
          "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(8, std::nullopt);
  CHECK(result.base_type == "xs:unsignedByte");
}

TEST_CASE("infer_xsd_type: no encoding, 16-bit → xs:unsignedShort",
          "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(16, std::nullopt);
  CHECK(result.base_type == "xs:unsignedShort");
}

TEST_CASE("infer_xsd_type: no encoding, 32-bit → xs:unsignedInt",
          "[bes_to_xsd]") {
  auto result = xb::wire::infer_xsd_type(32, std::nullopt);
  CHECK(result.base_type == "xs:unsignedInt");
}

// =========================================================================
// generate_xsd — full XSD output from BES
// =========================================================================

TEST_CASE("generate_xsd: produces valid XSD from simple BES", "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "http://example.com/itch";

  xb::bes::message_type msg;
  msg.name = "AddOrder";
  add_wire_field(msg, make_wire_field("msg_type", 8));
  add_field(msg, make_field("stock_locate", 16));
  add_field(msg, make_field("shares", 32));
  add_field(msg,
            make_field("stock", 64, xb::bes::primitive_encoding_type::ascii));
  add_field(msg, make_field("price", 32));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);

  // Must contain XML declaration and xs:schema root
  CHECK(xsd.find("<?xml") != std::string::npos);
  CHECK(xsd.find("xs:schema") != std::string::npos);
  CHECK(xsd.find("http://example.com/itch") != std::string::npos);

  // Must contain complex type for AddOrder
  CHECK(xsd.find("AddOrder") != std::string::npos);

  // Must contain element declarations for data fields
  CHECK(xsd.find("stock_locate") != std::string::npos);
  CHECK(xsd.find("shares") != std::string::npos);
  CHECK(xsd.find("stock") != std::string::npos);
  CHECK(xsd.find("price") != std::string::npos);

  // Must NOT contain wire-only field
  CHECK(xsd.find("msg_type") == std::string::npos);
}

TEST_CASE("generate_xsd: null-value field gets minOccurs=0", "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  xb::bes::message_type msg;
  msg.name = "OptionalMsg";
  add_field(msg, make_field("required_field", 32));
  add_field(msg, make_field("optional_field", 32, std::nullopt, "0xFFFFFFFF"));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);
  CHECK(xsd.find("minOccurs=\"0\"") != std::string::npos);
}

TEST_CASE("generate_xsd: wire-field and constants omitted", "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  xb::bes::message_type msg;
  msg.name = "Msg";
  add_wire_field(msg, make_wire_field("header", 8));
  add_field(msg, make_field("data", 32));
  xb::bes::constant_type ct;
  ct.name = "VERSION";
  ct.value = "1";
  add_constant(msg, std::move(ct));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);
  CHECK(xsd.find("header") == std::string::npos);
  CHECK(xsd.find("VERSION") == std::string::npos);
  CHECK(xsd.find("data") != std::string::npos);
}

TEST_CASE("generate_xsd: string field gets maxLength facet", "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  xb::bes::message_type msg;
  msg.name = "StringMsg";
  add_field(msg,
            make_field("symbol", 64, xb::bes::primitive_encoding_type::ascii));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);
  CHECK(xsd.find("maxLength") != std::string::npos);
  CHECK(xsd.find("value=\"8\"") != std::string::npos);
}

TEST_CASE("generate_xsd: non-standard width gets restriction facets",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  xb::bes::message_type msg;
  msg.name = "BitMsg";
  add_field(msg, make_field("flags", 12,
                            xb::bes::primitive_encoding_type::unsigned_));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);
  CHECK(xsd.find("maxInclusive") != std::string::npos);
  CHECK(xsd.find("4095") != std::string::npos);
}

TEST_CASE("generate_xsd: no target-namespace produces unqualified XSD",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;

  xb::bes::message_type msg;
  msg.name = "Simple";
  add_field(msg, make_field("val", 16));
  enc.choice.push_back(std::move(msg));

  auto xsd = xb::wire::generate_xsd(enc);
  CHECK(xsd.find("targetNamespace") == std::string::npos);
}

// =========================================================================
// build_synthetic_schemas — schema_set from BES
// =========================================================================

TEST_CASE("build_synthetic_schemas: creates complex types for messages",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "http://example.com/itch";

  xb::bes::message_type msg;
  msg.name = "AddOrder";
  add_field(msg, make_field("stock_locate", 16));
  add_field(msg, make_field("shares", 32));
  enc.choice.push_back(std::move(msg));

  auto schemas = xb::wire::build_synthetic_schemas(enc);

  auto* ct = schemas.find_complex_type(
      xb::qname("http://example.com/itch", "AddOrder"));
  REQUIRE(ct != nullptr);
  CHECK(ct->name().local_name() == "AddOrder");
}

TEST_CASE("build_synthetic_schemas: complex type has correct elements",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  xb::bes::message_type msg;
  msg.name = "Trade";
  add_wire_field(msg, make_wire_field("msg_type", 8));
  add_field(msg, make_field("stock_locate", 16));
  add_field(msg, make_field("price", 32));
  enc.choice.push_back(std::move(msg));

  auto schemas = xb::wire::build_synthetic_schemas(enc);

  auto* ct = schemas.find_complex_type(xb::qname("urn:test", "Trade"));
  REQUIRE(ct != nullptr);

  // Verify element names in the content model
  auto names = xb::wire::element_names_of(*ct);
  CHECK(names.contains("stock_locate"));
  CHECK(names.contains("price"));
  CHECK_FALSE(names.contains("msg_type")); // wire-field excluded
}

TEST_CASE("build_synthetic_schemas: no target-namespace uses empty string",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;

  xb::bes::message_type msg;
  msg.name = "Msg";
  add_field(msg, make_field("val", 16));
  enc.choice.push_back(std::move(msg));

  auto schemas = xb::wire::build_synthetic_schemas(enc);
  auto* ct = schemas.find_complex_type(xb::qname("", "Msg"));
  REQUIRE(ct != nullptr);
}

TEST_CASE("build_synthetic_schemas: framings are not turned into types",
          "[bes_to_xsd]") {
  xb::bes::encoding_type enc;
  enc.target_namespace = "urn:test";

  // Add a framing (should be ignored)
  xb::bes::framing_type frm;
  frm.name = "ethernet";
  enc.choice.push_back(std::move(frm));

  // Add a message
  xb::bes::message_type msg;
  msg.name = "Msg";
  add_field(msg, make_field("val", 16));
  enc.choice.push_back(std::move(msg));

  auto schemas = xb::wire::build_synthetic_schemas(enc);
  CHECK(schemas.find_complex_type(xb::qname("urn:test", "ethernet")) ==
        nullptr);
  CHECK(schemas.find_complex_type(xb::qname("urn:test", "Msg")) != nullptr);
}
