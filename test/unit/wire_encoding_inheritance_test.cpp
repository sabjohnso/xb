#include <xb/wire/encoding_resolver.hpp>

#include <xb/wire/encoding.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/schema.hpp>
#include <xb/schema_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace xb::bes;
using namespace xb::wire;

using plan_type = encoding_plan<encoding_type>;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

  xb::complex_type
  make_complex_type(const std::string& ns, const std::string& name) {
    return xb::complex_type(xb::qname{ns, name}, false, false,
                            xb::content_type{xb::content_kind::element_only,
                                             xb::complex_content{}});
  }

  xb::complex_type
  make_derived_type(const std::string& ns, const std::string& name,
                    const std::string& base_ns, const std::string& base_name) {
    return xb::complex_type(
        xb::qname{ns, name}, false, false,
        xb::content_type{
            xb::content_kind::element_only,
            xb::complex_content{xb::qname{base_ns, base_name},
                                xb::derivation_method::extension}});
  }

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

} // namespace

// ---------------------------------------------------------------------------
// Encoding inheritance: derived type inherits base's encoding
// ---------------------------------------------------------------------------

TEST_CASE("encoding_inheritance: derived type inherits base encoding",
          "[wire][encoding_inheritance]") {
  const std::string ns = "http://example.com";

  auto schemas =
      make_schema(ns, make_complex_type(ns, "BaseMessage"),
                  make_derived_type(ns, "DerivedMessage", ns, "BaseMessage"));

  encoding_type enc;
  enc.target_namespace = ns;

  // Only base has an explicit BES message
  message_type msg;
  msg.name = "Base";
  msg.type = "BaseMessage";
  field_type f;
  f.name = "value";
  f.bits = 32;
  msg.choice.push_back(std::move(f));
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);

  // Base should be bound
  auto* base_binding = plan.find_message(xb::qname{ns, "BaseMessage"});
  REQUIRE(base_binding != nullptr);
  CHECK(base_binding->message->name == "Base");

  // Derived should inherit base's encoding
  auto* derived_binding = plan.find_message(xb::qname{ns, "DerivedMessage"});
  REQUIRE(derived_binding != nullptr);
  // Points to the same message as the base
  CHECK(derived_binding->message == base_binding->message);
}

TEST_CASE("encoding_inheritance: explicit binding overrides inheritance",
          "[wire][encoding_inheritance]") {
  const std::string ns = "http://example.com";

  auto schemas =
      make_schema(ns, make_complex_type(ns, "BaseMessage"),
                  make_derived_type(ns, "DerivedMessage", ns, "BaseMessage"));

  encoding_type enc;
  enc.target_namespace = ns;

  // Base message
  message_type base_msg;
  base_msg.name = "Base";
  base_msg.type = "BaseMessage";
  field_type f1;
  f1.name = "value";
  f1.bits = 32;
  base_msg.choice.push_back(std::move(f1));
  enc.choice.push_back(std::move(base_msg));

  // Derived has its own explicit encoding
  message_type derived_msg;
  derived_msg.name = "Derived";
  derived_msg.type = "DerivedMessage";
  field_type f2;
  f2.name = "value";
  f2.bits = 64; // different width
  derived_msg.choice.push_back(std::move(f2));
  enc.choice.push_back(std::move(derived_msg));

  plan_type plan(enc, schemas);

  auto* base_binding = plan.find_message(xb::qname{ns, "BaseMessage"});
  auto* derived_binding = plan.find_message(xb::qname{ns, "DerivedMessage"});

  REQUIRE(base_binding != nullptr);
  REQUIRE(derived_binding != nullptr);

  // Derived should use its own encoding, not the base's
  CHECK(derived_binding->message->name == "Derived");
  CHECK(derived_binding->message != base_binding->message);
}

TEST_CASE("encoding_inheritance: no inheritance for restriction",
          "[wire][encoding_inheritance]") {
  const std::string ns = "http://example.com";

  // Make a restricted type (not extension)
  xb::complex_type restricted(
      xb::qname{ns, "RestrictedMessage"}, false, false,
      xb::content_type{
          xb::content_kind::element_only,
          xb::complex_content{xb::qname{ns, "BaseMessage"},
                              xb::derivation_method::restriction}});

  auto schemas = make_schema(ns, make_complex_type(ns, "BaseMessage"),
                             std::move(restricted));

  encoding_type enc;
  enc.target_namespace = ns;

  message_type msg;
  msg.name = "Base";
  msg.type = "BaseMessage";
  field_type f;
  f.name = "value";
  f.bits = 32;
  msg.choice.push_back(std::move(f));
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);

  // Base should be bound
  CHECK(plan.find_message(xb::qname{ns, "BaseMessage"}) != nullptr);
  // Restricted type should NOT inherit (restriction may narrow the type)
  CHECK(plan.find_message(xb::qname{ns, "RestrictedMessage"}) == nullptr);
}

TEST_CASE("encoding_inheritance: transitive inheritance A -> B -> C",
          "[wire][encoding_inheritance]") {
  const std::string ns = "http://example.com";

  auto schemas = make_schema(ns, make_complex_type(ns, "A"),
                             make_derived_type(ns, "B", ns, "A"),
                             make_derived_type(ns, "C", ns, "B"));

  encoding_type enc;
  enc.target_namespace = ns;

  message_type msg;
  msg.name = "AMsg";
  msg.type = "A";
  field_type f;
  f.name = "x";
  f.bits = 16;
  msg.choice.push_back(std::move(f));
  enc.choice.push_back(std::move(msg));

  plan_type plan(enc, schemas);

  auto* a = plan.find_message(xb::qname{ns, "A"});
  auto* b = plan.find_message(xb::qname{ns, "B"});
  auto* c = plan.find_message(xb::qname{ns, "C"});

  REQUIRE(a != nullptr);
  REQUIRE(b != nullptr);
  REQUIRE(c != nullptr);
  CHECK(b->message == a->message);
  CHECK(c->message == a->message);
}

TEST_CASE("encoding_inheritance: no base encoding means no inheritance",
          "[wire][encoding_inheritance]") {
  const std::string ns = "http://example.com";

  auto schemas = make_schema(ns, make_complex_type(ns, "Unbound"),
                             make_derived_type(ns, "Child", ns, "Unbound"));

  encoding_type enc;
  enc.target_namespace = ns;
  // No messages at all

  plan_type plan(enc, schemas);

  CHECK(plan.find_message(xb::qname{ns, "Unbound"}) == nullptr);
  CHECK(plan.find_message(xb::qname{ns, "Child"}) == nullptr);
}
