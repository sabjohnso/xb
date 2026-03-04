#ifdef XB_HAS_OPENSSL

#include <xb/wss_crypto.hpp>

#include <xb/xml_value.hpp>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <climits>
#include <memory>
#include <stdexcept>

namespace xb::wss::crypto {

  namespace {

    const char*
    md_name(hash_algorithm algo) {
      switch (algo) {
        case hash_algorithm::sha1:
          return "SHA1";
        case hash_algorithm::sha256:
          return "SHA256";
      }
      return nullptr;
    }

    const EVP_MD*
    get_md(hash_algorithm algo) {
      switch (algo) {
        case hash_algorithm::sha1:
          return EVP_sha1();
        case hash_algorithm::sha256:
          return EVP_sha256();
      }
      return nullptr;
    }

    // RAII wrapper for EVP_MD_CTX
    struct md_ctx_deleter {
      void
      operator()(EVP_MD_CTX* ctx) const {
        if (ctx) EVP_MD_CTX_free(ctx);
      }
    };

    using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, md_ctx_deleter>;

  } // namespace

  std::vector<std::byte>
  digest(hash_algorithm algo, const std::vector<std::byte>& data) {
    const EVP_MD* md = get_md(algo);
    if (!md) throw std::runtime_error("unsupported hash algorithm");

    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    md_ctx_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), buf, &len) != 1) {
      throw std::runtime_error("EVP digest failed");
    }

    std::vector<std::byte> result(len);
    for (unsigned int i = 0; i < len; ++i) {
      result[i] = static_cast<std::byte>(buf[i]);
    }

    OPENSSL_cleanse(buf, sizeof(buf));
    return result;
  }

  std::vector<std::byte>
  hmac(hash_algorithm algo, const std::vector<std::byte>& key,
       const std::vector<std::byte>& data) {
    const char* name = md_name(algo);
    if (!name) throw std::runtime_error("unsupported hash algorithm");

    // Use EVP_MAC API (non-deprecated, available since OpenSSL 3.0)
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) throw std::runtime_error("EVP_MAC_fetch failed");

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!ctx) throw std::runtime_error("EVP_MAC_CTX_new failed");

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>(name), 0),
        OSSL_PARAM_construct_end()};

    if (EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(key.data()),
                     key.size(), params) != 1) {
      EVP_MAC_CTX_free(ctx);
      throw std::runtime_error("EVP_MAC_init failed");
    }

    if (EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) != 1) {
      EVP_MAC_CTX_free(ctx);
      throw std::runtime_error("EVP_MAC_update failed");
    }

    unsigned char buf[EVP_MAX_MD_SIZE];
    size_t len = 0;
    if (EVP_MAC_final(ctx, buf, &len, sizeof(buf)) != 1) {
      EVP_MAC_CTX_free(ctx);
      throw std::runtime_error("EVP_MAC_final failed");
    }

    EVP_MAC_CTX_free(ctx);

    std::vector<std::byte> result(len);
    for (size_t i = 0; i < len; ++i) {
      result[i] = static_cast<std::byte>(buf[i]);
    }

    OPENSSL_cleanse(buf, sizeof(buf));
    return result;
  }

  std::vector<std::byte>
  random_bytes(std::size_t count) {
    if (count == 0) return {};
    if (count > static_cast<std::size_t>(INT_MAX)) {
      throw std::runtime_error("random_bytes: count exceeds INT_MAX");
    }

    std::vector<std::byte> result(count);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(result.data()),
                   static_cast<int>(count)) != 1) {
      throw std::runtime_error("RAND_bytes failed");
    }
    return result;
  }

  std::string
  compute_password_digest(const std::string& nonce_base64,
                          const std::string& created,
                          const std::string& password) {
    // Decode nonce from base64
    auto nonce_bytes = parse_base64_binary(nonce_base64);

    // Concatenate: nonce + created + password
    std::vector<std::byte> input;
    input.reserve(nonce_bytes.size() + created.size() + password.size());
    input.insert(input.end(), nonce_bytes.begin(), nonce_bytes.end());
    for (char c : created) {
      input.push_back(static_cast<std::byte>(c));
    }
    for (char c : password) {
      input.push_back(static_cast<std::byte>(c));
    }

    // SHA-1 digest per OASIS UsernameToken Profile 1.0 Section 3.1
    auto hash = digest(hash_algorithm::sha1, input);

    // Zero sensitive intermediate
    OPENSSL_cleanse(input.data(), input.size());

    // Base64 encode
    return format_base64_binary(hash);
  }

  bool
  constant_time_equal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
  }

} // namespace xb::wss::crypto

#endif // XB_HAS_OPENSSL
