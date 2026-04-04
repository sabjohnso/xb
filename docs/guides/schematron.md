# Schematron

xb parses [Schematron](http://schematron.com/) schemas and applies them as
assertion overlays on XSD types. Schematron rules express constraints that XSD
alone cannot — cross-field validation, conditional requirements, and business
rules.

## Usage

Schematron schemas are used as overlays on top of an XSD schema:

### CLI

```sh
xb generate -o gen/ schema.xsd --schematron rules.sch
```

### What Gets Generated

Schematron assertions become runtime validation checks in the generated
deserialization code. Each `<sch:assert>` or `<sch:report>` generates a
validation call that evaluates the XPath test expression against the parsed
data.

## Schematron Structure

A Schematron schema consists of patterns, rules, and assertions:

```xml
<sch:schema xmlns:sch="http://purl.oclc.org/dml/schematron">
  <sch:ns prefix="ord" uri="http://example.com/orders"/>

  <sch:pattern name="Order validation">
    <sch:rule context="ord:order">
      <sch:assert test="ord:total > 0">
        Order total must be positive.
      </sch:assert>
      <sch:assert test="count(ord:item) > 0">
        Order must have at least one item.
      </sch:assert>
    </sch:rule>

    <sch:rule context="ord:item">
      <sch:report test="ord:quantity = 0">
        Item has zero quantity.
      </sch:report>
    </sch:rule>
  </sch:pattern>
</sch:schema>
```

### Key Elements

| Element | Description |
|---------|-------------|
| `<sch:ns>` | Namespace binding for use in XPath expressions |
| `<sch:pattern>` | A group of related rules |
| `<sch:rule>` | Applies to elements matching the `context` XPath |
| `<sch:assert>` | Fails if the `test` XPath evaluates to false |
| `<sch:report>` | Reports when the `test` XPath evaluates to true |
| `<sch:diagnostics>` | Referenced diagnostic messages for detailed errors |

### Assert vs Report

- **`<sch:assert>`** — the test must be true; if false, the message is an error
- **`<sch:report>`** — the test is a condition to flag; if true, the message is
  a warning or informational notice
