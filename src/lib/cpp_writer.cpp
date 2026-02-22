#include <xb/cpp_writer.hpp>

#include <sstream>

namespace xb {

  namespace {

    void
    write_includes(std::ostream& os, const std::vector<cpp_include>& includes) {
      if (includes.empty()) return;

      // Partition into system (<...>) and local ("...") includes
      std::vector<const cpp_include*> system_includes;
      std::vector<const cpp_include*> local_includes;

      for (const auto& inc : includes) {
        if (!inc.path.empty() && inc.path.front() == '<')
          system_includes.push_back(&inc);
        else
          local_includes.push_back(&inc);
      }

      os << '\n';

      for (const auto* inc : system_includes)
        os << "#include " << inc->path << '\n';

      if (!system_includes.empty() && !local_includes.empty()) os << '\n';

      for (const auto* inc : local_includes)
        os << "#include " << inc->path << '\n';
    }

    void
    write_field(std::ostream& os, const cpp_field& field) {
      os << "  " << field.type << ' ' << field.name;
      if (!field.default_value.empty()) os << " = " << field.default_value;
      os << ";\n";
    }

    void
    write_struct(std::ostream& os, const cpp_struct& s) {
      if (s.fields.empty() && !s.generate_equality) {
        os << "struct " << s.name << " {};\n";
        return;
      }

      os << "struct " << s.name << " {\n";
      for (const auto& f : s.fields)
        write_field(os, f);

      if (s.generate_equality) {
        if (!s.fields.empty()) os << '\n';
        os << "  bool operator==(const " << s.name << "&) const = default;\n";
      }

      os << "};\n";
    }

    void
    write_enum(std::ostream& os, const cpp_enum& e) {
      os << "enum class " << e.name << " {\n";
      for (const auto& v : e.values)
        os << "  " << v.name << ",\n";
      os << "};\n";

      // to_string free function
      os << "\ninline std::string_view to_string(" << e.name << " v) {\n";
      os << "  switch (v) {\n";
      for (const auto& v : e.values) {
        os << "  case " << e.name << "::" << v.name << ": return \""
           << v.xml_value << "\";\n";
      }
      os << "  }\n";
      os << "  return \"\";\n";
      os << "}\n";

      // from_string free function
      os << "\ninline " << e.name << ' ' << e.name
         << "_from_string(std::string_view s) {\n";
      for (const auto& v : e.values) {
        os << "  if (s == \"" << v.xml_value << "\") return " << e.name
           << "::" << v.name << ";\n";
      }
      os << "  throw std::invalid_argument(std::string(\"invalid " << e.name
         << " value: \") + std::string(s));\n";
      os << "}\n";
    }

    void
    write_type_alias(std::ostream& os, const cpp_type_alias& a) {
      os << "using " << a.name << " = " << a.target << ";\n";
    }

    void
    write_forward_decl(std::ostream& os, const cpp_forward_decl& d) {
      os << "struct " << d.name << ";\n";
    }

    void
    write_decl(std::ostream& os, const cpp_decl& decl) {
      std::visit(
          [&os](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_struct>)
              write_struct(os, d);
            else if constexpr (std::is_same_v<T, cpp_enum>)
              write_enum(os, d);
            else if constexpr (std::is_same_v<T, cpp_type_alias>)
              write_type_alias(os, d);
            else if constexpr (std::is_same_v<T, cpp_forward_decl>)
              write_forward_decl(os, d);
          },
          decl);
    }

    void
    write_namespace(std::ostream& os, const cpp_namespace& ns) {
      os << "\nnamespace " << ns.name << " {\n";
      for (const auto& decl : ns.declarations) {
        os << '\n';
        write_decl(os, decl);
      }
      os << "\n} // namespace " << ns.name << '\n';
    }

  } // namespace

  std::string
  cpp_writer::write(const cpp_file& file) const {
    std::ostringstream os;
    os << "#pragma once\n";
    write_includes(os, file.includes);
    for (const auto& ns : file.namespaces)
      write_namespace(os, ns);
    return os.str();
  }

} // namespace xb
