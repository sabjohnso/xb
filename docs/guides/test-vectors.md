# Test Vectors

xb can automatically generate test vectors from XML schemas. Test vectors
exercise boundary values, choice alternatives, optional fields, and
repeating elements — providing systematic coverage of the data model
without writing tests by hand.

## Usage

```sh
xb test-vectors --element NAME [OPTIONS] SCHEMA...
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--element NAME` | (required) | Target element local name |
| `--namespace URI` | -- | Target element namespace URI |
| `--format FORMAT` | `xml` | Output format: `xml` or `catch2` |
| `--output-dir DIR` | -- | Write one file per vector to this directory |
| `-o`, `--output FILE` | stdout | Write all vectors to a single file |
| `--report` | -- | Print a coverage report to stderr |

## XML Output

By default, test vectors are written as XML documents:

```sh
xb test-vectors --element purchaseOrder po.xsd
```

Each vector is preceded by a comment identifying it:

```xml
<!-- vector: baseline -->
<purchaseOrder xmlns="http://example.com/po">...</purchaseOrder>
<!-- vector: optional-present:shipDate -->
<purchaseOrder xmlns="http://example.com/po">...</purchaseOrder>
<!-- vector: optional-absent:shipDate -->
<purchaseOrder xmlns="http://example.com/po">...</purchaseOrder>
```

### Writing to a Directory

Use `--output-dir` to write one file per vector with numbered filenames:

```sh
xb test-vectors --output-dir ./vectors --element purchaseOrder po.xsd
```

Produces:

```
vectors/
  01-baseline.xml
  02-optional-present_shipDate.xml
  03-optional-absent_shipDate.xml
  ...
```

## Catch2 Test Source

Generate a C++ test file with round-trip test cases:

```sh
xb test-vectors --format catch2 --element purchaseOrder po.xsd > po_tests.cpp
```

Each test case constructs a value, serializes it to XML, deserializes it
back, and checks equality:

```cpp
TEST_CASE("purchaseOrder: baseline", "[auto][purchaseOrder]") {
  po::purchase_order original;
  // ... field assignments ...

  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://example.com/po", "purchaseOrder"});
    writer.namespace_declaration("", "http://example.com/po");
    po::write_purchase_order(original, writer);
    writer.end_element();
  }

  xb::expat_reader reader(os.str());
  reader.read();
  auto parsed = po::read_purchase_order(reader);

  REQUIRE(original == parsed);
}
```

## Coverage Report

Add `--report` to print a coverage summary to stderr:

```sh
xb test-vectors --element purchaseOrder --report po.xsd
```

```
Test Vector Coverage Report
===========================
Types:             5 /    8  (62.5%)
CHOICE alts:       3 /    3  (100.0%)
Enum values:       5 /    5  (100.0%)
Boundary vals:    12 /   14  (85.7%)
Optional combos:   4 /    6  (66.7%)
```

The report shows how many schema constructs are exercised by the generated
vectors.

## Coverage Levels

Control how test vectors are combined with `--coverage`:

### `boundary` (Default)

Each field is varied independently against a baseline — one field changes
at a time while others hold their first boundary value. Produces
O(fields x values) vectors.

```sh
xb test-vectors --coverage boundary --element measurement schema.xsd
```

### `pairwise`

Every pair of (field_i = value_a, field_j = value_b) appears in at least
one vector. Uses a greedy covering algorithm. Catches interaction bugs
between fields with dramatically fewer vectors than exhaustive.

```sh
xb test-vectors --coverage pairwise --element measurement schema.xsd
```

### `exhaustive`

Full cross-product of all required scalar fields and their boundary values.
Produces every possible combination. Use only for types with few fields
or few values per field.

```sh
xb test-vectors --coverage exhaustive --element measurement schema.xsd
```

### Fallback Behavior

- If `pairwise` is requested but the type has fewer than 2 required scalar
  fields, boundary coverage is used instead.
- If `exhaustive` or `pairwise` is requested but the type has no required
  scalar fields (all are optional, complex, or repeating), boundary
  coverage is used instead.

### Limiting Output

Use `--max-vectors` to cap the total number of vectors:

```sh
xb test-vectors --coverage exhaustive --max-vectors 50 --element order schema.xsd
```

## What Gets Generated

The test vector generator walks the schema and produces vectors covering:

| Schema Construct | Vectors Generated |
|-----------------|-------------------|
| Required elements | Baseline vector with all required fields |
| Optional elements | One vector with field present, one with field absent |
| Choice groups | One vector per alternative |
| Repeating elements | Vectors at minOccurs, maxOccurs, and boundary counts |
| Enumerations | One vector per enumeration value |
| Numeric types | Boundary values (min, min+1, max-1, max, 0, 1) |
| String types | Boundary lengths from length/minLength/maxLength facets |
| Boolean | Both true and false |
| Date/time | Epoch, leap year, timezone variants |

## CMake Integration

Use `xb_generate_test_vectors()` to generate and compile round-trip tests
at build time:

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  MODE HEADER_ONLY)

xb_generate_test_vectors(
  TARGET my_auto_tests
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  ELEMENTS PurchaseOrder LineItem
  NAMESPACE "http://example.com/po"
  LINK my_types)
```

This generates one `.cpp` file per element containing Catch2 test cases,
compiles them into a test executable, and registers it with CTest. The
generated tests link against Catch2, the xb runtime, and any targets
listed in `LINK`.

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `TARGET` | Yes | Name of the test executable to create |
| `SCHEMAS` | Yes | Schema files to process |
| `ELEMENTS` | Yes | Root element names to generate tests for |
| `NAMESPACE` | No | Target namespace URI (auto-detected if omitted) |
| `COVERAGE` | No | Coverage level: `boundary`, `pairwise`, `exhaustive` |
| `MAX_VECTORS` | No | Maximum number of vectors per element |
| `LINK` | No | Additional targets to link (e.g., generated types library) |

## Input Formats

Test vectors work with all schema formats xb supports:

- XSD (`.xsd`)
- RELAX NG (`.rng`)
- RELAX NG Compact (`.rnc`)
- DTD (`.dtd`)
