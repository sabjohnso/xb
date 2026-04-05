# WSDL

xb parses WSDL 1.1 and WSDL 2.0 service definitions, resolves them to a
version-independent service IR, and generates C++ client stubs and server
skeletons.

## Supported Versions

| Version | Namespace | Parser |
|---------|-----------|--------|
| WSDL 1.1 | `http://schemas.xmlsoap.org/wsdl/` | `xb::wsdl_parser` |
| WSDL 2.0 | `http://www.w3.org/ns/wsdl` | `xb::wsdl2_parser` |

## Architecture

![WSDL flow: WSDL 1.1/2.0 to Service IR to generated code](../images/wsdl-flow-light.svg#only-light)
![WSDL flow: WSDL 1.1/2.0 to Service IR to generated code](../images/wsdl-flow-dark.svg#only-dark)

Both WSDL versions resolve to the same `xb::service::service_description` IR,
so code generation works identically regardless of the input version.

## Parsing WSDL

### WSDL 1.1

```cpp
#include <xb/wsdl_parser.hpp>
#include <xb/wsdl_resolver.hpp>

auto reader = xb::expat_reader(input);
xb::wsdl_parser parser;
xb::wsdl::document doc = parser.parse(reader);

// Resolve to version-independent service IR
xb::wsdl_resolver resolver;
xb::service::service_description desc = resolver.resolve(doc);
```

### WSDL 2.0

```cpp
#include <xb/wsdl2_parser.hpp>
#include <xb/wsdl2_resolver.hpp>

auto reader = xb::expat_reader(input);
xb::wsdl2_parser parser;
xb::wsdl2::description desc = parser.parse(reader);

// Resolve to the same service IR
xb::wsdl2_resolver resolver;
xb::service::service_description svc = resolver.resolve(desc);
```

## Service IR

The resolved service model (`xb::service` namespace) is version-independent:

| Type | Description |
|------|-------------|
| `service_description` | Top-level container for all resolved services |
| `resolved_operation` | A single operation with input/output/fault parts |
| `resolved_part` | A message part with XSD name, C++ type, and read/write functions |
| `resolved_fault` | A fault with name and detail part |
| `resolved_port` | A service endpoint with operations and SOAP version |

The `resolved_port.soap_ver` field carries the SOAP version (1.1 or 1.2)
through to code generation.

## Code Generation

```cpp
#include <xb/wsdl_codegen.hpp>

xb::wsdl_codegen codegen;
std::vector<xb::cpp_file> files = codegen.generate(service_desc);
```

This generates:

- **Client stub** — a class with methods for each operation, handling SOAP
  envelope construction, transport, and response parsing
- **Server skeleton** — an abstract base class with virtual methods for each
  operation, plus a dispatch method that routes incoming SOAP requests
- **Support utilities** — serialization helpers via `xb::service` support
  functions

## WSDL 2.0 Specifics

WSDL 2.0 has several differences from 1.1 that xb handles:

### Interface Inheritance

WSDL 2.0 interfaces can extend other interfaces:

```xml
<wsdl:interface name="ExtendedService" extends="tns:BaseService">
  <wsdl:operation name="newOperation" .../>
</wsdl:interface>
```

The resolver flattens inheritance — child interfaces include all parent
operations. If a child redefines an operation with the same name, the child's
version takes precedence.

### Message Exchange Patterns

WSDL 2.0 uses MEPs instead of operation types:

| MEP | Description |
|-----|-------------|
| `in-only` | One-way input |
| `robust-in-only` | One-way input with fault |
| `in-out` | Request-response |
| `in-optional-out` | Request with optional response |

### Default SOAP Version

WSDL 2.0 defaults to SOAP 1.2 (vs. 1.1 for WSDL 1.1). The SOAP binding
extension namespace is `http://www.w3.org/ns/wsdl/soap`.

## Transport

xb defines an abstract transport interface for SOAP communication:

```cpp
#include <xb/wsdl_transport.hpp>

class my_transport : public xb::service::transport {
public:
    xb::soap::envelope
    call(const std::string& endpoint,
         const std::string& soap_action,
         const xb::soap::envelope& request) override;
};
```

For HTTP transport with libcurl, see [HTTP Transport](http-transport.md).

## Hosting a SOAP Service

Use `http_listener` to host a SOAP service. The handler function receives
a SOAPAction and request envelope, and returns a response envelope:

```cpp
#include <xb/http_listener.hpp>

xb::service::http_listener server({.port = 8080});

server.serve([&](const std::string& action,
                 const xb::soap::envelope& request) {
    // Dispatch by SOAPAction, parse typed request, return typed response
    return dispatcher.dispatch(action, request);
});
```

The listener automatically handles SOAP version detection, HTTP error
responses, and exception-to-fault conversion. See
[HTTP Listener](http-listener.md) for full details.

## Complete Example

See `examples/wsdl-client/` for a working example with separate client and
server executables communicating over HTTP on localhost:

```sh
# Terminal 1: start the server
./weather_server 8080

# Terminal 2: run the client
./weather_client http://127.0.0.1:8080/ Springfield Shelbyville Atlantis
```

The server dispatches by SOAPAction, parses typed requests, and returns
typed responses (or SOAP faults). The client uses `http_transport` to make
calls, checks for faults, and parses typed responses.
