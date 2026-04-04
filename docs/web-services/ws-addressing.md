# WS-Addressing

xb implements [WS-Addressing](https://www.w3.org/TR/ws-addr-core/) for SOAP
message routing, correlation, and endpoint management. The implementation
lives in the `xb::wsa` namespace.

## Namespace

```
http://www.w3.org/2005/08/addressing
```

## Data Types

```cpp
#include <xb/wsa.hpp>

namespace xb::wsa {
    struct endpoint_reference {
        std::string address;
    };

    struct relates_to {
        std::string uri;
        std::string relationship_type;
    };

    struct addressing_headers {
        std::optional<std::string> to;
        std::optional<std::string> action;
        std::optional<std::string> message_id;
        std::optional<relates_to> relates_to;
        std::optional<endpoint_reference> reply_to;
        std::optional<endpoint_reference> fault_to;
        std::optional<endpoint_reference> from;
    };
}
```

## Adding Addressing Headers

```cpp
#include <xb/wsa_headers.hpp>

xb::soap::envelope env;
// ... build envelope ...

xb::wsa::addressing_headers headers;
headers.to = "http://example.com/service";
headers.action = "http://example.com/action/GetOrder";
headers.message_id = "urn:uuid:12345";

xb::wsa::add_addressing_headers(env, headers);
```

## Extracting Addressing Headers

```cpp
#include <xb/wsa_headers.hpp>

xb::soap::envelope env = xb::soap::read_envelope(reader);

xb::wsa::addressing_headers headers =
    xb::wsa::extract_addressing_headers(env);

if (headers.action) {
    std::cout << "Action: " << *headers.action << "\n";
}
if (headers.message_id) {
    std::cout << "MessageID: " << *headers.message_id << "\n";
}
```

## Header Pipeline Integration

Register WS-Addressing handlers with the SOAP header pipeline for automatic
processing:

```cpp
#include <xb/wsa_handler.hpp>

xb::soap::header_pipeline pipeline;
xb::wsa::addressing_headers wsa_headers;

// Register — headers are extracted automatically during processing
xb::wsa::register_wsa_handlers(pipeline, wsa_headers);

pipeline.process(env.headers);

// wsa_headers is now populated
```

## Reply Correlation

Use `relates_to` to correlate a response with its request:

```cpp
// Request
xb::wsa::addressing_headers req;
req.message_id = "urn:uuid:request-123";
req.reply_to = xb::wsa::endpoint_reference{
    "http://www.w3.org/2005/08/addressing/anonymous"
};

// Response
xb::wsa::addressing_headers resp;
resp.relates_to = xb::wsa::relates_to{
    .uri = "urn:uuid:request-123",
    .relationship_type = "http://www.w3.org/2005/08/addressing/reply"
};
```
