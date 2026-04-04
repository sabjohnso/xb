# SOAP

xb provides a SOAP 1.1 and 1.2 runtime for building and consuming SOAP
messages. The implementation lives in the `xb::soap` namespace.

## SOAP Versions

| Version | Namespace | Encoding Style |
|---------|-----------|----------------|
| SOAP 1.1 | `http://schemas.xmlsoap.org/soap/envelope/` | Document/Literal or RPC/Encoded |
| SOAP 1.2 | `http://www.w3.org/2003/05/soap-envelope` | Document/Literal |

## Envelope Model

A SOAP envelope consists of headers and a body:

```cpp
#include <xb/soap_model.hpp>

xb::soap::envelope env;
env.version = xb::soap::soap_version::v1_2;

// Add body elements
env.body.push_back(my_request_element);

// Add headers (optional)
xb::soap::header_block hdr;
hdr.content = my_header_element;
hdr.must_understand = true;
env.headers.push_back(hdr);
```

### Data Types

```cpp
namespace xb::soap {
    enum soap_version { v1_1, v1_2 };

    struct header_block {
        any_element content;
        bool must_understand = false;
        std::string role;
    };

    struct envelope {
        soap_version version = soap_version::v1_1;
        std::vector<header_block> headers;
        std::vector<any_element> body;
    };
}
```

## Reading and Writing Envelopes

```cpp
#include <xb/soap_envelope.hpp>

// Parse a SOAP envelope from XML
auto reader = xb::expat_reader(input);
xb::soap::envelope env = xb::soap::read_envelope(reader);

// Write a SOAP envelope to XML
auto writer = xb::ostream_writer(output);
xb::soap::write_envelope(writer, env);
```

## SOAP Faults

```cpp
#include <xb/soap_fault.hpp>

// Check if a body element is a fault
if (xb::soap::is_fault(env.body.front(), env.version)) {
    auto fault = xb::soap::read_fault(reader, env.version);
    // Handle fault...
}

// Write a fault response
xb::soap::fault f;
f.code = "soap:Server";
f.reason = "Internal error";
xb::soap::write_fault(writer, f, xb::soap::soap_version::v1_2);
```

## Header Pipeline

The header pipeline processes SOAP headers through registered handlers:

```cpp
#include <xb/soap_header.hpp>

xb::soap::header_pipeline pipeline;

// Register a handler — returns true if the header was processed
pipeline.add([](const xb::soap::header_block& hdr) -> bool {
    if (hdr.content.name().local_name() == "MyHeader") {
        // Process header
        return true;
    }
    return false;
});

// Process headers — throws soap_fault_exception for
// unhandled mustUnderstand headers
pipeline.process(env.headers);
```

## Integration with WSDL

When using WSDL-generated client stubs, SOAP envelopes are managed
automatically. See [WSDL](wsdl.md) for code generation from WSDL definitions.

## Integration with WS-* Extensions

The SOAP header pipeline integrates with:

- [WS-Addressing](ws-addressing.md) — message routing and correlation
- [WS-Security](ws-security.md) — authentication and encryption
- [MTOM/XOP](mtom-xop.md) — binary-optimized packaging
