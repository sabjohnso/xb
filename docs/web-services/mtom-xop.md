# MTOM/XOP

xb implements [MTOM](https://www.w3.org/TR/soap12-mtom/) (Message Transmission
Optimization Mechanism) and [XOP](https://www.w3.org/TR/xop10/) (XML-binary
Optimized Packaging) for efficient binary data transfer in SOAP messages.

## Overview

MTOM/XOP optimizes SOAP messages containing binary data (images, documents,
etc.) by extracting large `base64Binary` elements into separate MIME parts,
avoiding the ~33% base64 encoding overhead.

![MTOM message structure](../images/mtom-structure-light.svg#only-light)
![MTOM message structure](../images/mtom-structure-dark.svg#only-dark)

## Namespaces

| Prefix | Namespace |
|--------|-----------|
| `xop` | `http://www.w3.org/2004/08/xop/include` |

## Data Types

### Attachments

```cpp
#include <xb/xop.hpp>

xb::xop::attachment att;
att.content_id = "image001@example.com";
att.content_type = "image/png";
att.data = load_file("photo.png");  // std::vector<std::byte>
```

### MTOM Messages

```cpp
xb::xop::mtom_message msg;
msg.envelope = soap_envelope;
msg.attachments.push_back(att);
```

## MIME Multipart

The `xb::mime` namespace handles MIME multipart serialization:

```cpp
#include <xb/mime_multipart.hpp>

// Build multipart message
xb::mime::multipart_message multipart;
multipart.boundary = xb::mime::generate_boundary();

// SOAP part
xb::mime::mime_part soap_part;
soap_part.content_type = "application/soap+xml";
soap_part.body = serialize_envelope(env);
multipart.parts.push_back(soap_part);

// Binary attachment part
xb::mime::mime_part bin_part;
bin_part.content_type = "image/png";
bin_part.content_id = "image001@example.com";
bin_part.content_transfer_encoding = "binary";
bin_part.body = raw_image_data;
multipart.parts.push_back(bin_part);
```

## MTOM Transport

The MTOM transport wraps a standard HTTP transport to handle MIME
serialization and deserialization automatically:

```cpp
#include <xb/mtom_transport.hpp>

xb::service::mtom_options opts;
opts.optimization_threshold = 1024;  // bytes; smaller values stay inline

xb::service::mtom_transport transport(http_transport, opts);
```

### Optimization Threshold

The `optimization_threshold` controls which binary fields are extracted into
MIME parts:

- Fields **larger** than the threshold are extracted (MTOM-optimized)
- Fields **smaller** are left as inline base64 in the SOAP body

Default: 1024 bytes.

## Requirements

MTOM/XOP requires libcurl for HTTP transport.
