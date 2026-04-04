# Schema Conversion

xb converts between XSD, RELAX NG (XML and Compact), and DTD formats using
the `convert` subcommand.

## Conversion Matrix

| Input | XSD | RNG | RNC | DTD |
|-------|-----|-----|-----|-----|
| XSD   | --  | Yes | Yes | Yes |
| RNG   | Yes | --  | Yes | Yes |
| RNC   | Yes | Yes | --  | Yes |
| DTD   | Yes | Yes | Yes | --  |

## Usage

```sh
xb convert [OPTIONS] FILE
```

The input format is auto-detected from the file extension (`.xsd`, `.rng`,
`.rnc`, `.dtd`).

### Examples

```sh
# XSD to RELAX NG
xb convert -f rng schema.xsd

# XSD to RELAX NG Compact
xb convert -f rnc schema.xsd

# RELAX NG to XSD
xb convert -f xsd schema.rng

# RNC to RNG (Compact to XML syntax)
xb convert -f rng schema.rnc

# DTD to XSD
xb convert -f xsd schema.dtd

# Control indentation
xb convert -f rnc --indent 4 schema.xsd
```

### Options

| Option | Description |
|--------|-------------|
| `-f`, `--output-format` | Output format: `xsd`, `rng`, `rnc`, or `dtd` |
| `--indent` | Indentation width for pretty-printing (RNC defaults to 2) |

If `--output-format` is omitted, xb picks a sensible default:

- XSD or DTD input defaults to `rng`
- RNG or RNC input defaults to the other RNG format

### Output

Output is written to stdout. Redirect to a file:

```sh
xb convert -f xsd schema.rng > schema.xsd
```

## Conversion Notes

### XSD to RELAX NG

- Complex types map to `<define>` / `<ref>` patterns
- `xs:sequence` becomes `<group>`, `xs:choice` becomes `<choice>`
- `xs:all` becomes `<interleave>`
- Facet restrictions are preserved where RELAX NG supports them

### XSD to DTD

- Namespaces are dropped (DTD has no namespace support)
- Numeric and date types become `CDATA`
- Complex inheritance is flattened

### DTD to XSD

- `CDATA` attributes become `xs:string`
- Enumerated attributes become `xs:simpleType` with `xs:enumeration`
- `ID` / `IDREF` / `NMTOKEN` attributes keep their XSD counterparts

### RELAX NG to XSD

- Named patterns (`<define>`) become named `xs:complexType` definitions
- `<interleave>` becomes `xs:all` (with XSD restrictions on occurrence)
- Data types from the XSD datatype library are preserved directly
