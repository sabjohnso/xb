# RELAX NG

xb reads RELAX NG schemas in both XML syntax (`.rng`) and Compact Syntax
(`.rnc`), translates them to an internal XSD model, and generates C++ types
using the same code generation pipeline as XSD.

## Supported Formats

| Format | Extension | Parser |
|--------|-----------|--------|
| RELAX NG XML | `.rng` | `rng_xml_parser` |
| RELAX NG Compact | `.rnc` | `rng_compact_parser` |

## Usage

### CLI

```sh
# Generate from RELAX NG XML
xb generate --header-only -o gen/ schema.rng

# Generate from RELAX NG Compact
xb generate --header-only -o gen/ schema.rnc
```

The input format is auto-detected from the file extension.

### CMake

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.rng
  MODE HEADER_ONLY)
```

## How Translation Works

xb translates RELAX NG patterns to equivalent XSD constructs:

| RELAX NG Pattern | XSD Equivalent |
|------------------|---------------|
| `<element>` | `xs:element` |
| `<attribute>` | `xs:attribute` |
| `<group>` | `xs:sequence` |
| `<interleave>` | `xs:all` |
| `<choice>` | `xs:choice` |
| `<optional>` | `minOccurs="0"` |
| `<zeroOrMore>` | `minOccurs="0" maxOccurs="unbounded"` |
| `<oneOrMore>` | `minOccurs="1" maxOccurs="unbounded"` |
| `<text>` | `xs:string` mixed content |
| `<data type="...">` | XSD built-in type |
| `<value>` | `xs:enumeration` facet |
| `<list>` | `xs:list` |
| `<define>` / `<ref>` | Named type definitions |

## Example

A RELAX NG Compact schema:

```rnc
default namespace = "http://example.com/addressbook"

start = addressbook

addressbook = element addressbook {
  person+
}

person = element person {
  element name { text },
  element email { text }?
}
```

Generates the same C++ types as the equivalent XSD:

```cpp
struct person {
    std::string name;
    std::optional<std::string> email;
};

struct addressbook {
    std::vector<person> person;
};
```

## Converting Between Formats

Use `xb convert` to convert RELAX NG to/from other schema formats:

```sh
# RNG to XSD
xb convert -f xsd schema.rng

# RNC to RNG
xb convert -f rng schema.rnc

# XSD to RNC
xb convert -f rnc schema.xsd
```

See [Schema Conversion](schema-conversion.md) for details.

## Limitations

Some RELAX NG features don't have direct XSD equivalents. The translator
handles these with best-effort approximations:

- **`<interleave>` with repeating children** — XSD `xs:all` has restrictions
  on `maxOccurs` that RELAX NG's `<interleave>` does not
- **`<except>` on name classes** — translated where possible; complex
  exclusions may be simplified
- **Co-occurrence constraints** — RELAX NG can express constraints that XSD
  cannot; these are noted but not enforced in the generated code
