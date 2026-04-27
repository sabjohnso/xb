#pragma once

#include <xb/soap_model.hpp>
#include <xb/wsa.hpp>

namespace xb::wsa {

  void
  add_addressing_headers(soap::envelope& env, const addressing_headers& h);

  /// Extract WS-Addressing headers from @p env.  Equivalent to
  /// @ref extract_addressing_headers(env, {}) — no endpoint
  /// validation.
  addressing_headers
  extract_addressing_headers(const soap::envelope& env);

  /// Extract WS-Addressing headers from @p env, validating
  /// @c ReplyTo / @c FaultTo / @c From against
  /// @ref endpoint_validation_options::address_allowlist.  Throws
  /// @c std::runtime_error if any address is rejected.
  addressing_headers
  extract_addressing_headers(const soap::envelope& env,
                             const endpoint_validation_options& opts);

} // namespace xb::wsa
