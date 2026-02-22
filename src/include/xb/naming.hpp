#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace xb {

  struct codegen_options {
    std::unordered_map<std::string, std::string> namespace_map;
  };

  std::string
  to_snake_case(std::string_view name);

  std::string
  to_cpp_identifier(std::string_view xsd_name);

  std::string
  cpp_namespace_for(const std::string& xml_namespace,
                    const codegen_options& opts);

} // namespace xb
