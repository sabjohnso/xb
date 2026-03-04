#pragma once

#include <xb/soap_model.hpp>
#include <xb/wss.hpp>

namespace xb::wss {

  void
  add_security_header(soap::envelope& env, const security_header& h);

  security_header
  extract_security_header(const soap::envelope& env);

} // namespace xb::wss
