# Naming Styles

xb can transform XSD names to match your project's C++ naming conventions.
Three categories of names are independently configurable: types, fields, and
enum values.

## Available Styles

| Style | Input | Output |
|-------|-------|--------|
| `snake` (default) | `MyTypeName` | `my_type_name` |
| `pascal` | `my_type_name` | `MyTypeName` |
| `camel` | `my_type_name` | `myTypeName` |
| `upper-snake` | `MyTypeName` | `MY_TYPE_NAME` |
| `original` | `MyTypeName` | `MyTypeName` |

## Configuration

### CLI

```sh
xb generate \
  --type-style pascal \
  --field-style snake \
  --enum-style upper-snake \
  -o gen/ schema.xsd
```

### CMake

```cmake
xb_add_library(
  TARGET my_types
  SCHEMAS schema.xsd
  TYPE_STYLE pascal
  FIELD_STYLE snake
  ENUM_STYLE upper-snake)
```

## Examples

Given this XSD:

```xml
<xs:complexType name="ShippingAddress">
  <xs:sequence>
    <xs:element name="StreetAddress" type="xs:string"/>
    <xs:element name="PostalCode" type="xs:string"/>
  </xs:sequence>
</xs:complexType>

<xs:simpleType name="OrderStatus">
  <xs:restriction base="xs:string">
    <xs:enumeration value="InProgress"/>
    <xs:enumeration value="Shipped"/>
    <xs:enumeration value="Delivered"/>
  </xs:restriction>
</xs:simpleType>
```

### Default (`snake` everywhere)

```cpp
struct shipping_address {
    std::string street_address;
    std::string postal_code;
};

enum class order_status {
    in_progress, shipped, delivered
};
```

### Pascal types, snake fields, upper-snake enums

```cpp
struct ShippingAddress {
    std::string street_address;
    std::string postal_code;
};

enum class OrderStatus {
    IN_PROGRESS, SHIPPED, DELIVERED
};
```

### Original (preserve XSD names)

```cpp
struct ShippingAddress {
    std::string StreetAddress;
    std::string PostalCode;
};

enum class OrderStatus {
    InProgress, Shipped, Delivered
};
```
