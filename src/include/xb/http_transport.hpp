#pragma once

#include <xb/wsdl_transport.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace xb::service {

  struct http_options {
    std::chrono::milliseconds connect_timeout{30000};
    std::chrono::milliseconds request_timeout{60000};
    bool follow_redirects = true;
    bool verify_peer = true;
    std::string ca_bundle;
    std::string client_cert;
    std::string client_key;

    /// Maximum number of HTTP redirects to follow before aborting.
    /// Bounds redirect-loop attacks. Ignored when @ref follow_redirects
    /// is @c false.
    long max_redirects = 5;

    /// Maximum response body length, in bytes.  Larger responses are
    /// aborted by the write callback. Defaults to 64 MiB.
    std::size_t max_response_bytes = 64ULL * 1024ULL * 1024ULL;

    /// When @c true (the default), the transport refuses to connect to
    /// addresses that resolve into the loopback, RFC 1918 private,
    /// link-local, or cloud-metadata ranges.  Re-checked after each
    /// redirect so a malicious 302 cannot escape into a private
    /// destination.
    ///
    /// Test harnesses or service-mesh sidecars that legitimately need
    /// to call loopback set this to @c false.
    bool block_private_destinations = true;

    bool
    operator==(const http_options&) const = default;
  };

  struct http_response {
    int status_code = 0;
    std::string content_type;
    std::string body;

    bool
    operator==(const http_response&) const = default;
  };

  class http_transport : public transport {
  public:
    explicit http_transport(http_options opts = {});
    ~http_transport() override;
    http_transport(http_transport&&) noexcept;
    http_transport&
    operator=(http_transport&&) noexcept;

    soap::envelope
    call(const std::string& endpoint, const std::string& soap_action,
         const soap::envelope& request) override;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb::service
