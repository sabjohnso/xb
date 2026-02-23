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

  struct cpp_function {
    std::string return_type;
    std::string name;
    std::string parameters;
    std::string body;
    bool is_inline = true;

    bool
    operator==(const cpp_function&) const = default;
  };

  using cpp_decl = std::variant<cpp_struct, cpp_enum, cpp_type_alias,
                                cpp_forward_decl, cpp_function>;

  struct cpp_namespace {
    std::string name;
    std::vector<cpp_decl> declarations;

    bool
    operator==(const cpp_namespace&) const = default;
  };

  enum class file_kind { header, source };

  struct cpp_file {
    std::string filename;
    std::vector<cpp_include> includes;
    std::vector<cpp_namespace> namespaces;
    file_kind kind = file_kind::header;

    bool
    operator==(const cpp_file&) const = default;
  };

} // namespace xb
