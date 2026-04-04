# Type Mapping

xb maps XSD built-in types to C++ types. The defaults are standards-conformant,
but every mapping can be overridden.

## Default Mappings

### String Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:string` | `std::string` | `<string>` |
| `xs:normalizedString` | `std::string` | `<string>` |
| `xs:token` | `std::string` | `<string>` |
| `xs:anyURI` | `std::string` | `<string>` |
| `xs:ID` | `std::string` | `<string>` |
| `xs:IDREF` | `std::string` | `<string>` |
| `xs:Name` | `std::string` | `<string>` |
| `xs:NCName` | `std::string` | `<string>` |
| `xs:NMTOKEN` | `std::string` | `<string>` |
| `xs:language` | `std::string` | `<string>` |
| `xs:ENTITY` | `std::string` | `<string>` |

### List Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:IDREFS` | `std::vector<std::string>` | `<vector>` `<string>` |
| `xs:NMTOKENS` | `std::vector<std::string>` | `<vector>` `<string>` |
| `xs:ENTITIES` | `std::vector<std::string>` | `<vector>` `<string>` |

### Numeric Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:boolean` | `bool` | -- |
| `xs:float` | `float` | -- |
| `xs:double` | `double` | -- |
| `xs:decimal` | `xb::decimal` | `<xb/decimal.hpp>` |
| `xs:integer` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:nonPositiveInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:negativeInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:nonNegativeInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:positiveInteger` | `xb::integer` | `<xb/integer.hpp>` |

### Bounded Integer Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:long` | `int64_t` | `<cstdint>` |
| `xs:int` | `int32_t` | `<cstdint>` |
| `xs:short` | `int16_t` | `<cstdint>` |
| `xs:byte` | `int8_t` | `<cstdint>` |
| `xs:unsignedLong` | `uint64_t` | `<cstdint>` |
| `xs:unsignedInt` | `uint32_t` | `<cstdint>` |
| `xs:unsignedShort` | `uint16_t` | `<cstdint>` |
| `xs:unsignedByte` | `uint8_t` | `<cstdint>` |

### Date/Time Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:dateTime` | `xb::date_time` | `<xb/date_time.hpp>` |
| `xs:date` | `xb::date` | `<xb/date.hpp>` |
| `xs:time` | `xb::time` | `<xb/time.hpp>` |
| `xs:duration` | `xb::duration` | `<xb/duration.hpp>` |

### Binary Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:hexBinary` | `std::vector<std::byte>` | `<vector>` `<cstddef>` |
| `xs:base64Binary` | `std::vector<std::byte>` | `<vector>` `<cstddef>` |

### Other Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:QName` | `xb::qname` | `<xb/qname.hpp>` |

## Custom Type Mappings

### Typemap File Format

Override mappings with an XML typemap file (validated by
`schema/xb-typemap.xsd`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<typemap xmlns="http://xb.dev/typemap">
  <mapping xsd-type="integer"
           cpp-type="int"
           cpp-header=""/>
  <mapping xsd-type="decimal"
           cpp-type="double"
           cpp-header=""/>
  <mapping xsd-type="positiveInteger"
           cpp-type="unsigned"
           cpp-header=""/>
</typemap>
```

Each `<mapping>` has three required attributes:

| Attribute | Description |
|-----------|-------------|
| `xsd-type` | XSD built-in type name (without `xs:` prefix) |
| `cpp-type` | Fully qualified C++ type name |
| `cpp-header` | Header to include (empty string for built-in types) |

### Using a Typemap

**CLI:**

```sh
xb generate -t my-typemap.xml -o gen/ schema.xsd
```

**CMake:**

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS schema.xsd
  TYPE_MAP ${CMAKE_CURRENT_SOURCE_DIR}/my-typemap.xml)
```

### When to Customize

Common reasons to override the defaults:

- **Performance**: Map `xs:integer` to `int` or `int64_t` when you know the
  values fit, avoiding arbitrary-precision overhead
- **Interop**: Map `xs:decimal` to `double` when exact decimal arithmetic
  isn't needed
- **Domain types**: Map `xs:dateTime` to your project's own date/time type

!!! note
    Custom type mappings only apply to XSD built-in types. User-defined
    simple types with restrictions (e.g., an integer restricted to 0–100)
    derive their C++ type from their base type's mapping.

## xb Built-in Types

### `xb::decimal`

Arbitrary-precision decimal arithmetic. Faithfully represents all `xs:decimal`
values without floating-point rounding.

```cpp
#include <xb/decimal.hpp>

xb::decimal price("123.45");
xb::decimal tax = price * xb::decimal("0.08");
std::cout << tax.to_string();  // "9.876"
```

### `xb::integer`

Arbitrary-precision integer arithmetic. Maps unbounded XSD integer types
(`xs:integer`, `xs:positiveInteger`, etc.) without overflow.

```cpp
#include <xb/integer.hpp>

xb::integer big("99999999999999999999");
std::cout << big.to_string();
```

### Date/Time Types

`xb::date`, `xb::time`, `xb::date_time`, and `xb::duration` implement the
XSD date/time types with timezone support:

```cpp
#include <xb/date.hpp>
#include <xb/duration.hpp>

xb::date d(2024, 3, 15);          // 2024-03-15
d.year();   // 2024
d.month();  // 3
d.day();    // 15

xb::duration dur(false, 1, 2, 3, 4, 5, 0);  // P1Y2M3DT4H5M
dur.is_negative();  // false
```

### `xb::qname`

XML Qualified Name with namespace URI and local name:

```cpp
#include <xb/qname.hpp>

xb::qname q("http://example.com/ns", "element");
q.namespace_uri();  // "http://example.com/ns"
q.local_name();     // "element"
```
