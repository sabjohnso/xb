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

## Input Formats

Test vectors work with all schema formats xb supports:

- XSD (`.xsd`)
- RELAX NG (`.rng`)
- RELAX NG Compact (`.rnc`)
- DTD (`.dtd`)
