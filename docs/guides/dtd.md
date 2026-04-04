# DTD

xb parses Document Type Definitions and translates them to the internal XSD
model for code generation.

## Usage

### CLI

```sh
xb generate --header-only -o gen/ schema.dtd
```

### CMake

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.dtd
  MODE HEADER_ONLY)
```

## How Translation Works

DTD constructs are mapped to XSD equivalents:

| DTD Construct | XSD Equivalent |
|---------------|---------------|
| `<!ELEMENT>` with children | `xs:complexType` with `xs:sequence` / `xs:choice` |
| `<!ELEMENT>` with `EMPTY` | Empty `xs:complexType` |
| `<!ELEMENT>` with `ANY` | `xs:complexType` with wildcard |
| `<!ELEMENT>` with `(#PCDATA)` | `xs:string` simple type |
| `<!ELEMENT>` with mixed content | Mixed `xs:complexType` |
| `<!ATTLIST>` `CDATA` | `xs:attribute` of type `xs:string` |
| `<!ATTLIST>` enumerated | `xs:simpleType` with `xs:enumeration` |
| `<!ATTLIST>` `ID` | `xs:attribute` of type `xs:ID` |
| `<!ATTLIST>` `IDREF` | `xs:attribute` of type `xs:IDREF` |
| `<!ATTLIST>` `NMTOKEN` | `xs:attribute` of type `xs:NMTOKEN` |
| `?` (optional) | `minOccurs="0" maxOccurs="1"` |
| `*` (zero or more) | `minOccurs="0" maxOccurs="unbounded"` |
| `+` (one or more) | `minOccurs="1" maxOccurs="unbounded"` |
| `\|` (choice) | `xs:choice` |
| `,` (sequence) | `xs:sequence` |

## Example

Given a DTD:

```dtd
<!ELEMENT note (to, from, heading, body)>
<!ATTLIST note priority (low | normal | high) "normal">
<!ELEMENT to (#PCDATA)>
<!ELEMENT from (#PCDATA)>
<!ELEMENT heading (#PCDATA)>
<!ELEMENT body (#PCDATA)>
```

xb generates:

```cpp
enum class priority { low, normal, high };

struct note {
    priority priority = priority::normal;
    std::string to;
    std::string from;
    std::string heading;
    std::string body;
};
```

## Converting

```sh
# DTD to XSD
xb convert -f xsd schema.dtd

# DTD to RELAX NG
xb convert -f rng schema.dtd
```

See [Schema Conversion](schema-conversion.md) for the full conversion matrix.

## Limitations

DTDs are less expressive than XSD:

- **No namespaces** — DTD has no namespace support; generated types use a
  default namespace
- **No type reuse** — DTD elements define inline content models; the
  translator infers shared types where possible
- **Limited data types** — attribute types (`CDATA`, `ID`, `IDREF`, etc.)
  map to XSD built-in types, but there are no numeric or date types
