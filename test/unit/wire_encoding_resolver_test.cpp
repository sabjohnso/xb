#include <xb/wire/encoding_resolver.hpp>

#include <xb/wire/encoding.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace xb::bes;
using plan_type = xb::wire::encoding_plan<encoding_type>;

// ---------------------------------------------------------------------------
// Helpers: build minimal XSD schemas for testing
// ---------------------------------------------------------------------------

namespace {

  // Variadic helper to avoid vector-of-noncopyable issues
  template <typename... Types>
  xb::schema_set
  make_schema(const std::string& ns, Types&&... types) {
    xb::schema s;
    s.set_target_namespace(ns);
    (s.add_complex_type(std::forward<Types>(types)), ...);

    xb::schema_set ss;
    ss.add(std::move(s));
    ss.resolve();
    return ss;
  }

  xb::complex_type
  make_complex_type(const std::string& ns, const std::string& name) {
    return xb::complex_type(xb::qname{ns, name}, /*abstract=*/false,
                            /*mixed=*/false,
                            xb::content_type{xb::content_kind::element_only,
                                             xb::complex_content{}});
  }

  // Build a complex type with named elements in a sequence
  xb::complex_type
  make_complex_type_with_elements(const std::string& ns,
                                  const std::string& name,
                                  std::vector<std::string> element_names) {
    xb::model_group seq(xb::compositor_kind::sequence);
    for (auto& ename : element_names) {
      seq.add_particle(xb::particle(xb::element_decl(
          xb::qname{ns, ename},
          xb::qname{"http://www.w3.org/2001/XMLSchema", "string"})));
    }
    xb::complex_content cc;
    cc.content_model = std::move(seq);
    return xb::complex_type(
        xb::qname{ns, name}, /*abstract=*/false,
        /*mixed=*/false,
        xb::content_type{xb::content_kind::element_only, std::move(cc)});
  }

} // namespace

// ---------------------------------------------------------------------------
// encoding_plan: basic construction
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: empty encoding produces empty plan",
          "[wire][encoding_resolver]") {
  encoding_type enc;
  xb::schema_set schemas;
  schemas.resolve();

  plan_type plan(enc, schemas);
  CHECK(plan.messages().empty());
}

// ---------------------------------------------------------------------------
// encoding_plan: message binding via type attribute
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: message with type attribute binds to XSD type",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "OrderMessage"));

  encoding_type enc;
  enc.target_namespace = "http://example.com";
  message_type msg;
  msg.name = "Order";
  msg.type = "OrderMessage"; // local name, resolved via target_namespace
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  REQUIRE(plan.messages().size() == 1);
  CHECK(plan.messages()[0].target_type ==
        xb::qname{"http://example.com", "OrderMessage"});
  CHECK(plan.messages()[0].message->name == "Order");
}

TEST_CASE("encoding_plan: message with Clark notation type attribute",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "OrderMessage"));

  encoding_type enc;
  message_type msg;
  msg.name = "Order";
  msg.type = "{http://example.com}OrderMessage";
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  REQUIRE(plan.messages().size() == 1);
  CHECK(plan.messages()[0].target_type ==
        xb::qname{"http://example.com", "OrderMessage"});
}

// ---------------------------------------------------------------------------
// encoding_plan: message binding via selector qname
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: selector qname binds to XSD type",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "TradeMessage"));

  encoding_type enc;
  message_type msg;
  msg.name = "Trade";
  selector_type sel;
  sel.qname = "{http://example.com}TradeMessage";
  msg.selector = std::move(sel);
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  REQUIRE(plan.messages().size() == 1);
  CHECK(plan.messages()[0].target_type ==
        xb::qname{"http://example.com", "TradeMessage"});
}

// ---------------------------------------------------------------------------
// encoding_plan: selector namespace matches all types
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: selector namespace matches types in namespace",
          "[wire][encoding_resolver]") {
  auto schemas = make_schema("http://example.com",
                             make_complex_type("http://example.com", "TypeA"),
                             make_complex_type("http://example.com", "TypeB"));

  encoding_type enc;
  message_type msg;
  msg.name = "AllExampleTypes";
  selector_type sel;
  sel.namespace_ = "http://example.com";
  msg.selector = std::move(sel);
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  // Namespace selector matches multiple types
  CHECK(plan.messages().size() == 2);
}

