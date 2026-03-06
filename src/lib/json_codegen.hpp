#pragma once

#include <xb/cpp_code.hpp>
#include <xb/simple_type.hpp>

#include "codegen_internal.hpp"

#include <optional>
#include <set>
#include <string>

namespace xb {

  // Generate to_json/from_json for an enum type
  std::optional<cpp_function>
  generate_enum_to_json(const cpp_enum& e);

  std::optional<cpp_function>
  generate_enum_from_json(const cpp_enum& e);

  // Generate to_json/from_json for a union simple type (std::variant)
  std::optional<cpp_function>
  generate_union_to_json(const simple_type& st, const type_resolver& resolver,
                         std::set<std::string>& seen_variant_types);

  std::optional<cpp_function>
  generate_union_from_json(const simple_type& st, const type_resolver& resolver,
                           std::set<std::string>& seen_variant_types);

  // Generate to_json/from_json for a complex type struct
  cpp_function
  generate_to_json_function(const cpp_struct& s);

  cpp_function
  generate_from_json_function(const cpp_struct& s);

} // namespace xb
