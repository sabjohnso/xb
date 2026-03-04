#include <xb/wss.hpp>

#include <catch2/catch_test_macros.hpp>

namespace wss = xb::wss;

// -- Namespace constants ------------------------------------------------------

TEST_CASE("wss: wsse namespace URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::wsse_ns) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-wssecurity-secext-1.0.xsd");
}

TEST_CASE("wss: wsu namespace URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::wsu_ns) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-wssecurity-utility-1.0.xsd");
}

TEST_CASE("wss: password text type URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::password_text_type) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-username-token-profile-1.0#PasswordText");
}

TEST_CASE("wss: password digest type URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::password_digest_type) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-username-token-profile-1.0#PasswordDigest");
}

TEST_CASE("wss: x509 token type URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::x509_token_type) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-x509-token-profile-1.0#X509v3");
}

TEST_CASE("wss: base64 encoding type URI matches OASIS spec", "[wss]") {
  CHECK(std::string(wss::base64_encoding_type) ==
        "http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-soap-message-security-1.0#Base64Binary");
}

// -- username_token -----------------------------------------------------------

TEST_CASE("wss: username_token default construction", "[wss]") {
  wss::username_token ut;
  CHECK(ut.username.empty());
  CHECK(ut.password.empty());
  CHECK(ut.password_type == wss::password_text_type);
  CHECK_FALSE(ut.nonce.has_value());
  CHECK_FALSE(ut.created.has_value());
}

TEST_CASE("wss: username_token equality", "[wss]") {
  wss::username_token a;
  a.username = "alice";
  a.password = "secret";
  a.nonce = "bm9uY2U=";
  a.created = "2026-01-01T00:00:00Z";

  wss::username_token b = a;
  CHECK(a == b);

  b.username = "bob";
  CHECK_FALSE(a == b);

  // Verify each field contributes to equality
  b = a;
  b.password = "other";
  CHECK_FALSE(a == b);

  b = a;
  b.password_type = std::string(wss::password_digest_type);
  CHECK_FALSE(a == b);

  b = a;
  b.nonce = "different";
  CHECK_FALSE(a == b);

  b = a;
  b.created = "2026-02-02T00:00:00Z";
  CHECK_FALSE(a == b);

  b = a;
  b.nonce = std::nullopt;
  CHECK_FALSE(a == b);
}

TEST_CASE("wss: username_token with nonce and created", "[wss]") {
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  ut.password_type = std::string(wss::password_digest_type);
  ut.nonce = "dGVzdG5vbmNl";
  ut.created = "2026-03-01T00:00:00Z";

  CHECK(ut.nonce.has_value());
  CHECK(ut.created.has_value());
  CHECK(ut.password_type == wss::password_digest_type);
}

// -- timestamp ----------------------------------------------------------------

TEST_CASE("wss: timestamp default construction", "[wss]") {
  wss::timestamp ts;
  CHECK(ts.created.empty());
  CHECK_FALSE(ts.expires.has_value());
}

TEST_CASE("wss: timestamp equality", "[wss]") {
  wss::timestamp a;
  a.created = "2026-03-01T00:00:00Z";
  a.expires = "2026-03-01T00:05:00Z";

  wss::timestamp b = a;
  CHECK(a == b);

  b.expires = "2026-03-01T01:00:00Z";
  CHECK_FALSE(a == b);

  // expires present vs absent
  b = a;
  b.expires = std::nullopt;
  CHECK_FALSE(a == b);

  // created differs
  b = a;
  b.created = "2026-04-01T00:00:00Z";
  CHECK_FALSE(a == b);
}

// -- binary_security_token ----------------------------------------------------

TEST_CASE("wss: binary_security_token default construction", "[wss]") {
  wss::binary_security_token bst;
  CHECK(bst.value_type.empty());
  CHECK(bst.encoding_type == wss::base64_encoding_type);
  CHECK(bst.value.empty());
  CHECK_FALSE(bst.wsu_id.has_value());
}

TEST_CASE("wss: binary_security_token equality", "[wss]") {
  wss::binary_security_token a;
  a.value_type = std::string(wss::x509_token_type);
  a.value = "MIIB...";
  a.wsu_id = "cert1";

  wss::binary_security_token b = a;
  CHECK(a == b);

  b.value = "MIIC...";
  CHECK_FALSE(a == b);

  b = a;
  b.encoding_type = "http://other#encoding";
  CHECK_FALSE(a == b);

  b = a;
  b.wsu_id = "cert2";
  CHECK_FALSE(a == b);

  b = a;
  b.wsu_id = std::nullopt;
  CHECK_FALSE(a == b);
}

TEST_CASE("wss: binary_security_token with wsu_id", "[wss]") {
  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...";
  bst.wsu_id = "X509Token";

  CHECK(bst.wsu_id.has_value());
  CHECK(*bst.wsu_id == "X509Token");
}

// -- security_header ----------------------------------------------------------

TEST_CASE("wss: security_header default construction", "[wss]") {
  wss::security_header sh;
  CHECK_FALSE(sh.username.has_value());
  CHECK_FALSE(sh.ts.has_value());
  CHECK_FALSE(sh.binary_token.has_value());
}

TEST_CASE("wss: security_header equality", "[wss]") {
  wss::security_header a;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  a.username = ut;
  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  a.ts = ts;
  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.value = "MIIB...";
  a.binary_token = bst;

  wss::security_header b = a;
  CHECK(a == b);

  // Each optional field contributes to equality
  b = a;
  b.ts->expires = "2026-03-01T00:05:00Z";
  CHECK_FALSE(a == b);

  b = a;
  b.username->username = "bob";
  CHECK_FALSE(a == b);

  b = a;
  b.binary_token->value = "MIIC...";
  CHECK_FALSE(a == b);

  b = a;
  b.binary_token = std::nullopt;
  CHECK_FALSE(a == b);
}

TEST_CASE("wss: security_header with all fields", "[wss]") {
  wss::security_header sh;
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "secret";
  sh.username = ut;

  wss::timestamp ts;
  ts.created = "2026-03-01T00:00:00Z";
  ts.expires = "2026-03-01T00:05:00Z";
  sh.ts = ts;

  wss::binary_security_token bst;
  bst.value_type = std::string(wss::x509_token_type);
  bst.encoding_type = std::string(wss::base64_encoding_type);
  bst.value = "MIIB...";
  bst.wsu_id = "X509Token";
  sh.binary_token = bst;

  CHECK(sh.username.has_value());
  CHECK(sh.ts.has_value());
  CHECK(sh.binary_token.has_value());
}
