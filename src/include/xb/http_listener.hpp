#pragma once

#include <xb/soap_listener.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace xb::service {

  struct http_listener_options {
    /// Interface to bind. Defaults to loopback so that an
    /// unconfigured listener is not reachable from the network.
    /// Set to ="0.0.0.0"= explicitly to listen on all interfaces.
    std::string bind_address = "127.0.0.1";

    std::uint16_t port = 8080;
    std::string path = "/";
    int backlog = 16;

    /// Maximum permitted HTTP request body length in bytes. Requests
    /// declaring a larger @c Content-Length are rejected with HTTP 413
    /// before any body data is read. Defaults to 16 MiB.
    std::size_t max_request_bytes = 16ULL * 1024ULL * 1024ULL;

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
