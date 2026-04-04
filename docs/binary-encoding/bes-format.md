# BES Format Reference

A Binary Encoding Specification (BES) is an XML document in the
`http://xb.dev/encoding` namespace, validated by `schema/xb-encoding.xsd`.

## Document Structure

```xml
<encoding xmlns="http://xb.dev/encoding"
          target-namespace="http://example.com/protocol">

  <!-- Global defaults -->
  <defaults byte-order="big-endian"/>

  <!-- Optional imports -->
  <import href="base-encoding.bes.xml"/>

  <!-- Frame definitions (optional) -->
  <frame name="EthernetFrame">...</frame>

  <!-- Message definitions -->
  <message name="MyMessage" type="MyType" discriminant-value="0x01">
    ...
  </message>
</encoding>
```

## Root Element: `<encoding>`

| Attribute | Required | Description |
|-----------|----------|-------------|
| `target-namespace` | No | Target namespace URI for generated types |

## `<defaults>`

Sets default encoding properties applied to all fields unless overridden:

| Attribute | Values | Description |
|-----------|--------|-------------|
| `byte-order` | `big-endian`, `little-endian` | Default byte order |
| `bit-order` | `msb-first`, `lsb-first` | Default bit order |
| `alignment` | Integer (bits) | Default field alignment |
| `string-encoding` | `ascii`, `utf-8`, `utf-16` | Default string encoding |
| `string-padding` | `none`, `null`, `space` | Default string padding |

## `<message>`

Defines a binary message layout:

| Attribute | Required | Description |
|-----------|----------|-------------|
| `name` | Yes | Message name (used for generated class names) |
| `type` | No | XSD type name this message maps to |
| `discriminant-value` | No | Value that identifies this message type |

### Child Elements

#### `<field>`

A data field that maps to an XSD element:

| Attribute | Required | Description |
|-----------|----------|-------------|
| `name` | Yes | Field name |
| `bits` | Yes | Field width in bits |
| `encoding` | No | `unsigned`, `twos-complement`, `bcd`, `ieee754`, `ascii`, `utf-8`, `utf-16`, `fixed-point`, `epoch` |
| `byte-order` | No | Override byte order for this field |
| `alignment` | No | Override alignment for this field |

```xml
<field name="value" bits="32" encoding="unsigned"/>
<field name="symbol" bits="64" encoding="ascii"/>
<field name="temperature" bits="16" encoding="twos-complement"/>
```

#### `<wire-field>`

A field that exists only on the wire (not in the XSD type). Typically used
for message type discriminants, padding, or reserved bytes:

```xml
<wire-field name="msg_type" bits="8"/>
<wire-field name="reserved" bits="16"/>
```

Wire fields with a `discriminant-value` on the parent message are
automatically populated during construction.

#### `<computed-field>`

A field whose value is computed (e.g., checksum, CRC):

```xml
<computed-field name="checksum" bits="16"
                algorithm="crc16"
                byte-range="0:end"/>
```

#### `<group>`

A repeating group of fields:

```xml
<group name="items" count-field="item_count">
  <field name="item_id" bits="32" encoding="unsigned"/>
  <field name="quantity" bits="16" encoding="unsigned"/>
</group>
```

#### `<choice>`

A discriminated choice between alternatives:

```xml
<choice discriminant-field="msg_type">
  <alternative value="0x01" type="Heartbeat"/>
  <alternative value="0x02" type="Alert"/>
</choice>
```

## `<frame>`

Defines a protocol frame layer for multi-layer protocol parsing:

```xml
<frame name="EthernetFrame">
  <field name="dst_mac" bits="48" encoding="unsigned"/>
  <field name="src_mac" bits="48" encoding="unsigned"/>
  <field name="ether_type" bits="16" encoding="unsigned"/>
</frame>
```

Frames are composed into stacks. See [Protocol Framing](protocol-framing.md).

## `<import>`

Import and optionally override another BES document:

```xml
<import href="base-protocol.bes.xml"/>
```

Imported definitions can be overridden by redefining them in the importing
document.

## Encoding Types

| Encoding | Description | Typical Use |
|----------|-------------|-------------|
| `unsigned` | Unsigned integer | Counters, IDs, timestamps |
| `twos-complement` | Signed two's complement | Signed values |
| `bcd` | Binary-coded decimal | Packed numeric data |
| `ieee754` | IEEE 754 floating point | Scientific data |
| `fixed-point` | Fixed-point decimal | Rates, measurements |
| `ascii` | ASCII string (padded) | Symbols, identifiers |
| `utf-8` | UTF-8 string | Text fields |
| `utf-16` | UTF-16 string | Wide text fields |
| `epoch` | Epoch timestamp | Timestamps |
| `bitset` | Bit flags | Status flags |
