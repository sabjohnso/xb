#pragma once

#include <xb/naming.hpp>
#include <xb/service_model.hpp>
#include <xb/type_map.hpp>
#include <xb/wsdl_model.hpp>

namespace xb {

  class wsdl_resolver {
  public:
    service::service_description
    resolve(const wsdl::document& doc, const type_map& types,
            const codegen_options& options = {}) const;
  };

} // namespace xb
