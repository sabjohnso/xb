#pragma once

#include <xb/naming.hpp>
#include <xb/service_model.hpp>
#include <xb/type_map.hpp>
#include <xb/wsdl2_model.hpp>

namespace xb {

  class wsdl2_resolver {
  public:
    service::service_description
    resolve(const wsdl2::description& desc, const type_map& types,
            const codegen_options& options = {}) const;
  };

} // namespace xb
