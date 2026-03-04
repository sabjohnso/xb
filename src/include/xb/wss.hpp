#pragma once

#include <optional>
#include <string>

namespace xb::wss {

  // OASIS WS-Security 1.0 namespace URIs
  inline constexpr auto wsse_ns = "http://docs.oasis-open.org/wss/2004/01/"
                                  "oasis-200401-wss-wssecurity-secext-1.0.xsd";
  inline constexpr auto wsu_ns = "http://docs.oasis-open.org/wss/2004/01/"
                                 "oasis-200401-wss-wssecurity-utility-1.0.xsd";

  // Password type URIs
  inline constexpr auto password_text_type =
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-username-token-profile-1.0#PasswordText";
  inline constexpr auto password_digest_type =
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-username-token-profile-1.0#PasswordDigest";

  // Token profile URIs
  inline constexpr auto x509_token_type =
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-x509-token-profile-1.0#X509v3";
  inline constexpr auto base64_encoding_type =
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-soap-message-security-1.0#Base64Binary";

  struct username_token {
    std::string username;
    std::string password;
    std::string password_type = std::string(password_text_type);
    std::optional<std::string> nonce;
    std::optional<std::string> created;

    bool
    operator==(const username_token&) const = default;
  };

  struct timestamp {
    std::string created;
    std::optional<std::string> expires;

    bool
    operator==(const timestamp&) const = default;
  };

  struct binary_security_token {
    std::string value_type;
    std::string encoding_type = std::string(base64_encoding_type);
    std::string value;
    std::optional<std::string> wsu_id;

    bool
    operator==(const binary_security_token&) const = default;
  };

  struct security_header {
    std::optional<username_token> username;
    std::optional<timestamp> ts;
    std::optional<binary_security_token> binary_token;

    bool
    operator==(const security_header&) const = default;
  };

} // namespace xb::wss
