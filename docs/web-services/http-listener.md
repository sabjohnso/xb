# HTTP Listener

The `http_listener` class is a built-in HTTP server for hosting SOAP services.
It implements the `soap_listener` interface using POSIX sockets and lives in
the `xb::service` namespace.

!!! note "Requires POSIX sockets"
    `http_listener` is available on Linux and macOS (`XB_HAS_POSIX_SOCKETS`
    is defined). Windows users can implement the `soap_listener` interface
    with their own HTTP server.

## Basic Usage

```cpp
#include <xb/http_listener.hpp>

xb::service::http_listener server({.port = 8080});

// serve() blocks until stop() is called
server.serve([](const std::string& soap_action,
                const xb::soap::envelope& request)
                 -> xb::soap::envelope {
    // Handle the request and return a response
    if (soap_action == "http://example.com/MyAction") {
        return handle_my_action(request);
    }
    throw std::runtime_error("Unknown action");
});
```

## Options

```cpp
xb::service::http_listener_options opts;
opts.bind_address = "127.0.0.1";  // localhost only
opts.port = 0;                     // OS-assigned port
opts.path = "/";
opts.backlog = 16;

xb::service::http_listener server(opts);
auto port = server.listening_port();  // actual assigned port
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `bind_address` | `string` | `"0.0.0.0"` | IP address to bind to |
| `port` | `uint16_t` | `8080` | Port number (0 for OS-assigned) |
| `path` | `string` | `"/"` | URL path to serve on |
| `backlog` | `int` | `16` | TCP listen backlog |

## Shutdown

Call `stop()` from another thread to shut down the listener:

```cpp
xb::service::http_listener server({.port = 8080});

std::thread server_thread([&] {
    server.serve(my_handler);
});

// ... later ...
server.stop();
server_thread.join();
```

## Error Handling

The listener automatically converts exceptions to SOAP faults:

- If the handler throws `std::exception`, the listener returns an HTTP 500
  response with a SOAP fault containing the exception message
- Non-POST requests receive HTTP 405
- Malformed requests receive HTTP 400

## SOAP Version Detection

The listener detects the SOAP version from the incoming Content-Type:

| Content-Type | Detected Version | SOAPAction Source |
|-------------|-----------------|-------------------|
| `text/xml` | SOAP 1.1 | `SOAPAction` HTTP header |
| `application/soap+xml` | SOAP 1.2 | `action` parameter in Content-Type |

## Complete Example

A server and client communicating over HTTP on localhost:

```cpp
#include <xb/http_listener.hpp>
#include <xb/http_transport.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/wsdl_support.hpp>
#include <thread>

// Server handler
xb::soap::envelope
handle_request(const std::string& action,
               const xb::soap::envelope& request) {
    auto input = xb::service::parse_body_element<MyRequestType>(
        request.body.front(), read_my_request_type);

    MyResponseType output = process(input);

    auto body = xb::service::make_body_element(
        xb::qname{ns, "MyResponse"}, output, write_response);

    xb::soap::envelope response;
    response.version = request.version;
    response.body.push_back(body);
    return response;
}

int main() {
    // Start server
    xb::service::http_listener server(
        {.bind_address = "127.0.0.1", .port = 0});
    auto port = server.listening_port();

    std::thread server_thread([&] { server.serve(handle_request); });

    // Make client call
    xb::service::http_transport client;
    std::string endpoint = "http://127.0.0.1:" + std::to_string(port);

    xb::soap::envelope request;
    // ... build request ...
    auto response = client.call(endpoint, "MyAction", request);
    xb::service::check_fault(response);

    // Shutdown
    server.stop();
    server_thread.join();
}
```

See `examples/wsdl-client/` for a complete working example with typed
request/response marshalling and fault handling.

## Listener Interface

`http_listener` implements the abstract `soap_listener` interface:

```cpp
namespace xb::service {
    using soap_handler =
        std::function<soap::envelope(const std::string& soap_action,
                                     const soap::envelope& request)>;

    class soap_listener {
    public:
        virtual ~soap_listener() = default;
        virtual void serve(soap_handler handler) = 0;
        virtual void stop() = 0;
    };
}
```

You can implement `soap_listener` with a different HTTP server library
(e.g., Boost.Beast, cpp-httplib) if you need features beyond what the
built-in listener provides (TLS termination, thread pools, HTTP/2, etc.).
