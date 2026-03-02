#pragma once

#include <xb/soap_model.hpp>
#include <xb/xml_reader.hpp>
#include <xb/xml_writer.hpp>

namespace xb::soap {

  // Parse a SOAP fault from a Body child element
  fault
  read_fault(xml_reader& reader, soap_version version);

  // Write a SOAP fault as a Body child element
  void
  write_fault(xml_writer& writer, const fault& f, soap_version version);

  // Check if a body child is a Fault element
  bool
  is_fault(const any_element& body_child, soap_version version);

} // namespace xb::soap
