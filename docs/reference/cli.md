# CLI Reference

```
xb <command> [options]
```

## Commands

### `generate`

Generate C++ types and serialization code from XSD.

```
xb generate [OPTIONS] SCHEMA...
```

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--output-dir` | `-o` | `.` | Output directory for generated files |
| `--type-map` | `-t` | -- | Type map override file (xb-typemap.xml) |
| `--namespace-map` | `-n` | -- | Namespace mapping (URI=NS). Repeatable |
| `--header-only` | -- | -- | Generate single .hpp file |
| `--file-per-type` | -- | -- | Generate one header per type |
| `--encapsulation` | -- | `raw-struct` | `raw-struct` or `wrapped` |
| `--namespace-style` | -- | `short` | `short` (last URI segment) or `full` (mangled URI) |
| `--header-suffix` | -- | `.hpp` | Suffix for generated headers |
| `--source-suffix` | -- | `.cpp` | Suffix for generated sources |
| `--type-style` | -- | `snake` | Naming style for types |
| `--field-style` | -- | `snake` | Naming style for fields |
| `--enum-style` | -- | `snake` | Naming style for enum values |
| `--generate-docs` | -- | -- | Emit `///` doxygen comments from XSD annotations |
| `--separate-fwd-header` | -- | -- | Emit separate `_fwd` header files |
| `--no-format` | -- | -- | Skip clang-format post-processing |
| `--encoding` | -- | -- | BES file for binary type generation |
| `--validation-level` | -- | `full` | Binary validation: `full`, `structural`, `discriminant` |
| `--binary-only` | -- | -- | Generate only binary types (suppress XML types) |
| `--list-outputs` | -- | -- | Print expected output filenames and exit |

**Positional:** `SCHEMA...` — XSD schema files to process.

**Naming styles:** `snake`, `pascal`, `camel`, `upper-snake`, `original`.

#### Examples

```sh
# Basic header-only generation
xb generate --header-only -o gen/ schema.xsd

# Custom namespace mapping
xb generate -n "http://example.com/ns=myns" -o gen/ schema.xsd

# BES binary types only
xb generate --encoding protocol.bes.xml -o gen/ --binary-only

# XSD + BES together
xb generate --encoding protocol.bes.xml -o gen/ schema.xsd

# Pascal case types, snake case fields, screaming snake enums
xb generate --type-style pascal --field-style snake --enum-style upper-snake \
    -o gen/ schema.xsd
```

---

### `sample-doc`

Generate a sample XML document from a schema.

```
xb sample-doc --element NAME [OPTIONS] SCHEMA...
```

| Option | Default | Description |
|--------|---------|-------------|
| `--element` | (required) | Target element local name |
| `--namespace` | -- | Target element namespace URI |
| `--populate-optional` | -- | Include optional elements and attributes |
| `--max-depth` | `20` | Recursion depth limit |
| `--output` | stdout | Output file |

#### Examples

```sh
xb sample-doc --element purchaseOrder po.xsd
xb sample-doc --element order --namespace "http://example.com/orders" \
    --populate-optional schema.xsd
```

---

### `fetch`

Fetch XSD schemas and their transitive dependencies.

```
xb fetch [OPTIONS] URL_OR_PATH
```

| Option | Default | Description |
|--------|---------|-------------|
| `--output-dir` | `.` | Output directory for fetched schemas |
| `--manifest` | -- | Write JSON manifest to file |
| `--fail-fast` | -- | Stop on first fetch error (default: best-effort) |

#### Examples

```sh
xb fetch http://example.com/schema.xsd --output-dir schemas/
xb fetch --manifest deps.json --output-dir schemas/ \
    http://example.com/schema.xsd
```

---

### `generate-xsd`

Generate XSD from a Binary Encoding Specification (BES) file.

```
xb generate-xsd --encoding FILE [OPTIONS]
```

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--encoding` | `-e` | (required) | BES file |
| `--output` | `-o` | stdout | Output XSD file |

#### Examples

```sh
xb generate-xsd -e protocol.bes.xml -o protocol.xsd
```

---

### `convert`

Convert between schema formats (XSD/DTD/RNG/RNC).

```
xb convert [OPTIONS] FILE
```

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--output-format` | `-f` | auto | Output format: `xsd`, `rng`, `rnc`, `dtd` |
| `--indent` | -- | format-dependent | Indentation width |

Input format is auto-detected from file extension.

#### Examples

```sh
xb convert -f xsd schema.rng > schema.xsd
xb convert -f rnc schema.xsd
xb convert -f dtd schema.rng
```

---

### `railroad`

Render schema content models as railroad diagrams in SVG.

```
xb railroad [OPTIONS] SCHEMA...
```

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--type` | -- | -- | Target complex type local name |
| `--element` | -- | -- | Target root element (discovers reachable types) |
| `--namespace` | -- | -- | Namespace URI |
| `--output` | `-o` | stdout | Output SVG file |
| `--theme` | -- | `light` | Color scheme: `light` or `dark` |
| `--transparent` | -- | -- | Transparent background (no solid fill) |

#### Examples

```sh
xb railroad --type Address -o address.svg schema.xsd
xb railroad --element purchaseOrder -o po.svg schema.xsd
xb railroad --element root schema.rnc > diagram.svg
```

---

### `generate-wsdl`

Generate C++ client and server code from a WSDL file.

```
xb generate-wsdl [OPTIONS] WSDL
```

Parses a WSDL 1.1 or 2.0 file and produces:

- XSD type headers (from inline `<types>` schemas)
- Client stub headers (`*_client.hpp`) with typed methods per operation
- Server skeleton headers (`*_server.hpp`) with interface + dispatcher classes

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--output-dir` | `-o` | `.` | Output directory |
| `--type-map` | `-t` | -- | Type map override file |
| `--namespace-map` | `-n` | -- | Namespace mapping (URI=NS). Repeatable |
| `--wsdl-mode` | -- | `both` | What to generate: `client`, `server`, or `both` |
| `--header-only` | -- | -- | Generate header-only output |
| `--type-style` | -- | `snake` | Naming style for types |
| `--field-style` | -- | `snake` | Naming style for fields |
| `--enum-style` | -- | `snake` | Naming style for enum values |
| `--no-format` | -- | -- | Skip clang-format post-processing |

**Positional:** `WSDL` — WSDL file to process (`.wsdl`).

WSDL version is auto-detected from the root element namespace.

#### Examples

```sh
# Generate client + server + types from WSDL
xb generate-wsdl -o gen/ service.wsdl

# Client only, with namespace mapping
xb generate-wsdl --wsdl-mode client \
    -n "http://example.com/ns=myns" -o gen/ service.wsdl

# Server only
xb generate-wsdl --wsdl-mode server -o gen/ service.wsdl
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Usage error (invalid arguments) |
| `2` | I/O error (cannot read/write file) |
| `3` | Schema parse error |
| `4` | Code generation error |
