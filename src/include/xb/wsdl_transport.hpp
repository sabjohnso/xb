#pragma once

#include <xb/soap_model.hpp>

#include <stdexcept>
#include <string>

namespace xb::service {

  struct transport_error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  class transport {
  public:
    virtual ~transport() = default;

    virtual soap::envelope
    call(const std::string& endpoint, const std::string& soap_action,
         const soap::envelope& request) = 0;
  };

} // namespace xb::service
