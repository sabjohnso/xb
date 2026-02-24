#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace xb {

  enum class output_mode { header_only, split, file_per_type };
  enum class validation_mode { none, on_demand };

  struct codegen_options {
    std::unordered_map<std::string, std::string> namespace_map;
    output_mode mode = output_mode::header_only;
    validation_mode validation = validation_mode::on_demand;
  };

  std::string
  to_snake_case(std::string_view name);

  std::string
  to_cpp_identifier(std::string_view xsd_name);

  std::string
  cpp_namespace_for(const std::string& xml_namespace,
                    const codegen_options& opts);

} // namespace xb
