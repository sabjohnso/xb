#pragma once

#include <xb/wsdl_transport.hpp>

#include <chrono>
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
