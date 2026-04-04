# WS-Security

xb implements [WS-Security 1.0](https://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0.pdf)
for SOAP message authentication. The implementation lives in the `xb::wss`
namespace, with cryptographic operations in `xb::wss::crypto`.

## Namespaces

| Prefix | Namespace |
|--------|-----------|
| `wsse` | `http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd` |
| `wsu` | `http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd` |

## Supported Token Types

### Username Token

```cpp
#include <xb/wss.hpp>

xb::wss::username_token token;
token.username = "alice";
token.password = "secret";
token.password_type = xb::wss::password_type_text;
// or: token.password_type = xb::wss::password_type_digest;
```

### X.509 Binary Security Token

For certificate-based authentication:

```cpp
xb::wss::binary_security_token bst;
bst.value_type = xb::wss::x509v3_value_type;
bst.encoding_type = xb::wss::base64_encoding_type;
bst.value = base64_encoded_cert;
```

### Timestamp

```cpp
xb::wss::timestamp ts;
ts.created = "2024-01-15T10:30:00Z";
ts.expires = "2024-01-15T10:35:00Z";  // optional
```

## Adding Security Headers

```cpp
#include <xb/wss_headers.hpp>

xb::soap::envelope env;
// ... build envelope ...

xb::wss::security_header security;
security.username_token = token;
security.timestamp = ts;

xb::wss::add_security_header(env, security);
```

## Extracting Security Headers

```cpp
#include <xb/wss_headers.hpp>

xb::wss::security_header security =
    xb::wss::extract_security_header(env);

if (security.username_token) {
    std::cout << "User: " << security.username_token->username << "\n";
}
```

## Header Pipeline Integration

```cpp
#include <xb/wss_handler.hpp>

xb::soap::header_pipeline pipeline;
xb::wss::security_header wss_headers;

xb::wss::register_wss_handlers(pipeline, wss_headers);
pipeline.process(env.headers);
```

## Cryptographic Operations

!!! note "Requires OpenSSL"
    Cryptographic functions require building xb with OpenSSL support.

```cpp
#include <xb/wss_crypto.hpp>

namespace crypto = xb::wss::crypto;

// Password digest (for PasswordDigest authentication)
std::string digest = crypto::compute_password_digest(
    nonce_base64, created_timestamp, password);

// Hash
auto sha256 = crypto::digest(
    crypto::hash_algorithm::sha256, data);

// HMAC
auto hmac = crypto::hmac(
    crypto::hash_algorithm::sha256, key, data);

// Random nonce
auto nonce = crypto::random_bytes(16);
```

### Password Digest Authentication

The WS-Security password digest is computed as:

```
Base64(SHA-1(Nonce + Created + Password))
```

```cpp
auto nonce = crypto::random_bytes(16);
std::string nonce_b64 = base64_encode(nonce);
std::string created = "2024-01-15T10:30:00Z";

xb::wss::username_token token;
token.username = "alice";
token.password = crypto::compute_password_digest(
    nonce_b64, created, "secret");
token.password_type = xb::wss::password_type_digest;
token.nonce = nonce_b64;
token.created = created;
```
