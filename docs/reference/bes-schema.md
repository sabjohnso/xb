# BES Schema Reference

The Binary Encoding Specification format is defined by
`schema/xb-encoding.xsd` in the `http://xb.dev/encoding` namespace. This page
documents the complete element and attribute vocabulary.

## Document Structure

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="URI">
  <defaults .../>
  <import href="..."/>
  <frame name="...">...</frame>
  <message name="..." type="..." discriminant-value="...">
    ...
  </message>
</encoding>
```

## Elements

### `<encoding>` (root)

| Attribute | Type | Description |
|-----------|------|-------------|
| `target-namespace` | URI | Namespace for generated types |

### `<defaults>`

Global encoding defaults applied to all fields:

| Attribute | Type | Values |
|-----------|------|--------|
| `byte-order` | enum | `big-endian`, `little-endian` |
| `bit-order` | enum | `msb-first`, `lsb-first` |
| `alignment` | int | Field alignment in bits |
| `string-encoding` | enum | `ascii`, `utf-8`, `utf-16` |
| `string-padding` | enum | `none`, `null`, `space` |

### `<import>`

| Attribute | Type | Description |
|-----------|------|-------------|
| `href` | URI | Path to imported BES document |

### `<frame>`

Protocol frame layer definition:

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | string | Frame name (used for generated class names) |

Children: `<field>`, `<wire-field>`

### `<message>`

Application message definition:

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | string | Message name |
| `type` | string | XSD type this message maps to |
| `discriminant-value` | string | Value identifying this message type (hex or decimal) |

Children: `<field>`, `<wire-field>`, `<computed-field>`, `<group>`, `<choice>`

### `<field>`

Data field (maps to XSD element):

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | string | (required) | Field name |
| `bits` | int | (required) | Width in bits |
| `encoding` | enum | `unsigned` | Encoding type |
| `byte-order` | enum | from defaults | Byte order override |
| `bit-order` | enum | from defaults | Bit order override |
| `alignment` | int | from defaults | Alignment override |

### `<wire-field>`

Wire-only field (not in XSD type):

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | string | Field name |
| `bits` | int | Width in bits |

Wire fields are auto-populated from `discriminant-value` during `_owned`
construction.

### `<computed-field>`

Computed field (checksum, CRC):

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | string | Field name |
| `bits` | int | Width in bits |
| `algorithm` | string | Computation algorithm |
| `byte-range` | string | Range specification (e.g., `0:end`) |

### `<group>`

Repeating group:

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | string | Group name |
| `count-field` | string | Field name containing the count |

Children: `<field>`, `<wire-field>`

### `<choice>`

Discriminated choice:

| Attribute | Type | Description |
|-----------|------|-------------|
| `discriminant-field` | string | Field name used for dispatch |

Children: `<alternative>`

### `<alternative>`

Choice alternative:

| Attribute | Type | Description |
|-----------|------|-------------|
| `value` | string | Discriminant value |
| `type` | string | Message/type name for this alternative |

## Encoding Types

| Value | Description | Typical Widths |
|-------|-------------|---------------|
| `unsigned` | Unsigned integer | 8, 16, 32, 48, 64 |
| `twos-complement` | Signed two's complement | 8, 16, 32, 64 |
| `bcd` | Binary-coded decimal | 4n bits |
| `ieee754` | IEEE 754 floating point | 32, 64 |
| `fixed-point` | Fixed-point decimal | any |
| `ascii` | ASCII string (padded to width) | 8n bits |
| `utf-8` | UTF-8 string | variable |
| `utf-16` | UTF-16 string | 16n bits |
| `epoch` | Epoch timestamp | 32, 64 |
| `bitset` | Bit flags | any |

## Validation

BES files are validated against `schema/xb-encoding.xsd`. Use any
XSD-validating parser to check a BES file:

```sh
xmllint --schema schema/xb-encoding.xsd protocol.bes.xml
```
