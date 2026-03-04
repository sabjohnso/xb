#include <xb/wss_crypto.hpp>

#include <catch2/catch_test_macros.hpp>

#ifdef XB_HAS_OPENSSL

namespace crypto = xb::wss::crypto;

// Helper: make bytes from string
static std::vector<std::byte>
to_bytes(const std::string& s) {
  std::vector<std::byte> result;
  for (char c : s) {
    result.push_back(static_cast<std::byte>(c));
  }
  return result;
}

// Helper: format bytes as hex string
static std::string
to_hex(const std::vector<std::byte>& bytes) {
  static constexpr auto hex = "0123456789abcdef";
  std::string result;
  for (auto b : bytes) {
    auto v = static_cast<unsigned char>(b);
    result += hex[v >> 4];
    result += hex[v & 0xF];
  }
  return result;
}

// -- SHA-1 digest test vectors (NIST) -----------------------------------------

TEST_CASE("wss crypto: SHA-1 digest of empty input", "[wss_crypto]") {
  auto result = crypto::digest(crypto::hash_algorithm::sha1, {});
  CHECK(result.size() == 20);
  CHECK(to_hex(result) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST_CASE("wss crypto: SHA-1 digest of 'abc'", "[wss_crypto]") {
  auto result = crypto::digest(crypto::hash_algorithm::sha1, to_bytes("abc"));
  CHECK(result.size() == 20);
  CHECK(to_hex(result) == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

// -- SHA-256 digest test vectors ----------------------------------------------

TEST_CASE("wss crypto: SHA-256 digest of empty input", "[wss_crypto]") {
  auto result = crypto::digest(crypto::hash_algorithm::sha256, {});
  CHECK(result.size() == 32);
  CHECK(to_hex(result) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("wss crypto: SHA-256 digest of 'abc'", "[wss_crypto]") {
  auto result = crypto::digest(crypto::hash_algorithm::sha256, to_bytes("abc"));
  CHECK(result.size() == 32);
  CHECK(to_hex(result) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// -- HMAC test vectors (RFC 2202 / RFC 4231) ----------------------------------

TEST_CASE("wss crypto: HMAC-SHA1 with known vector", "[wss_crypto]") {
  // RFC 2202 test case 2: key = "Jefe", data = "what do ya want for nothing?"
  auto key = to_bytes("Jefe");
  auto data = to_bytes("what do ya want for nothing?");
  auto result = crypto::hmac(crypto::hash_algorithm::sha1, key, data);
  CHECK(to_hex(result) == "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}

TEST_CASE("wss crypto: HMAC-SHA256 with known vector", "[wss_crypto]") {
  // RFC 4231 test case 2: key = "Jefe", data = "what do ya want for nothing?"
  auto key = to_bytes("Jefe");
  auto data = to_bytes("what do ya want for nothing?");
  auto result = crypto::hmac(crypto::hash_algorithm::sha256, key, data);
  CHECK(to_hex(result) ==
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST_CASE("wss crypto: HMAC-SHA1 with empty data", "[wss_crypto]") {
  auto key = to_bytes("key");
  auto result = crypto::hmac(crypto::hash_algorithm::sha1, key, {});
  CHECK(result.size() == 20);
  CHECK_FALSE(result.empty());
}

// -- random_bytes -------------------------------------------------------------

TEST_CASE("wss crypto: random_bytes produces requested length",
          "[wss_crypto]") {
  auto bytes = crypto::random_bytes(16);
  CHECK(bytes.size() == 16);

  auto bytes32 = crypto::random_bytes(32);
  CHECK(bytes32.size() == 32);
}

TEST_CASE("wss crypto: random_bytes differs on successive calls",
          "[wss_crypto]") {
  auto a = crypto::random_bytes(16);
  auto b = crypto::random_bytes(16);
  CHECK(a != b);
}

TEST_CASE("wss crypto: random_bytes zero length", "[wss_crypto]") {
  auto result = crypto::random_bytes(0);
  CHECK(result.empty());
}

// -- compute_password_digest --------------------------------------------------

TEST_CASE("wss crypto: compute_password_digest matches OASIS known value",
          "[wss_crypto]") {
  // Password_Digest = Base64(SHA-1(nonce_bytes + created_utf8 + password_utf8))
  // Verified with: python3 -c "import base64,hashlib;
  //   print(base64.b64encode(hashlib.sha1(
  //     base64.b64decode('LKqI6G/AikKCQrN0zqZFlg==')
  //     + b'2003-07-16T01:24:32Z' + b'taadtaadpstcsm').digest()).decode())"
  auto result = crypto::compute_password_digest(
      "LKqI6G/AikKCQrN0zqZFlg==", "2003-07-16T01:24:32Z", "taadtaadpstcsm");
  CHECK(result == "vjwUgK9DkI4e3lonFkyGHvQPKlE=");
}

TEST_CASE("wss crypto: compute_password_digest is deterministic",
          "[wss_crypto]") {
  auto a = crypto::compute_password_digest("dGVzdA==", "2026-01-01T00:00:00Z",
                                           "password");
  auto b = crypto::compute_password_digest("dGVzdA==", "2026-01-01T00:00:00Z",
                                           "password");
  CHECK(a == b);
}

TEST_CASE("wss crypto: compute_password_digest differs with different password",
          "[wss_crypto]") {
  auto a = crypto::compute_password_digest("dGVzdA==", "2026-01-01T00:00:00Z",
                                           "password1");
  auto b = crypto::compute_password_digest("dGVzdA==", "2026-01-01T00:00:00Z",
                                           "password2");
  CHECK(a != b);
}

// -- constant_time_equal ------------------------------------------------------

TEST_CASE("wss crypto: constant_time_equal with identical strings",
          "[wss_crypto]") {
  CHECK(crypto::constant_time_equal("abc", "abc"));
  CHECK(crypto::constant_time_equal("", ""));
}

TEST_CASE("wss crypto: constant_time_equal with different strings",
          "[wss_crypto]") {
  CHECK_FALSE(crypto::constant_time_equal("abc", "abd"));
  CHECK_FALSE(crypto::constant_time_equal("abc", "ab"));
  CHECK_FALSE(crypto::constant_time_equal("abc", "abcd"));
}

#endif // XB_HAS_OPENSSL
