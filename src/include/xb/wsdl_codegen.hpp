#pragma once

#include <xb/cpp_code.hpp>
#include <xb/service_model.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace xb {

  struct wsdl_codegen_options {
    std::unordered_map<std::string, std::string> namespace_map;
  };

  class wsdl_codegen {
  public:
    std::vector<cpp_file>
    generate_client(const service::service_description& desc,
                    const wsdl_codegen_options& options = {}) const;

    std::vector<cpp_file>
    generate_server(const service::service_description& desc,
                    const wsdl_codegen_options& options = {}) const;
  };

} // namespace xb
