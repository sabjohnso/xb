#pragma once

#include <xb/soap_listener.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace xb::service {

  struct http_listener_options {
    std::string bind_address = "0.0.0.0";
    std::uint16_t port = 8080;
    std::string path = "/";
    int backlog = 16;

    bool
    operator==(const http_listener_options&) const = default;
  };

  class http_listener : public soap_listener {
  public:
    explicit http_listener(http_listener_options opts = {});
    ~http_listener() override;
    http_listener(http_listener&&) noexcept;
    http_listener&
    operator=(http_listener&&) noexcept;

    void
    serve(soap_handler handler) override;

    void
    stop() override;

    /// Returns the port the listener is bound to.  Useful when the
    /// configured port is 0 (OS-assigned).
    std::uint16_t
    listening_port() const;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb::service
