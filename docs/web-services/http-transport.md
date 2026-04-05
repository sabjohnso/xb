# HTTP Transport

The `http_transport` class is the built-in HTTP client for making SOAP calls.
It implements the `transport` interface using libcurl and lives in the
`xb::service` namespace.

!!! note "Requires libcurl"
    `http_transport` is only available when xb is built with libcurl
    (`XB_HAS_CURL` is defined).

## Basic Usage

```cpp
#include <xb/http_transport.hpp>

xb::service::http_transport client;

xb::soap::envelope request;
request.version = xb::soap::soap_version::v1_2;
// ... populate request body ...

auto response = client.call(
    "https://example.com/service",
    "http://example.com/action/MyOperation",
    request);
```

## Options

Configure the transport with `http_options`:

```cpp
xb::service::http_options opts;
opts.connect_timeout = std::chrono::milliseconds{5000};
opts.request_timeout = std::chrono::milliseconds{30000};
opts.follow_redirects = true;

xb::service::http_transport client(opts);
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `connect_timeout` | `chrono::milliseconds` | 30000 | TCP connection timeout |
| `request_timeout` | `chrono::milliseconds` | 60000 | Total request timeout |
| `follow_redirects` | `bool` | `true` | Follow HTTP 3xx redirects |
| `verify_peer` | `bool` | `true` | Verify TLS peer certificate |
| `ca_bundle` | `string` | `""` | Path to CA certificate bundle |
| `client_cert` | `string` | `""` | Path to client TLS certificate |
| `client_key` | `string` | `""` | Path to client TLS private key |

## TLS / Mutual Authentication

```cpp
xb::service::http_options opts;
opts.verify_peer = true;
opts.ca_bundle = "/etc/ssl/certs/ca-certificates.crt";
opts.client_cert = "/path/to/client.pem";
opts.client_key = "/path/to/client-key.pem";

xb::service::http_transport client(opts);
```

## SOAP Version Handling

The transport automatically sets the correct HTTP Content-Type based on the
envelope's SOAP version:

| SOAP Version | Content-Type | SOAPAction |
|-------------|-------------|------------|
| 1.1 | `text/xml; charset=utf-8` | Separate `SOAPAction` HTTP header |
| 1.2 | `application/soap+xml; charset=utf-8; action="..."` | Embedded in Content-Type |

## Error Handling

- Unreachable endpoints throw `transport_error`
- HTTP 500 responses with a valid SOAP envelope are returned normally
  (the caller can check for faults via `check_fault()`)
- HTTP 500 responses without a valid SOAP body throw `transport_error`

```cpp
try {
    auto response = client.call(endpoint, action, request);
    xb::service::check_fault(response);  // throws soap_call_fault
} catch (const xb::service::soap_call_fault& e) {
    // SOAP-level fault
} catch (const xb::service::transport_error& e) {
    // HTTP-level error (connection refused, timeout, etc.)
}
```

## Transport Interface

`http_transport` implements the abstract `transport` interface:

```cpp
namespace xb::service {
    class transport {
    public:
        virtual ~transport() = default;
        virtual soap::envelope
        call(const std::string& endpoint,
             const std::string& soap_action,
             const soap::envelope& request) = 0;
    };
}
```

You can substitute any implementation of `transport` — for testing, use an
in-process mock; for production, use `http_transport` or `mtom_transport`.
