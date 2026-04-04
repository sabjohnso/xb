# Sample Documents

The `sample-doc` subcommand generates minimal conforming XML instances from a
schema. This is useful for testing, prototyping, and understanding the
structure of complex schemas.

## Usage

```sh
xb sample-doc --element ROOT_ELEMENT [OPTIONS] SCHEMA...
```

### Required

| Option | Description |
|--------|-------------|
| `--element NAME` | Local name of the root element to instantiate |

### Optional

| Option | Description |
|--------|-------------|
| `--namespace URI` | Namespace URI of the target element |
| `--populate-optional` | Include optional elements and attributes |
| `--max-depth N` | Recursion depth limit (default: 20) |
| `--output FILE` | Write to a file instead of stdout |

## Examples

### Minimal Document

```sh
xb sample-doc --element purchaseOrder po.xsd
```

Produces a document with only required elements and attributes, using
placeholder values appropriate for each type:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<po:purchaseOrder xmlns:po="http://example.com/po">
  <po:shipTo>
    <po:name>string</po:name>
    <po:street>string</po:street>
    <po:city>string</po:city>
    <po:state>string</po:state>
    <po:zip>string</po:zip>
  </po:shipTo>
  <po:item>
    <po:productName>string</po:productName>
    <po:quantity>0</po:quantity>
  </po:item>
</po:purchaseOrder>
```

### With Optional Fields

```sh
xb sample-doc --element purchaseOrder --populate-optional po.xsd
```

Includes optional elements and attributes that would otherwise be omitted.

### Namespaced Element

When multiple schemas define elements with the same local name:

```sh
xb sample-doc --element order \
  --namespace "http://example.com/orders" \
  orders.xsd common.xsd
```

### Depth-Limited

For schemas with deeply recursive types:

```sh
xb sample-doc --element document --max-depth 5 schema.xsd
```

## Placeholder Values

The generated document uses type-appropriate placeholder values:

| XSD Type | Placeholder |
|----------|-------------|
| `xs:string` | `string` |
| `xs:int`, `xs:integer` | `0` |
| `xs:boolean` | `false` |
| `xs:date` | `2000-01-01` |
| `xs:dateTime` | `2000-01-01T00:00:00` |
| `xs:decimal` | `0.0` |
| Enumerations | First enumeration value |
