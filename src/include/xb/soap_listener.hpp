#pragma once

#include <xb/soap_model.hpp>

#include <functional>
#include <string>

namespace xb::service {

  /// Function signature for handling SOAP requests.
  /// Takes the SOAPAction and request envelope, returns a response envelope.
  using soap_handler = std::function<soap::envelope(
      const std::string& soap_action, const soap::envelope& request)>;

  /// Abstract listener interface. Implementations bind to a network
  /// transport and serve SOAP requests.
  class soap_listener {
  public:
    virtual ~soap_listener() = default;

    /// Serve requests using the given handler.  Blocks until stop() is
    /// called from another thread.
    virtual void
    serve(soap_handler handler) = 0;

    /// Signal the listener to stop accepting new connections and return
    /// from serve().
    virtual void
    stop() = 0;
  };

} // namespace xb::service
