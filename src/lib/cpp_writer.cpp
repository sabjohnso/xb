#include <xb/cpp_writer.hpp>

#include <sstream>
#include <string>

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
    write_doc_comment(std::ostream& os, const std::string& doc) {
      if (doc.empty()) return;
      std::istringstream iss(doc);
      std::string line;
      while (std::getline(iss, line))
        os << "/// " << line << '\n';
    }

    void
    write_struct(std::ostream& os, const cpp_struct& s) {
      write_doc_comment(os, s.doc_comment);
      if (s.fields.empty() && !s.generate_equality) {
        os << "struct " << s.name << " {};\n";
        return;
      }

      os << "struct " << s.name << " {\n";
      for (const auto& f : s.fields)
        write_field(os, f);

      if (s.generate_equality) {
        if (!s.fields.empty()) os << '\n';
        // Use elaborated type specifier (const struct X&) to prevent
        // shadowing when a field name matches the struct name.
        os << "  bool operator==(const struct " << s.name
           << "&) const = default;\n";
      }

      os << "};\n";
    }

    // Extract the inner type from wrapper<inner> (e.g. "std::optional<int>"
    // → "int")
    std::string
    extract_inner(const std::string& type, const std::string& wrapper) {
      auto prefix = wrapper + "<";
      if (type.size() <= prefix.size() + 1) return "";
      if (type.substr(0, prefix.size()) != prefix) return "";
      if (type.back() != '>') return "";
      return type.substr(prefix.size(), type.size() - prefix.size() - 1);
    }

    // Simple singularization: strip trailing 's' if present
    std::string
    singularize(const std::string& name) {
      if (name.size() > 1 && name.back() == 's')
        return name.substr(0, name.size() - 1);
      return name;
    }

    // Emit field accessor declarations (no bodies) for split-mode header
    void
    write_class_field_decls(std::ostream& os, const cpp_class& cls) {
      for (const auto& f : cls.fields) {
        std::string inner;
        os << '\n';

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          os << "  const " << f.type << "& " << f.name << "() const;\n";
          os << "  void set_" << f.name << "(" << inner << " value);\n";
          os << "  void clear_" << f.name << "();\n";
        } else if ((inner = extract_inner(f.type, "std::vector")) != "") {
          auto singular = singularize(f.name);
          os << "  auto " << f.name << "() const;\n";
          os << "  void add_" << singular << "(" << inner << " value);\n";
          os << "  std::size_t " << f.name << "_size() const;\n";
          os << "  void clear_" << f.name << "();\n";
        } else {
          os << "  const " << f.type << "& " << f.name << "() const;\n";
          os << "  void set_" << f.name << "(" << f.type << " value);\n";
        }
      }
    }

    // Emit field accessor definitions (with bodies) inline in the class
    void
    write_class_field_inline(std::ostream& os, const cpp_class& cls) {
      for (const auto& f : cls.fields) {
        std::string inner;
        os << '\n';

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          os << "  const " << f.type << "& " << f.name
             << "() const { return data_." << f.name << "; }\n";
          os << "  void set_" << f.name << "(" << inner << " value) { data_."
             << f.name << " = std::move(value); }\n";
          os << "  void clear_" << f.name << "() { data_." << f.name
             << ".reset(); }\n";
        } else if ((inner = extract_inner(f.type, "std::vector")) != "") {
          auto singular = singularize(f.name);
          os << "  auto " << f.name
             << "() const { return std::views::all(data_." << f.name
             << "); }\n";
          os << "  void add_" << singular << "(" << inner << " value) { data_."
             << f.name << ".push_back(std::move(value)); }\n";
          os << "  std::size_t " << f.name << "_size() const { return data_."
             << f.name << ".size(); }\n";
          os << "  void clear_" << f.name << "() { data_." << f.name
             << ".clear(); }\n";
        } else {
          os << "  const " << f.type << "& " << f.name
             << "() const { return data_." << f.name << "; }\n";
          os << "  void set_" << f.name << "(" << f.type << " value) { data_."
             << f.name << " = std::move(value); }\n";
        }
      }
    }

    // Emit class declaration (header)
    void
    write_class_header(std::ostream& os, const cpp_class& cls) {
      write_doc_comment(os, cls.doc_comment);
      os << "class " << cls.name << " {\n";
      os << "  detail::" << cls.detail_struct_name << " data_;\n";
      os << "public:\n";

      os << "  " << cls.name << "() = default;\n";
      os << "  explicit " << cls.name << "(detail::" << cls.detail_struct_name
         << " d)";

      if (cls.inline_methods) {
        os << " : data_(std::move(d)) {}\n";
        write_class_field_inline(os, cls);
      } else {
        os << ";\n";
        write_class_field_decls(os, cls);
      }

      if (cls.generate_equality) {
        os << '\n';
        os << "  bool operator==(const " << cls.name << "&) const = default;\n";
      }

      os << '\n';
      if (cls.inline_methods) {
        os << "  const detail::" << cls.detail_struct_name
           << "& data() const { return data_; }\n";
        os << "  detail::" << cls.detail_struct_name
           << "& data() { return data_; }\n";
      } else {
        os << "  const detail::" << cls.detail_struct_name
           << "& data() const;\n";
        os << "  detail::" << cls.detail_struct_name << "& data();\n";
      }

      os << "};\n";
    }

    // Emit out-of-line method definitions (source file)
    void
    write_class_source(std::ostream& os, const cpp_class& cls) {
      const auto& c = cls.name;
      const auto& d = cls.detail_struct_name;

      // Constructor
      os << c << "::" << c << "(detail::" << d
         << " d) : data_(std::move(d)) {}\n";

      for (const auto& f : cls.fields) {
        std::string inner;
        os << '\n';

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          os << "const " << f.type << "& " << c << "::" << f.name
             << "() const { return data_." << f.name << "; }\n";
          os << "void " << c << "::set_" << f.name << "(" << inner
             << " value) { data_." << f.name << " = std::move(value); }\n";
          os << "void " << c << "::clear_" << f.name << "() { data_." << f.name
             << ".reset(); }\n";
        } else if ((inner = extract_inner(f.type, "std::vector")) != "") {
          auto singular = singularize(f.name);
          os << "auto " << c << "::" << f.name
             << "() const { return std::views::all(data_." << f.name
             << "); }\n";
          os << "void " << c << "::add_" << singular << "(" << inner
             << " value) { data_." << f.name
             << ".push_back(std::move(value)); }\n";
          os << "std::size_t " << c << "::" << f.name
             << "_size() const { return data_." << f.name << ".size(); }\n";
          os << "void " << c << "::clear_" << f.name << "() { data_." << f.name
             << ".clear(); }\n";
        } else {
          os << "const " << f.type << "& " << c << "::" << f.name
             << "() const { return data_." << f.name << "; }\n";
          os << "void " << c << "::set_" << f.name << "(" << f.type
             << " value) { data_." << f.name << " = std::move(value); }\n";
        }
      }

      // data() accessors
      os << '\n';
      os << "const detail::" << d << "& " << c
         << "::data() const { return data_; }\n";
      os << "detail::" << d << "& " << c << "::data() { return data_; }\n";
    }

    void
    write_enum(std::ostream& os, const cpp_enum& e) {
      write_doc_comment(os, e.doc_comment);
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
    write_function_definition(std::ostream& os, const cpp_function& f,
                              bool emit_inline) {
      if (emit_inline) os << "inline ";
      os << f.return_type << ' ' << f.name << '(';
      os << f.parameters;
      os << ") {\n";
      os << f.body;
      os << "}\n";
    }

    void
    write_function_declaration(std::ostream& os, const cpp_function& f) {
      os << f.return_type << ' ' << f.name << '(';
      os << f.parameters;
      os << ");\n";
    }

    void
    write_function(std::ostream& os, const cpp_function& f, file_kind kind) {
      if (kind == file_kind::source) {
        // Source: only render non-inline function definitions
        if (!f.is_inline) write_function_definition(os, f, false);
        // Inline functions are skipped in source mode
        return;
      }

      // Header mode
      if (f.is_inline) {
        write_function_definition(os, f, true);
      } else {
        write_function_declaration(os, f);
      }
    }

    void
    write_decl(std::ostream& os, const cpp_decl& decl, file_kind kind) {
      std::visit(
          [&os, kind](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_function>) {
              write_function(os, d, kind);
            } else if constexpr (std::is_same_v<T, cpp_class>) {
              if (kind == file_kind::source) {
                if (!d.inline_methods) write_class_source(os, d);
              } else {
                write_class_header(os, d);
              }
            } else if (kind == file_kind::source) {
              // Source mode: skip all non-function declarations
              return;
            } else if constexpr (std::is_same_v<T, cpp_struct>) {
              write_struct(os, d);
            } else if constexpr (std::is_same_v<T, cpp_enum>) {
              write_enum(os, d);
            } else if constexpr (std::is_same_v<T, cpp_type_alias>) {
              write_type_alias(os, d);
            } else if constexpr (std::is_same_v<T, cpp_forward_decl>) {
              write_forward_decl(os, d);
            } else if constexpr (std::is_same_v<T, cpp_raw_text>) {
              os << d.text;
            }
          },
          decl);
    }

    bool
    has_visible_decl(const cpp_decl& decl, file_kind kind) {
      return std::visit(
          [kind](const auto& d) -> bool {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, cpp_function>) {
              if (kind == file_kind::source) return !d.is_inline;
              return true;
            } else if constexpr (std::is_same_v<T, cpp_class>) {
              if (kind == file_kind::source) return !d.inline_methods;
              return true;
            } else {
              return kind == file_kind::header;
            }
          },
          decl);
    }

    void
    write_namespace(std::ostream& os, const cpp_namespace& ns, file_kind kind) {
      os << "\nnamespace " << ns.name << " {\n";
      for (const auto& decl : ns.declarations) {
        if (has_visible_decl(decl, kind)) {
          os << '\n';
          write_decl(os, decl, kind);
        }
      }
      os << "\n} // namespace " << ns.name << '\n';
    }

  } // namespace

  std::string
  cpp_writer::write(const cpp_file& file) const {
    return write(file, write_options{file.kind});
  }

  std::string
  cpp_writer::write(const cpp_file& file, write_options opts) const {
    std::ostringstream os;
    if (opts.kind == file_kind::header) os << "#pragma once\n";
    write_includes(os, file.includes);
    for (const auto& ns : file.namespaces)
      write_namespace(os, ns, opts.kind);
    return os.str();
  }

} // namespace xb