// ---------------------------------------------------------------------------
// encoding_plan: specificity - qname wins over namespace
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: qname selector wins over namespace selector",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "OrderMessage"));

  encoding_type enc;

  // Namespace-level encoding
  message_type ns_msg;
  ns_msg.name = "DefaultEncoding";
  ns_msg.byte_order = byte_order_type::big_endian;
  selector_type ns_sel;
  ns_sel.namespace_ = "http://example.com";
  ns_msg.selector = std::move(ns_sel);
  enc.choice.push_back(std::move(ns_msg));

  // Specific qname encoding (should win)
  message_type qn_msg;
  qn_msg.name = "OrderEncoding";
  qn_msg.byte_order = byte_order_type::little_endian;
  selector_type qn_sel;
  qn_sel.qname = "{http://example.com}OrderMessage";
  qn_msg.selector = std::move(qn_sel);
  enc.choice.push_back(std::move(qn_msg));

  plan_type plan(enc, schemas);
  // Only the qname match should win for OrderMessage
  auto* bound =
      plan.find_message(xb::qname{"http://example.com", "OrderMessage"});
  REQUIRE(bound != nullptr);
  CHECK(bound->message->name == "OrderEncoding");
  CHECK(bound->message->byte_order == byte_order_type::little_endian);
}

// ---------------------------------------------------------------------------
// encoding_plan: framing lookup
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: framing lookup by name",
          "[wire][encoding_resolver]") {
  xb::schema_set schemas;
  schemas.resolve();

  encoding_type enc;
  framing_type fr;
  fr.name = "itch_frame";
  fr.byte_order = byte_order_type::big_endian;
  enc.choice.push_back(std::move(fr));

  plan_type plan(enc, schemas);
  auto* found = plan.find_framing("itch_frame");
  REQUIRE(found != nullptr);
  CHECK(found->name == "itch_frame");
  CHECK(found->byte_order == byte_order_type::big_endian);

  CHECK(plan.find_framing("nonexistent") == nullptr);
}

// ---------------------------------------------------------------------------
// encoding_plan: frame-stack lookup
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: frame-stack lookup by name",
          "[wire][encoding_resolver]") {
  xb::schema_set schemas;
  schemas.resolve();

  encoding_type enc;

  xb::bes::framing_type framing;
  framing.name = "ethernet";
  enc.choice.push_back(std::move(framing));

  frame_stack_type fs;
  fs.name = "itch_multicast";
  layer_type l;
  l.ref = "ethernet";
  fs.layer.push_back(std::move(l));
  enc.choice.push_back(std::move(fs));

  plan_type plan(enc, schemas);
  auto* found = plan.find_frame_stack("itch_multicast");
  REQUIRE(found != nullptr);
  CHECK(found->name == "itch_multicast");
  REQUIRE(found->layer.size() == 1);
  CHECK(found->layer[0].ref == "ethernet");

  CHECK(plan.find_frame_stack("nonexistent") == nullptr);
}

// ---------------------------------------------------------------------------
// encoding_plan: defaults
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: defaults from encoding document",
          "[wire][encoding_resolver]") {
  xb::schema_set schemas;
  schemas.resolve();

  encoding_type enc;
  defaults_type defs;
  defs.byte_order = byte_order_type::little_endian;
  defs.alignment = 32;
  enc.defaults = std::move(defs);

  plan_type plan(enc, schemas);
  CHECK(plan.defaults().byte_order == byte_order_type::little_endian);
  CHECK(*plan.defaults().alignment == 32);
}

TEST_CASE("encoding_plan: defaults empty when not specified",
          "[wire][encoding_resolver]") {
  xb::schema_set schemas;
  schemas.resolve();

  encoding_type enc;
  plan_type plan(enc, schemas);
  CHECK(!plan.defaults().byte_order.has_value());
}

// ---------------------------------------------------------------------------
// encoding_plan: message references framing
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: message framing reference resolves",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "OrderMessage"));

  encoding_type enc;
  enc.target_namespace = "http://example.com";

  framing_type fr;
  fr.name = "order_frame";
  enc.choice.push_back(std::move(fr));

  message_type msg;
  msg.name = "Order";
  msg.type = "OrderMessage";
  msg.framing = "order_frame";
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  REQUIRE(plan.messages().size() == 1);
  auto& bound = plan.messages()[0];
  CHECK(bound.message->framing == "order_frame");

  // The framing should be findable
  auto* fr_ptr = plan.find_framing("order_frame");
  REQUIRE(fr_ptr != nullptr);
  CHECK(fr_ptr->name == "order_frame");
}

// ---------------------------------------------------------------------------
// encoding_plan: unresolved type is an error
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: message with type not in schema throws",
          "[wire][encoding_resolver]") {
  xb::schema_set schemas;
  schemas.resolve();

  encoding_type enc;
  enc.target_namespace = "http://example.com";
  message_type msg;
  msg.name = "Order";
  msg.type = "NonexistentType";
  enc.choice.push_back(std::move(msg));

  CHECK_THROWS_AS(plan_type(enc, schemas), std::runtime_error);
}

