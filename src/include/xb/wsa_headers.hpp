#pragma once

#include <xb/soap_model.hpp>
#include <xb/wsa.hpp>

namespace xb::wsa {

  void
  add_addressing_headers(soap::envelope& env, const addressing_headers& h);

  addressing_headers
  extract_addressing_headers(const soap::envelope& env);

} // namespace xb::wsa
