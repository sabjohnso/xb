# XSD Type Map Reference

Complete mapping of XSD built-in types to their default C++ representations.

## String Types

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

## List Types

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:IDREFS` | `std::vector<std::string>` | `<vector>` `<string>` |
| `xs:NMTOKENS` | `std::vector<std::string>` | `<vector>` `<string>` |
| `xs:ENTITIES` | `std::vector<std::string>` | `<vector>` `<string>` |

## Boolean

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:boolean` | `bool` | -- |

## Floating Point

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:float` | `float` | -- |
| `xs:double` | `double` | -- |

## Arbitrary Precision

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:decimal` | `xb::decimal` | `<xb/decimal.hpp>` |
| `xs:integer` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:nonPositiveInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:negativeInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:nonNegativeInteger` | `xb::integer` | `<xb/integer.hpp>` |
| `xs:positiveInteger` | `xb::integer` | `<xb/integer.hpp>` |

## Bounded Integers

| XSD Type | C++ Type | Range | Header |
|----------|----------|-------|--------|
| `xs:long` | `int64_t` | -2^63 to 2^63-1 | `<cstdint>` |
| `xs:int` | `int32_t` | -2^31 to 2^31-1 | `<cstdint>` |
| `xs:short` | `int16_t` | -32768 to 32767 | `<cstdint>` |
| `xs:byte` | `int8_t` | -128 to 127 | `<cstdint>` |
| `xs:unsignedLong` | `uint64_t` | 0 to 2^64-1 | `<cstdint>` |
| `xs:unsignedInt` | `uint32_t` | 0 to 2^32-1 | `<cstdint>` |
| `xs:unsignedShort` | `uint16_t` | 0 to 65535 | `<cstdint>` |
| `xs:unsignedByte` | `uint8_t` | 0 to 255 | `<cstdint>` |

## Date and Time

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:dateTime` | `xb::date_time` | `<xb/date_time.hpp>` |
| `xs:date` | `xb::date` | `<xb/date.hpp>` |
| `xs:time` | `xb::time` | `<xb/time.hpp>` |
| `xs:duration` | `xb::duration` | `<xb/duration.hpp>` |

## Binary

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:hexBinary` | `std::vector<std::byte>` | `<vector>` `<cstddef>` |
| `xs:base64Binary` | `std::vector<std::byte>` | `<vector>` `<cstddef>` |

## Qualified Names

| XSD Type | C++ Type | Header |
|----------|----------|--------|
| `xs:QName` | `xb::qname` | `<xb/qname.hpp>` |

## Overriding Defaults

See [Type Mapping](../guides/type-mapping.md) for how to customize these
mappings with a typemap file.

## XSD Construct Mappings

Beyond built-in types, XSD structural constructs also have C++ mappings:

| XSD Construct | C++ Mapping |
|---------------|------------|
| `xs:choice` | `std::variant<alternatives...>` |
| `xs:union` | `std::variant<member_types...>` |
| Optional element (`minOccurs="0"`) | `std::optional<T>` |
| Repeating element (`maxOccurs > 1`) | `std::vector<T>` |
| `xs:list` | `std::vector<T>` |
| `xs:complexType` | `struct` |
| `xs:simpleType` with `xs:enumeration` | `enum class` |
| Substitution group | `std::variant<members...>` |
