#pragma once

#include <xb/soap_model.hpp>
#include <xb/xml_reader.hpp>
#include <xb/xml_writer.hpp>

namespace xb::soap {

  // Parse a SOAP envelope from XML (reader positioned at Envelope start)
  envelope
  read_envelope(xml_reader& reader);

  // Write a SOAP envelope to XML
  void
  write_envelope(xml_writer& writer, const envelope& env);

} // namespace xb::soap