// ---------------------------------------------------------------------------
// encoding_plan: multiple messages bind to different types
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: multiple messages bind independently",
          "[wire][encoding_resolver]") {
  auto schemas =
      make_schema("http://example.com",
                  make_complex_type("http://example.com", "OrderMessage"),
                  make_complex_type("http://example.com", "TradeMessage"));

  encoding_type enc;
  enc.target_namespace = "http://example.com";

  message_type msg1;
  msg1.name = "Order";
  msg1.type = "OrderMessage";
  enc.choice.push_back(std::move(msg1));

  message_type msg2;
  msg2.name = "Trade";
  msg2.type = "TradeMessage";
  enc.choice.push_back(std::move(msg2));

  plan_type plan(enc, schemas);
  REQUIRE(plan.messages().size() == 2);

  auto* order =
      plan.find_message(xb::qname{"http://example.com", "OrderMessage"});
  auto* trade =
      plan.find_message(xb::qname{"http://example.com", "TradeMessage"});
  REQUIRE(order != nullptr);
  REQUIRE(trade != nullptr);
  CHECK(order->message->name == "Order");
  CHECK(trade->message->name == "Trade");
}

// ---------------------------------------------------------------------------
// Frame-stack reference validation
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: valid framing ref does not throw",
          "[wire][encoding_resolver]") {
  encoding_type enc;
  xb::schema_set schemas;
  schemas.resolve();

  xb::bes::framing_type framing;
  framing.name = "ethernet";
  enc.choice.push_back(std::move(framing));

  xb::bes::frame_stack_type fs;
  fs.name = "my-stack";
  layer_type layer;
  layer.ref = "ethernet";
  fs.layer.push_back(std::move(layer));
  enc.choice.push_back(std::move(fs));

  plan_type plan(enc, schemas);
  CHECK(plan.find_frame_stack("my-stack") != nullptr);
}

TEST_CASE("encoding_plan: unknown framing ref throws descriptive error",
          "[wire][encoding_resolver]") {
  encoding_type enc;
  xb::schema_set schemas;
  schemas.resolve();

  xb::bes::frame_stack_type fs;
  fs.name = "bad-stack";
  layer_type layer;
  layer.ref = "nonexistent";
  fs.layer.push_back(std::move(layer));
  enc.choice.push_back(std::move(fs));

  CHECK_THROWS_WITH(plan_type(enc, schemas),
                    Catch::Matchers::ContainsSubstring("nonexistent") &&
                        Catch::Matchers::ContainsSubstring("bad-stack"));
}

// ---------------------------------------------------------------------------
// Field name cross-validation against XSD element names
// ---------------------------------------------------------------------------

TEST_CASE("encoding_plan: field names matching XSD elements are accepted",
          "[wire][encoding_resolver]") {
  auto ct = make_complex_type_with_elements(
      "http://example.com", "Order", {"stock_locate", "shares", "price"});
  auto schemas = make_schema("http://example.com", std::move(ct));

  encoding_type enc;
  enc.target_namespace = "http://example.com";

  message_type msg;
  msg.name = "Order";
  msg.type = "Order";

  field_type f1;
  f1.name = "stock_locate";
  f1.bits = 16;
  msg.choice.push_back(std::move(f1));

  field_type f2;
  f2.name = "shares";
  f2.bits = 32;
  msg.choice.push_back(std::move(f2));

  field_type f3;
  f3.name = "price";
  f3.bits = 32;
  msg.choice.push_back(std::move(f3));

  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  CHECK(plan.messages().size() == 1);
}

TEST_CASE("encoding_plan: field name not in XSD type throws error",
          "[wire][encoding_resolver]") {
  auto ct = make_complex_type_with_elements("http://example.com", "Order",
                                            {"stock_locate", "shares"});
  auto schemas = make_schema("http://example.com", std::move(ct));

  encoding_type enc;
  enc.target_namespace = "http://example.com";

  message_type msg;
  msg.name = "Order";
  msg.type = "Order";

  field_type f1;
  f1.name = "stock_locate";
  f1.bits = 16;
  msg.choice.push_back(std::move(f1));

  field_type f2;
  f2.name = "nonexistent_field";
  f2.bits = 32;
  msg.choice.push_back(std::move(f2));

  enc.choice.push_back(std::move(msg));

  CHECK_THROWS_WITH(plan_type(enc, schemas),
                    Catch::Matchers::ContainsSubstring("nonexistent_field") &&
                        Catch::Matchers::ContainsSubstring("Order"));
}

TEST_CASE("encoding_plan: wire-field name not in XSD type is exempt",
          "[wire][encoding_resolver]") {
  auto ct = make_complex_type_with_elements("http://example.com", "Order",
                                            {"stock_locate", "shares"});
  auto schemas = make_schema("http://example.com", std::move(ct));

  encoding_type enc;
  enc.target_namespace = "http://example.com";

  message_type msg;
  msg.name = "Order";
  msg.type = "Order";

  // wire-field: no XSD counterpart expected
  wire_field_type wf;
  wf.name = "msg_type";
  wf.bits = 8;
  msg.choice.push_back(std::move(wf));

  field_type f1;
  f1.name = "stock_locate";
  f1.bits = 16;
  msg.choice.push_back(std::move(f1));

  field_type f2;
  f2.name = "shares";
  f2.bits = 32;
  msg.choice.push_back(std::move(f2));

  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);
  CHECK(plan.messages().size() == 1);
}
