#include <xb/cpp_writer.hpp>

#include <set>
#include <sstream>
#include <string>

namespace {
  bool
  is_template_type(const std::string& type) {
    return type.find('<') != std::string::npos;
  }

  std::string
  alias_name(const std::string& field_name) {
    return field_name + "_type";
  }

  // Extract the template argument from "Wrapper<Arg>" accounting for
  // nested angle brackets.  Returns "" if type doesn't match the pattern.
  std::string
  extract_template_arg(const std::string& type) {
    auto pos = type.find('<');
    if (pos == std::string::npos || type.back() != '>') return "";
    return type.substr(pos + 1, type.size() - pos - 2);
  }

  // Return the wrapper prefix before '<' (e.g. "std::vector")
  std::string
  template_wrapper(const std::string& type) {
    auto pos = type.find('<');
    if (pos == std::string::npos) return "";
    return type.substr(0, pos);
  }

  // Check whether emitting "using <alias> = <type>" would be
  // self-referential (the alias name appears inside the type).
  bool
  alias_collides(const std::string& alias, const std::string& type) {
    auto pos = type.find(alias);
    if (pos == std::string::npos) return false;
    // Verify it's a whole-word match (not a substring of a longer identifier)
    auto end = pos + alias.size();
    if (end < type.size() && (std::isalnum(type[end]) || type[end] == '_'))
      return false;
    if (pos > 0 && (std::isalnum(type[pos - 1]) || type[pos - 1] == '_'))
      return false;
    return true;
  }
} // namespace

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
    write_field(std::ostream& os, const cpp_field& field,
                const std::set<std::string>* emitted_aliases = nullptr) {
      bool use_alias =
          emitted_aliases && emitted_aliases->count(alias_name(field.name)) > 0;
      std::string type = use_alias ? alias_name(field.name) : field.type;
      os << "  " << type << ' ' << field.name;
      if (!field.default_value.empty())
        os << " = " << field.default_value;
      else
        os << "{}";
      os << ";\n";
    }

    void
    write_doc_comment(std::ostream& os, const std::string& doc) {
      if (doc.empty()) return;
      os << "/**\n";
      std::istringstream iss(doc);
      std::string line;
      bool first = true;
      while (std::getline(iss, line)) {
        if (first) {
          os << " * @brief " << line << '\n';
          first = false;
        } else {
          os << " * " << line << '\n';
        }
      }
      os << " */\n";
    }

    // Write a Doxygen comment block for a class, including @details
    // and @nosubgrouping.  Generates a brief even if no annotation.
    void
    write_class_doc_comment(std::ostream& os, const std::string& name,
                            const std::string& doc) {
      os << "/**\n";
      os << " * @brief Class corresponding to the %" << name
         << " schema type.\n";
      if (!doc.empty()) {
        os << " *\n";
        os << " * @details ";
        std::istringstream iss(doc);
        std::string line;
        bool first = true;
        while (std::getline(iss, line)) {
          if (first) {
            os << line << '\n';
            first = false;
          } else {
            os << " * " << line << '\n';
          }
        }
      }
      os << " *\n";
      os << " * @nosubgrouping\n";
      os << " */\n";
    }

    void
    write_struct(std::ostream& os, const cpp_struct& s) {
      write_doc_comment(os, s.doc_comment);
      if (s.fields.empty() && !s.generate_equality) {
        os << "struct " << s.name << " {};\n";
        return;
      }

      os << "struct " << s.name << " {\n";

      // Collect all field names and bare type names so we can detect
      // alias collisions (e.g. alias "md_sec_size_type" from field
      // "md_sec_size" would shadow the actual type "md_sec_size_type").
      std::set<std::string> reserved_names;
      reserved_names.insert(s.name); // prevent alias shadowing enclosing type
      for (const auto& f : s.fields) {
        reserved_names.insert(f.name);
        // Extract the bare type name (strip std::optional, std::vector, etc.)
        auto t = f.type;
        for (const auto* prefix : {"std::optional<", "std::vector<"}) {
          if (t.starts_with(prefix)) {
            t = t.substr(std::string_view(prefix).size());
            if (!t.empty() && t.back() == '>') t.pop_back();
          }
        }
        reserved_names.insert(t);
      }

      // Emit type aliases for template specialization fields.
      // For nested specializations (e.g. vector<variant<A,B>>), emit an
      // inner element alias first, then an outer alias that references it.
      // Track which aliases we actually emit so write_field can use them.
      std::set<std::string> emitted_aliases;
      for (const auto& f : s.fields) {
        if (!is_template_type(f.type)) continue;
        if (alias_collides(alias_name(f.name), f.type)) continue;
        // Skip if the alias name collides with a field name or type name
        if (reserved_names.count(alias_name(f.name))) continue;

        auto inner = extract_template_arg(f.type);
        // Check if the inner type is itself a single template specialization
        // (e.g. std::variant<A,B>). We need to find the first comma at depth 0
        // (outside angle brackets). If there's no top-level comma but there IS
        // a '<', then the inner is a single template type.
        bool has_top_level_comma = false;
        bool has_lt = false;
        int depth = 0;
        for (char c : inner) {
          if (c == '<') {
            has_lt = true;
            ++depth;
          } else if (c == '>')
            --depth;
          else if (c == ',' && depth == 0) {
            has_top_level_comma = true;
            break;
          }
        }
        bool inner_is_template = has_lt && !has_top_level_comma;
        if (inner_is_template) {
          // Nested: emit inner alias then outer using it
          std::string inner_alias = f.name + "_element_type";
          std::string wrapper = template_wrapper(f.type);
          os << "  using " << inner_alias << " = " << inner << ";\n";
          os << "  using " << alias_name(f.name) << " = " << wrapper << "<"
             << inner_alias << ">;\n";
        } else {
          os << "  using " << alias_name(f.name) << " = " << f.type << ";\n";
        }
        emitted_aliases.insert(alias_name(f.name));
      }

      for (const auto& f : s.fields)
        write_field(os, f, &emitted_aliases);

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

    // Resolve the display type for a class field: for template fields,
    // use the alias defined in the raw struct.
    std::string
    class_field_type(const cpp_class& cls, const cpp_field& f) {
      if (is_template_type(f.type) &&
          !alias_collides(alias_name(f.name), f.type))
        return cls.raw_struct_name + "::" + alias_name(f.name);
      return f.type;
    }

    // Resolve the element type for a vector field's indexed access.
    // For nested templates (e.g. vector<variant<A,B>>), returns the
    // element alias from the raw struct.  Otherwise returns the raw
    // inner type.
    std::string
    class_element_type(const cpp_class& cls, const cpp_field& f) {
      auto inner = extract_template_arg(f.type);
      if (is_template_type(inner))
        return cls.raw_struct_name + "::" + f.name + "_element_type";
      return inner;
    }

    // Count how many vector fields a class has
    std::size_t
    count_sequence_fields(const cpp_class& cls) {
      std::size_t n = 0;
      for (const auto& f : cls.fields)
        if (extract_inner(f.type, "std::vector") != "") ++n;
      return n;
    }

    // Emit a single-line Doxygen brief if docs are enabled.
    void
    member_doc(std::ostream& os, bool enabled, const std::string& brief) {
      if (!enabled) return;
      os << "\n  /** @brief " << brief << " */";
    }

    // Emit sequence (vector) field declarations for a class.
    // If emit_body is true, inline method bodies are included.
    // If sole_sequence is true, unprefixed aliases are also emitted.
    void
    write_sequence_field(std::ostream& os, const cpp_class& cls,
                         const cpp_field& f, bool emit_body,
                         bool sole_sequence) {
      std::string ftype = class_field_type(cls, f);
      std::string etype = class_element_type(cls, f);
      std::string d = "data_." + f.name;
      auto n = f.name;
      bool docs = cls.generate_member_docs;

      auto def = [&](const std::string& sig, const std::string& body) {
        if (emit_body)
          os << "\n  " << sig << " { " << body << " }\n";
        else
          os << "\n  " << sig << ";\n";
      };

      // Indexed access
      member_doc(os, docs, "Access " + n + " element at index.");
      def("const " + etype + "& " + n + "(std::size_t i) const",
          "return " + d + "[i];");
      member_doc(os, docs, "Access " + n + " element at index.");
      def(etype + "& " + n + "(std::size_t i)", "return " + d + "[i];");

      // Iterators (prefixed)
      member_doc(os, docs, "Return iterator to the beginning of " + n + ".");
      def(ftype + "::iterator " + n + "_begin()", "return " + d + ".begin();");
      member_doc(os, docs, "Return iterator past the end of " + n + ".");
      def(ftype + "::iterator " + n + "_end()", "return " + d + ".end();");
      member_doc(os, docs, "Return iterator to the beginning of " + n + ".");
      def(ftype + "::const_iterator " + n + "_begin() const",
          "return " + d + ".begin();");
      member_doc(os, docs, "Return iterator past the end of " + n + ".");
      def(ftype + "::const_iterator " + n + "_end() const",
          "return " + d + ".end();");
      member_doc(os, docs, "Return iterator to the beginning of " + n + ".");
      def(ftype + "::const_iterator " + n + "_cbegin() const",
          "return " + d + ".cbegin();");
      member_doc(os, docs, "Return iterator past the end of " + n + ".");
      def(ftype + "::const_iterator " + n + "_cend() const",
          "return " + d + ".cend();");

      // Size and clear
      member_doc(os, docs, "Return the number of " + n + " elements.");
      def("std::size_t " + n + "_size() const", "return " + d + ".size();");
      member_doc(os, docs, "Remove all " + n + " elements.");
      def("void clear_" + n + "()", d + ".clear();");

      // Mutators
      member_doc(os, docs, "Insert an element into " + n + ".");
      def(ftype + "::iterator " + n + "_insert(" + ftype +
              "::const_iterator pos, const " + etype + "& value)",
          "return " + d + ".insert(pos, value);");
      member_doc(os, docs, "Erase an element from " + n + ".");
      def(ftype + "::iterator " + n + "_erase(" + ftype +
              "::const_iterator pos)",
          "return " + d + ".erase(pos);");
      member_doc(os, docs, "Append an element to " + n + ".");
      def("void " + n + "_push_back(" + etype + " value)",
          d + ".push_back(std::move(value));");
      member_doc(os, docs, "Remove the last " + n + " element.");
      def("void " + n + "_pop_back()", d + ".pop_back();");

      // emplace with forwarding
      member_doc(os, docs, "Emplace an element into " + n + ".");
      if (emit_body) {
        os << "\n  template <typename... Args>\n";
        os << "  " << ftype << "::iterator " << n << "_emplace(" << ftype
           << "::const_iterator pos, Args&&... args) { return " << d
           << ".emplace(pos, std::forward<Args>(args)...); }\n";
      } else {
        os << "\n  template <typename... Args>\n";
        os << "  " << ftype << "::iterator " << n << "_emplace(" << ftype
           << "::const_iterator pos, Args&&... args);\n";
      }

      // insert_range
      member_doc(os, docs, "Insert a range of elements into " + n + ".");
      if (emit_body) {
        os << "\n  template <typename Range>\n";
        os << "  void " << n << "_insert_range(" << ftype
           << "::const_iterator pos, Range&& range)"
           << " { " << d
           << ".insert(pos, std::begin(range), std::end(range)); }\n";
      } else {
        os << "\n  template <typename Range>\n";
        os << "  void " << n << "_insert_range(" << ftype
           << "::const_iterator pos, Range&& range);\n";
      }

      // append_range
      member_doc(os, docs, "Append a range of elements to " + n + ".");
      if (emit_body) {
        os << "\n  template <typename Range>\n";
        os << "  void " << n << "_append_range(Range&& range)"
           << " { " << d << ".insert(" << d
           << ".end(), std::begin(range), std::end(range)); }\n";
      } else {
        os << "\n  template <typename Range>\n";
        os << "  void " << n << "_append_range(Range&& range);\n";
      }

      // emplace_back with forwarding
      member_doc(os, docs, "Emplace an element at the end of " + n + ".");
      if (emit_body) {
        os << "\n  template <typename... Args>\n";
        os << "  " << etype << "& " << n
           << "_emplace_back(Args&&... args) { return " << d
           << ".emplace_back(std::forward<Args>(args)...); }\n";
      } else {
        os << "\n  template <typename... Args>\n";
        os << "  " << etype << "& " << n << "_emplace_back(Args&&... args);\n";
      }

      // Unprefixed aliases for sole sequence
      if (sole_sequence) {
        member_doc(os, docs, "Alias for " + n + "_begin.");
        def(ftype + "::iterator begin()", "return " + d + ".begin();");
        member_doc(os, docs, "Alias for " + n + "_end.");
        def(ftype + "::iterator end()", "return " + d + ".end();");
        member_doc(os, docs, "Alias for " + n + "_begin.");
        def(ftype + "::const_iterator begin() const",
            "return " + d + ".begin();");
        member_doc(os, docs, "Alias for " + n + "_end.");
        def(ftype + "::const_iterator end() const", "return " + d + ".end();");
        member_doc(os, docs, "Alias for " + n + "_cbegin.");
        def(ftype + "::const_iterator cbegin() const",
            "return " + d + ".cbegin();");
        member_doc(os, docs, "Alias for " + n + "_cend.");
        def(ftype + "::const_iterator cend() const",
            "return " + d + ".cend();");
        member_doc(os, docs, "Alias for " + n + "_size.");
        def("std::size_t size() const", "return " + d + ".size();");
      }
    }

    // Emit out-of-line sequence method definitions (source file)
    void
    write_sequence_source(std::ostream& os, const cpp_class& cls,
                          const cpp_field& f, bool sole_sequence) {
      const auto& c = cls.name;
      std::string ftype = class_field_type(cls, f);
      std::string etype = class_element_type(cls, f);
      std::string d = "data_." + f.name;
      auto n = f.name;

      auto def = [&](const std::string& ret, const std::string& sig,
                     const std::string& body) {
        os << ret << " " << c << "::" << sig << " { " << body << " }\n\n";
      };

      // Indexed access
      def("const " + etype + "&", n + "(std::size_t i) const",
          "return " + d + "[i];");
      def(etype + "&", n + "(std::size_t i)", "return " + d + "[i];");

      // Iterators
      def(ftype + "::iterator", n + "_begin()", "return " + d + ".begin();");
      def(ftype + "::iterator", n + "_end()", "return " + d + ".end();");
      def(ftype + "::const_iterator", n + "_begin() const",
          "return " + d + ".begin();");
      def(ftype + "::const_iterator", n + "_end() const",
          "return " + d + ".end();");
      def(ftype + "::const_iterator", n + "_cbegin() const",
          "return " + d + ".cbegin();");
      def(ftype + "::const_iterator", n + "_cend() const",
          "return " + d + ".cend();");

      // Size and clear
      def("std::size_t", n + "_size() const", "return " + d + ".size();");
      def("void", "clear_" + n + "()", d + ".clear();");

      // Mutators
      def(ftype + "::iterator",
          n + "_insert(" + ftype + "::const_iterator pos, const " + etype +
              "& value)",
          "return " + d + ".insert(pos, value);");
      def(ftype + "::iterator", n + "_erase(" + ftype + "::const_iterator pos)",
          "return " + d + ".erase(pos);");
      def("void", n + "_push_back(" + etype + " value)",
          d + ".push_back(std::move(value));");
      def("void", n + "_pop_back()", d + ".pop_back();");

      // Unprefixed aliases for sole sequence
      if (sole_sequence) {
        def(ftype + "::iterator", "begin()", "return " + d + ".begin();");
        def(ftype + "::iterator", "end()", "return " + d + ".end();");
        def(ftype + "::const_iterator", "begin() const",
            "return " + d + ".begin();");
        def(ftype + "::const_iterator", "end() const",
            "return " + d + ".end();");
        def(ftype + "::const_iterator", "cbegin() const",
            "return " + d + ".cbegin();");
        def(ftype + "::const_iterator", "cend() const",
            "return " + d + ".cend();");
        def("std::size_t", "size() const", "return " + d + ".size();");
      }
    }

    // Emit field accessor declarations (no bodies) for split-mode header
    void
    write_class_field_decls(std::ostream& os, const cpp_class& cls) {
      bool sole_seq = count_sequence_fields(cls) == 1;
      bool docs = cls.generate_member_docs;
      for (const auto& f : cls.fields) {
        std::string inner;
        std::string ftype = class_field_type(cls, f);

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          member_doc(os, docs, "Get the " + f.name + " value.");
          os << "\n  const " << ftype << "& " << f.name << "() const;\n";
          member_doc(os, docs, "Set the " + f.name + " value.");
          os << "\n  void set_" << f.name << "(" << inner << " value);\n";
          member_doc(os, docs, "Clear the " + f.name + " value.");
          os << "\n  void clear_" << f.name << "();\n";
        } else if (extract_inner(f.type, "std::vector") != "") {
          write_sequence_field(os, cls, f, false, sole_seq);
        } else {
          member_doc(os, docs, "Get the " + f.name + " value.");
          os << "\n  const " << ftype << "& " << f.name << "() const;\n";
          member_doc(os, docs, "Set the " + f.name + " value.");
          os << "\n  void set_" << f.name << "(" << ftype << " value);\n";
        }
      }
    }

    // Emit field accessor definitions (with bodies) inline in the class
    void
    write_class_field_inline(std::ostream& os, const cpp_class& cls) {
      bool sole_seq = count_sequence_fields(cls) == 1;
      bool docs = cls.generate_member_docs;
      for (const auto& f : cls.fields) {
        std::string inner;
        std::string ftype = class_field_type(cls, f);

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          member_doc(os, docs, "Get the " + f.name + " value.");
          os << "\n  const " << ftype << "& " << f.name
             << "() const { return data_." << f.name << "; }\n";
          member_doc(os, docs, "Set the " + f.name + " value.");
          os << "\n  void set_" << f.name << "(" << inner << " value) { data_."
             << f.name << " = std::move(value); }\n";
          member_doc(os, docs, "Clear the " + f.name + " value.");
          os << "\n  void clear_" << f.name << "() { data_." << f.name
             << ".reset(); }\n";
        } else if (extract_inner(f.type, "std::vector") != "") {
          write_sequence_field(os, cls, f, true, sole_seq);
        } else {
          member_doc(os, docs, "Get the " + f.name + " value.");
          os << "\n  const " << ftype << "& " << f.name
             << "() const { return data_." << f.name << "; }\n";
          member_doc(os, docs, "Set the " + f.name + " value.");
          os << "\n  void set_" << f.name << "(" << ftype << " value) { data_."
             << f.name << " = std::move(value); }\n";
        }
      }
    }

    // Emit class declaration (header)
    void
    write_class_header(std::ostream& os, const cpp_class& cls) {
      write_class_doc_comment(os, cls.name, cls.doc_comment);
      bool docs = cls.generate_member_docs;
      os << "class " << cls.name << " {\n";
      os << "  " << cls.raw_struct_name << " data_;\n";
      os << "public:\n";

      member_doc(os, docs, "Default constructor.");
      os << "\n  " << cls.name << "() = default;\n";
      member_doc(os, docs, "Construct from raw data.");
      os << "\n  explicit " << cls.name << "(" << cls.raw_struct_name << " d)";

      if (cls.inline_methods) {
        os << " : data_(std::move(d)) {}\n";
        write_class_field_inline(os, cls);
      } else {
        os << ";\n";
        write_class_field_decls(os, cls);
      }

      if (cls.generate_equality) {
        member_doc(os, docs, "Equality comparison.");
        os << '\n';
        os << "  bool operator==(const " << cls.name << "&) const = default;\n";
      }

      os << "};\n";
    }

    // Emit out-of-line method definitions (source file)
    void
    write_class_source(std::ostream& os, const cpp_class& cls) {
      const auto& c = cls.name;
      const auto& d = cls.raw_struct_name;

      // Constructor
      os << c << "::" << c << "(" << d << " d) : data_(std::move(d)) {}\n\n";

      for (const auto& f : cls.fields) {
        std::string inner;
        std::string ftype = class_field_type(cls, f);

        if ((inner = extract_inner(f.type, "std::optional")) != "") {
          os << "const " << ftype << "& " << c << "::" << f.name
             << "() const { return data_." << f.name << "; }\n\n";
          os << "void " << c << "::set_" << f.name << "(" << inner
             << " value) { data_." << f.name << " = std::move(value); }\n\n";
          os << "void " << c << "::clear_" << f.name << "() { data_." << f.name
             << ".reset(); }\n\n";
        } else if (extract_inner(f.type, "std::vector") != "") {
          write_sequence_source(os, cls, f, count_sequence_fields(cls) == 1);
        } else {
          os << "const " << ftype << "& " << c << "::" << f.name
             << "() const { return data_." << f.name << "; }\n\n";
          os << "void " << c << "::set_" << f.name << "(" << ftype
             << " value) { data_." << f.name << " = std::move(value); }\n\n";
        }
      }
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
      os << "}\n\n";
    }

    void
    write_function_declaration(std::ostream& os, const cpp_function& f) {
      os << f.return_type << ' ' << f.name << '(';
      os << f.parameters;
      os << ");\n\n";
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
