#pragma once

#include <xb/rng_pattern.hpp>
#include <xb/xml_writer.hpp>

#include <string>

namespace xb {

  void
  rng_write(const rng::pattern& p, xml_writer& writer);

  std::string
  rng_write_string(const rng::pattern& p);

  std::string
  rng_write_string(const rng::pattern& p, int indent);

} // namespace xb
