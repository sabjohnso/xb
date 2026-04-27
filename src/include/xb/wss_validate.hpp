#pragma once

/// @file
/// Validation helpers for WS-Security timestamps and tokens.  Separate
/// from the data-only @ref xb/wss.hpp so callers that only need the
/// types do not pull in @c <chrono> machinery.

#include <xb/wss.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace xb::wss {

  /// Outcome of validating a @ref timestamp / @ref username_token
  /// against the current clock.
  enum class validation_status {
    ok,
    expired,   ///< Expires already past, or Created older than skew window.
    future,    ///< Created is more than the skew window in the future.
    malformed, ///< Created or Expires could not be parsed as ISO 8601.
  };

  /// Knobs for @ref validate_timestamp / @ref verify_username_token.
  struct validate_timestamp_options {
    /// How far the message clock may differ from this node's clock
    /// before the timestamp is rejected.  Defaults to 1 minute, which
    /// is tighter than the SAML/Kerberos convention of 5 minutes;
    /// callers with looser tolerance should override explicitly.
    std::chrono::seconds max_clock_skew{60};
  };

  /// Validate a WS-Security @ref timestamp against @p now.
  ///
  /// - When @c Expires is present, it must be no earlier than
  ///   @c now - max_clock_skew.
  /// - When @c Expires is absent, @c Created must be no earlier than
  ///   @c now - max_clock_skew (i.e. the timestamp itself must be
  ///   recent).
  /// - In either case, @c Created must be no later than
  ///   @c now + max_clock_skew, so a peer with a wildly fast clock
  ///   cannot mint future-dated tokens.
  ///
  /// @p now defaults to @c system_clock::now() at second precision,
  /// but is parameterised so tests can pin it.
  validation_status
  validate_timestamp(const timestamp& ts, validate_timestamp_options opts = {},
                     std::chrono::sys_seconds now =
                         std::chrono::time_point_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now()));

#ifdef XB_HAS_OPENSSL

  /// Outcome of @ref verify_username_token.
  enum class authentication_status {
    ok,
    unknown_user,        ///< Lookup returned no password for the username.
    wrong_password,      ///< Digest did not match the expected value.
    missing_credentials, ///< Required field (nonce/created/digest) absent.
    expired,             ///< Created is older than the skew window.
    future,              ///< Created is in the future beyond the skew window.
    malformed,           ///< Created could not be parsed.
  };

  /// Function that returns the password for a given username, or
  /// @c std::nullopt if the user is unknown.  The receiver supplies
  /// this; xb does not store passwords.
  using password_lookup_fn =
      std::function<std::optional<std::string>(const std::string& username)>;

  /// Verify a WS-Security UsernameToken in digest mode.
  ///
  /// Recomputes @c Base64(H(nonce + created + password)) using the
  /// algorithm in @ref username_token::digest_algorithm and compares
  /// the result to the value in @ref username_token::password using a
  /// constant-time comparison.  @c Created is bounded against the same
  /// skew window used by @ref validate_timestamp, so a stale token
  /// cannot replay forever.
  ///
  /// Plaintext mode (@c password_text_type) is rejected as
  /// @c missing_credentials — callers that need to accept plaintext
  /// must compare passwords themselves over a TLS connection.  This
  /// helper is for digest mode only.
  authentication_status
  verify_username_token(const username_token& ut,
                        const password_lookup_fn& lookup,
                        validate_timestamp_options opts = {},
                        std::chrono::sys_seconds now =
                            std::chrono::time_point_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now()));

#endif // XB_HAS_OPENSSL

} // namespace xb::wss
