#include <addressbook.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>

#include <iostream>
#include <sstream>

// Build an addressbook with two people, serialize it to XML,
// then parse the XML back and print the result.

int
main() {
  using namespace addressbook;

  // -- Construct values --

  phone p1;
  p1.value = "+1-555-0100";
  p1.kind = phone_kind::work;

  phone p2;
  p2.value = "+1-555-0101";
  p2.kind = phone_kind::mobile;

  address addr;
  addr.street = "123 Elm St";
  addr.city = "Springfield";
  addr.state = "IL";
  addr.zip = "62704";

  person alice;
  alice.id = 1;
  alice.name = "Alice Smith";
  alice.email = "alice@example.com";
  alice.phone_ = {p1, p2};
  alice.address_ = addr;

  person bob;
  bob.id = 2;
  bob.name = "Bob Jones";
  // bob has no email, no phone, no address

  addressbook_type book;
  book.person_ = {alice, bob};

  // -- Serialize to XML --

  std::ostringstream xml_out;
  {
    xb::ostream_writer writer(xml_out);
    writer.start_element(
        xb::qname{"http://example.com/addressbook", "addressbook"});
    writer.namespace_declaration("", "http://example.com/addressbook");
    write_addressbook_type(book, writer);
    writer.end_element();
  }

  std::string xml = xml_out.str();
  std::cout << "=== Serialized XML ===\n" << xml << "\n\n";

  // -- Deserialize from XML --

  xb::expat_reader reader(xml);
  reader.read();
  auto parsed = read_addressbook_type(reader);

  // -- Print parsed values --

  std::cout << "=== Parsed addressbook ===\n";
  for (const auto& p : parsed.person_) {
    std::cout << "Person #" << p.id << ": " << p.name << "\n";

    if (p.email) std::cout << "  Email: " << *p.email << "\n";

    for (const auto& ph : p.phone_)
      std::cout << "  Phone (" << to_string(ph.kind) << "): " << ph.value
                << "\n";

    if (p.address_) {
      const auto& a = *p.address_;
      std::cout << "  Address: " << a.street << ", " << a.city;
      if (a.state) std::cout << ", " << *a.state;
      std::cout << " " << a.zip << "\n";
    }
  }

  // -- Verify round-trip --

  if (parsed == book) {
    std::cout << "\nRound-trip: OK\n";
    return 0;
  }
  std::cerr << "\nRound-trip: MISMATCH\n";
  return 1;
}
