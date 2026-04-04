# XML Schema Code Generation

xb parses XML Schema 1.1 (XSD) definitions and generates C++ types with full
serialization and deserialization support.

## What Gets Generated

For each XSD construct, xb generates corresponding C++ types:

| XSD Construct | C++ Output |
|---------------|------------|
| `xs:complexType` with elements | `struct` with typed fields |
| `xs:simpleType` with enumeration | `enum class` with string conversion |
| `xs:simpleType` with restriction | Type alias or newtype |
| `xs:choice` | `std::variant<...>` |
| `xs:sequence` | Struct fields in order |
| `xs:all` | Struct fields (order-independent parsing) |
| `xs:element` (global) | Top-level `read()` / `write()` functions |
| `xs:attribute` | Struct field |
| Optional elements (`minOccurs="0"`) | `std::optional<T>` |
| Repeating elements (`maxOccurs="unbounded"`) | `std::vector<T>` |
| `xs:union` | `std::variant<...>` |
| `xs:list` | `std::vector<T>` |
| Substitution groups | `std::variant<...>` |

## Running Code Generation

### CLI

```sh
xb generate [OPTIONS] SCHEMA...
```

Common options:

```sh
# Header-only output
xb generate --header-only -o gen/ schema.xsd

# Split headers and sources (default)
xb generate -o gen/ schema.xsd

# One header per type
xb generate --file-per-type -o gen/ schema.xsd

# Custom namespace mapping
xb generate -n "http://example.com/ns=myns" -o gen/ schema.xsd

# Custom type mapping
xb generate -t my-typemap.xml -o gen/ schema.xsd

# Include doxygen comments from XSD annotations
xb generate --generate-docs -o gen/ schema.xsd
```

### CMake

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schema.xsd
  MODE HEADER_ONLY)
```

See [CMake Integration](../getting-started/cmake-integration.md) for full
details.

## Complex Types

### Sequences

```xml
<xs:complexType name="Address">
  <xs:sequence>
    <xs:element name="street" type="xs:string"/>
    <xs:element name="city"   type="xs:string"/>
    <xs:element name="zip"    type="xs:string"/>
  </xs:sequence>
</xs:complexType>
```

Generates:

```cpp
struct address {
    std::string street;
    std::string city;
    std::string zip;
};
```

### Choice Groups

```xml
<xs:complexType name="Payment">
  <xs:choice>
    <xs:element name="credit-card" type="CreditCard"/>
    <xs:element name="bank-transfer" type="BankTransfer"/>
    <xs:element name="cash" type="xs:boolean"/>
  </xs:choice>
</xs:complexType>
```

Generates:

```cpp
struct payment {
    std::variant<credit_card, bank_transfer, bool> choice;
};
```

Use `std::visit` for exhaustive handling:

```cpp
std::visit(overloaded{
    [](const credit_card& cc) { /* ... */ },
    [](const bank_transfer& bt) { /* ... */ },
    [](bool cash) { /* ... */ },
}, payment.choice);
```

### Optional and Repeating Elements

```xml
<xs:complexType name="Person">
  <xs:sequence>
    <xs:element name="name" type="xs:string"/>
    <xs:element name="email" type="xs:string" minOccurs="0"/>
    <xs:element name="phone" type="xs:string" minOccurs="0" maxOccurs="unbounded"/>
  </xs:sequence>
</xs:complexType>
```

Generates:

```cpp
struct person {
    std::string name;
    std::optional<std::string> email;
    std::vector<std::string> phone;
};
```

### Attributes

```xml
<xs:complexType name="Product">
  <xs:sequence>
    <xs:element name="description" type="xs:string"/>
  </xs:sequence>
  <xs:attribute name="id" type="xs:int" use="required"/>
  <xs:attribute name="discontinued" type="xs:boolean"/>
</xs:complexType>
```

Generates:

```cpp
struct product {
    int32_t id;                              // required attribute
    std::optional<bool> discontinued;        // optional attribute
    std::string description;                 // element
};
```

## Simple Types

### Enumerations

```xml
<xs:simpleType name="Color">
  <xs:restriction base="xs:string">
    <xs:enumeration value="red"/>
    <xs:enumeration value="green"/>
    <xs:enumeration value="blue"/>
  </xs:restriction>
</xs:simpleType>
```

Generates:

```cpp
enum class color { red, green, blue };

auto to_string(color v) -> std::string_view;
auto color_from_string(std::string_view s) -> color;
```

### Unions

```xml
<xs:simpleType name="StringOrInt">
  <xs:union memberTypes="xs:string xs:int"/>
</xs:simpleType>
```

Generates:

```cpp
using string_or_int = std::variant<std::string, int32_t>;
```

## Inheritance

### Extension

```xml
<xs:complexType name="Shape">
  <xs:sequence>
    <xs:element name="color" type="xs:string"/>
  </xs:sequence>
</xs:complexType>

<xs:complexType name="Circle">
  <xs:complexContent>
    <xs:extension base="Shape">
      <xs:sequence>
        <xs:element name="radius" type="xs:double"/>
      </xs:sequence>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
```

Generates a flattened struct (composition over inheritance):

```cpp
struct circle {
    std::string color;   // from Shape
    double radius;       // from Circle
};
```

## Multi-Schema Projects

When your XSD imports other schemas, pass all schema files:

```sh
xb generate -o gen/ main.xsd common.xsd types.xsd
```

Or use `xb fetch` to collect schemas and their transitive dependencies first:

```sh
xb fetch http://example.com/schema.xsd --output-dir schemas/
xb generate -o gen/ schemas/*.xsd
```

## Output Modes

| Mode | Flag | Description |
|------|------|-------------|
| Split | (default) | Separate `.hpp` and `.cpp` files |
| Header-only | `--header-only` | Single `.hpp` with inline implementations |
| File-per-type | `--file-per-type` | One header per type |

Use `--list-outputs` to see what files will be generated without writing them:

```sh
xb generate --list-outputs schema.xsd
```

## Complete Example

See `examples/xsd-addressbook/` for a working example that defines an
addressbook schema with sequences, optional elements, repeating elements,
attributes, and enumerations, then generates types, constructs values,
serializes to XML, and verifies a round-trip.
