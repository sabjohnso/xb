# Quick Start

This guide walks you through generating C++ types from an XSD schema in under
five minutes.

## 1. Write an XSD Schema

Create a file `addressbook.xsd`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/addressbook"
           xmlns:ab="http://example.com/addressbook"
           elementFormDefault="qualified">

  <xs:complexType name="Address">
    <xs:sequence>
      <xs:element name="street" type="xs:string"/>
      <xs:element name="city"   type="xs:string"/>
      <xs:element name="zip"    type="xs:string"/>
    </xs:sequence>
  </xs:complexType>

  <xs:complexType name="Person">
    <xs:sequence>
      <xs:element name="name"    type="xs:string"/>
      <xs:element name="email"   type="xs:string" minOccurs="0"/>
      <xs:element name="address" type="ab:Address"/>
    </xs:sequence>
  </xs:complexType>

  <xs:element name="addressbook">
    <xs:complexType>
      <xs:sequence>
        <xs:element name="person" type="ab:Person"
                    maxOccurs="unbounded"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
```

## 2. Generate C++ Code

### Using the CLI

```sh
xb generate --header-only -o gen/ addressbook.xsd
```

This produces `gen/addressbook.hpp` containing C++ structs for `Address`,
`Person`, and the root `addressbook` element, plus `read()` and `write()`
functions for XML serialization.

### Using CMake

Add to your `CMakeLists.txt`:

```cmake
find_package(xb REQUIRED)

xb_add_library(
  TARGET addressbook_types
  SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/addressbook.xsd
  MODE HEADER_ONLY)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE addressbook_types)
```

## 3. Use the Generated Types

```cpp
#include <addressbook.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <fstream>
#include <iostream>

int main() {
    // Read an XML document
    std::ifstream input("contacts.xml");
    auto reader = xb::expat_reader(input);
    auto book = addressbook::addressbook::read(reader);

    // Access typed fields
    for (const auto& person : book.person) {
        std::cout << person.name << "\n";
        std::cout << "  " << person.address.city << "\n";
        if (person.email) {
            std::cout << "  " << *person.email << "\n";
        }
    }

    // Write back to XML
    std::ofstream output("output.xml");
    auto writer = xb::ostream_writer(output);
    addressbook::addressbook::write(writer, book);
}
```

## 4. Generate a Sample Document

Don't have an XML file yet? Generate one from the schema:

```sh
xb sample-doc --element addressbook addressbook.xsd
```

This produces a minimal conforming XML instance:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ab:addressbook xmlns:ab="http://example.com/addressbook">
  <ab:person>
    <ab:name>string</ab:name>
    <ab:address>
      <ab:street>string</ab:street>
      <ab:city>string</ab:city>
      <ab:zip>string</ab:zip>
    </ab:address>
  </ab:person>
</ab:addressbook>
```

## What's Next?

- [CMake Integration](cmake-integration.md) — `xb_add_library` and
  `xb_generate_cpp` in detail
- [Type Mapping](../guides/type-mapping.md) — customize how XSD types map to
  C++ types
- [Naming Styles](../guides/naming-styles.md) — control type, field, and enum
  naming conventions
- [Binary Encoding](../binary-encoding/overview.md) — generate zero-copy
  binary types from BES
