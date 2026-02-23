#pragma once

#include <xb/xml_reader.hpp>
#include <xb/xml_value.hpp>
#include <xb/xml_writer.hpp>

#include <string>

namespace xb {

  // Read text content of current element (advances past end_element)
  std::string
  read_text(xml_reader& reader);

  // Read a simple-typed element: read_text + parse<T>
  template <typename T>
  T
  read_simple(xml_reader& reader) {
    return parse<T>(read_text(reader));
  }

  // Write a simple-typed element: start_element + characters + end_element
  template <typename T>
  void
  write_simple(xml_writer& writer, const qname& name, const T& value) {
    writer.start_element(name);
    writer.characters(format(value));
    writer.end_element();
  }

  // Skip current element and all children
  void
  skip_element(xml_reader& reader);

} // namespace xb
