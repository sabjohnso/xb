/// @file
/// Tests for xb::wss::validate_timestamp — checks that Created/Expires
/// outside the configured skew window are rejected, while well-formed
/// recent timestamps pass.

#include <xb/wss.hpp>
#include <xb/wss_validate.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <sstream>
#include <string>

namespace wss = xb::wss;
using namespace std::chrono_literals;

namespace {

  /// Format a sys_seconds as a WSS-style ISO 8601 timestamp ending in Z.
  std::string
  iso8601_utc(std::chrono::sys_seconds tp) {
    auto days = std::chrono::floor<std::chrono::days>(tp);
    std::chrono::year_month_day ymd{days};
    std::chrono::hh_mm_ss<std::chrono::seconds> hms{tp - days};
    std::ostringstream os;
    os << static_cast<int>(ymd.year()) << '-';
    os.fill('0');
    os.width(2);
    os << static_cast<unsigned>(ymd.month()) << '-';
    os.width(2);
    os << static_cast<unsigned>(ymd.day()) << 'T';
    os.width(2);
    os << hms.hours().count() << ':';
    os.width(2);
    os << hms.minutes().count() << ':';
    os.width(2);
    os << hms.seconds().count() << 'Z';
    return os.str();
  }

} // namespace

TEST_CASE("wss validate_timestamp accepts a current timestamp",
          "[wss_validate]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now);
  ts.expires = iso8601_utc(now + 60s);

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::ok);
}

TEST_CASE("wss validate_timestamp rejects an expired timestamp",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now - 3600s);
  ts.expires = iso8601_utc(now - 3000s); // 50 min ago

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::expired);
}

TEST_CASE("wss validate_timestamp rejects future-dated Created beyond skew",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now + 3600s); // 1 hour ahead

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::future);
}

TEST_CASE("wss validate_timestamp tolerates Created within the default skew",
          "[wss_validate]") {
  // Default skew is 1 minute.
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now + 30s); // half the default skew, ahead

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::ok);
}

TEST_CASE("wss validate_timestamp rejects malformed Created",
          "[wss_validate]") {
  wss::timestamp ts;
  ts.created = "not a date";

  auto result = wss::validate_timestamp(
      ts, {},
      std::chrono::time_point_cast<std::chrono::seconds>(
          std::chrono::system_clock::now()));
  CHECK(result == wss::validation_status::malformed);
}

TEST_CASE("wss validate_timestamp accepts when only Created is present and "
          "recent",
          "[wss_validate]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now);
  // No Expires — that is permitted; just verify Created is recent.

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::ok);
}

TEST_CASE("wss validate_timestamp rejects Created older than skew when no "
          "Expires given",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::timestamp ts;
  ts.created = iso8601_utc(now - 3600s); // 1 hour old, no Expires

  auto result = wss::validate_timestamp(ts, {}, now);
  CHECK(result == wss::validation_status::expired);
}

// -- verify_username_token (digest mode) --------------------------------------

#ifdef XB_HAS_OPENSSL

#include <xb/wss_crypto.hpp>

TEST_CASE("wss verify_username_token: valid SHA-256 digest accepted",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "alice";
  ut.nonce = "LKqI6G/AikKCQrN0zqZFlg==";
  ut.created = iso8601_utc(now);
  ut.password = wss::crypto::compute_password_digest(
      *ut.nonce, *ut.created, "alice-secret",
      wss::crypto::hash_algorithm::sha256);

  auto lookup = [](const std::string& user) -> std::optional<std::string> {
    if (user == "alice") return "alice-secret";
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::ok);
}

TEST_CASE("wss verify_username_token: wrong password rejected",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "alice";
  ut.nonce = "LKqI6G/AikKCQrN0zqZFlg==";
  ut.created = iso8601_utc(now);
  // Digest computed with the wrong password
  ut.password = wss::crypto::compute_password_digest(
      *ut.nonce, *ut.created, "guess", wss::crypto::hash_algorithm::sha256);

  auto lookup = [](const std::string& user) -> std::optional<std::string> {
    if (user == "alice") return "alice-secret";
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::wrong_password);
}

TEST_CASE("wss verify_username_token: unknown user rejected",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "mallory";
  ut.nonce = "LKqI6G/AikKCQrN0zqZFlg==";
  ut.created = iso8601_utc(now);
  ut.password = wss::crypto::compute_password_digest(
      *ut.nonce, *ut.created, "anything", wss::crypto::hash_algorithm::sha256);

  auto lookup = [](const std::string&) -> std::optional<std::string> {
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::unknown_user);
}

TEST_CASE("wss verify_username_token: stale Created rejected",
          "[wss_validate][security]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "alice";
  ut.nonce = "LKqI6G/AikKCQrN0zqZFlg==";
  ut.created = iso8601_utc(now - 3600s); // 1 hour old
  ut.password = wss::crypto::compute_password_digest(
      *ut.nonce, *ut.created, "alice-secret",
      wss::crypto::hash_algorithm::sha256);

  auto lookup = [](const std::string& user) -> std::optional<std::string> {
    if (user == "alice") return "alice-secret";
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::expired);
}

TEST_CASE("wss verify_username_token: plaintext mode is not accepted",
          "[wss_validate][security]") {
  // The helper is digest-mode only.  Callers that need to accept
  // plaintext do so themselves over a TLS-protected transport.
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "alice";
  ut.password = "alice-secret";
  ut.password_type = std::string(wss::password_text_type);

  auto lookup = [](const std::string& user) -> std::optional<std::string> {
    if (user == "alice") return "alice-secret";
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::missing_credentials);
}

TEST_CASE("wss verify_username_token: SHA-1 opt-in still verifies",
          "[wss_validate]") {
  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
  wss::username_token ut;
  ut.username = "alice";
  ut.nonce = "LKqI6G/AikKCQrN0zqZFlg==";
  ut.created = iso8601_utc(now);
  ut.digest_algorithm = wss::crypto::hash_algorithm::sha1;
  ut.password = wss::crypto::compute_password_digest(
      *ut.nonce, *ut.created, "alice-secret",
      wss::crypto::hash_algorithm::sha1);

  auto lookup = [](const std::string& user) -> std::optional<std::string> {
    if (user == "alice") return "alice-secret";
    return std::nullopt;
  };
  CHECK(wss::verify_username_token(ut, lookup, {}, now) ==
        wss::authentication_status::ok);
}

#endif // XB_HAS_OPENSSL
