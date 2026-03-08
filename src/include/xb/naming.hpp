#pragma once

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xb {

  enum class output_mode { header_only, split, file_per_type };
  enum class validation_mode { none, on_demand };
  enum class json_mode { none, enabled };
  enum class encapsulation_mode { raw_struct, wrapped };

  enum class naming_style {
    snake_case,
    pascal_case,
    camel_case,
    upper_snake,
    original
  };

  enum class naming_category { type_, field, enum_value, function };

  struct naming_options {
    naming_style type_style = naming_style::snake_case;
    naming_style field_style = naming_style::snake_case;
    naming_style enum_style = naming_style::snake_case;
    naming_style function_style = naming_style::snake_case;

    std::vector<std::pair<std::string, std::string>> type_rules;
    std::vector<std::pair<std::string, std::string>> field_rules;
  };

  struct codegen_options {
    std::unordered_map<std::string, std::string> namespace_map;
    output_mode mode = output_mode::header_only;
    validation_mode validation = validation_mode::on_demand;
    json_mode json = json_mode::none;
    std::string header_suffix = ".hpp";
    std::string source_suffix = ".cpp";
    naming_options naming;
    bool separate_fwd_header = false;
    bool generate_docs = false;
    encapsulation_mode encapsulation = encapsulation_mode::raw_struct;
  };

  // -- Naming style conversions --

  std::string
  to_snake_case(std::string_view name);

  std::string
  to_pascal_case(std::string_view name);

  std::string
  to_camel_case(std::string_view name);

  std::string
  to_upper_snake_case(std::string_view name);

  std::string
  apply_naming_style(std::string_view name, naming_style style);

  // Apply naming style for a category, then regex rules, then sanitize
  // as a valid C++ identifier.
  std::string
  apply_naming(std::string_view name, naming_category category,
               const naming_options& opts);

  std::string
  to_cpp_identifier(std::string_view xsd_name);

  std::string
  cpp_namespace_for(const std::string& xml_namespace,
                    const codegen_options& opts);

  std::string
  default_namespace_for(const std::string& xml_namespace,
                        const std::set<std::string>& all_namespaces,
                        const codegen_options& opts = {});

} // namespace xb
