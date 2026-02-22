#pragma once

#include <string>
#include <variant>
#include <vector>

namespace xb {

  struct cpp_include {
    std::string path;

    bool
    operator==(const cpp_include&) const = default;
  };

  struct cpp_enumerator {
    std::string name;
    std::string xml_value;

    bool
    operator==(const cpp_enumerator&) const = default;
  };

  struct cpp_enum {
    std::string name;
    std::vector<cpp_enumerator> values;

    bool
    operator==(const cpp_enum&) const = default;
  };

  struct cpp_field {
    std::string type;
    std::string name;
    std::string default_value;

    bool
    operator==(const cpp_field&) const = default;
  };

  struct cpp_struct {
    std::string name;
    std::vector<cpp_field> fields;
    bool generate_equality = true;

    bool
    operator==(const cpp_struct&) const = default;
  };

  struct cpp_type_alias {
    std::string name;
    std::string target;

    bool
    operator==(const cpp_type_alias&) const = default;
  };

  struct cpp_forward_decl {
    std::string name;

    bool
    operator==(const cpp_forward_decl&) const = default;
  };

  using cpp_decl =
      std::variant<cpp_struct, cpp_enum, cpp_type_alias, cpp_forward_decl>;

  struct cpp_namespace {
    std::string name;
    std::vector<cpp_decl> declarations;

    bool
    operator==(const cpp_namespace&) const = default;
  };

  struct cpp_file {
    std::string filename;
    std::vector<cpp_include> includes;
    std::vector<cpp_namespace> namespaces;

    bool
    operator==(const cpp_file&) const = default;
  };

} // namespace xb
