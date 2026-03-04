#pragma once

#include <xb/http_transport.hpp>
#include <xb/wsdl_transport.hpp>

#include <cstddef>
#include <memory>

namespace xb::service {

  struct mtom_options {
    std::size_t optimization_threshold = 1024;

    bool
    operator==(const mtom_options&) const = default;
  };

  class mtom_transport : public transport {
  public:
    explicit mtom_transport(http_options http_opts = {},
                            mtom_options mtom_opts = {});
    ~mtom_transport() override;
    mtom_transport(mtom_transport&&) noexcept;
    mtom_transport&
    operator=(mtom_transport&&) noexcept;

    soap::envelope
    call(const std::string& endpoint, const std::string& soap_action,
         const soap::envelope& request) override;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb::service
