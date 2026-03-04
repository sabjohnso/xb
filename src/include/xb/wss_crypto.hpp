#pragma once

#ifdef XB_HAS_OPENSSL

#include <cstddef>
#include <string>
#include <vector>

namespace xb::wss::crypto {

  enum class hash_algorithm { sha1, sha256 };

  std::vector<std::byte>
  digest(hash_algorithm algo, const std::vector<std::byte>& data);

  std::vector<std::byte>
  hmac(hash_algorithm algo, const std::vector<std::byte>& key,
       const std::vector<std::byte>& data);

  std::vector<std::byte>
  random_bytes(std::size_t count);

  // WS-Security password digest: Base64(SHA-1(nonce + created + password))
  std::string
  compute_password_digest(const std::string& nonce_base64,
                          const std::string& created,
                          const std::string& password);

  // Constant-time comparison for digest verification (timing side-channel safe)
  bool
  constant_time_equal(const std::string& a, const std::string& b);

} // namespace xb::wss::crypto

#endif // XB_HAS_OPENSSL
