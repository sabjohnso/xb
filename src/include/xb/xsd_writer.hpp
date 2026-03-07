#pragma once

#include <xb/schema.hpp>
#include <xb/xml_writer.hpp>

#include <string>

namespace xb {

  void
  xsd_write(const schema& s, xml_writer& writer);

  std::string
  xsd_write_string(const schema& s);

  std::string
  xsd_write_string(const schema& s, int indent);

} // namespace xb
