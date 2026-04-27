#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xb::wss::crypto {

  /// Hash algorithm selector.  Visible regardless of @c XB_HAS_OPENSSL
  /// because @ref xb::wss::username_token uses it as the type of its
  /// @c digest_algorithm field, even in builds without crypto.
  enum class hash_algorithm { sha1, sha256 };

#ifdef XB_HAS_OPENSSL

  std::vector<std::byte>
  digest(hash_algorithm algo, const std::vector<std::byte>& data);

  std::vector<std::byte>
  hmac(hash_algorithm algo, const std::vector<std::byte>& key,
       const std::vector<std::byte>& data);

  std::vector<std::byte>
  random_bytes(std::size_t count);

  /// Compute the WS-Security UsernameToken password digest:
  ///   Base64(H(nonce_bytes + created + password))
  /// where @p algo selects the hash function.  Defaults to SHA-256;
  /// the OASIS UsernameToken Profile 1.0 spec mandates SHA-1, which
  /// remains available as an opt-in for legacy interop.
  std::string
  compute_password_digest(const std::string& nonce_base64,
                          const std::string& created,
                          const std::string& password,
                          hash_algorithm algo = hash_algorithm::sha256);

  /// Constant-time comparison for digest verification
  /// (timing side-channel safe).
  bool
  constant_time_equal(const std::string& a, const std::string& b);

#endif // XB_HAS_OPENSSL

} // namespace xb::wss::crypto
