#include <xb/test_harness_generator.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group.hpp>
#include <xb/naming.hpp>
#include <xb/test_value_generator.hpp>
#include <xb/test_vector_generator.hpp>
#include <xb/type_map.hpp>

#include <ostream>
#include <string>

namespace xb {

  namespace {

    std::string
    cpp_type_name(const qname& xsd_type) {
      return to_snake_case(xsd_type.local_name());
    }

    std::string
    cpp_field_name(const qname& xsd_name) {
      return to_cpp_identifier(xsd_name.local_name());
    }

    // Derive a file stem from a namespace URI: last segment, snake-cased.
    // Matches codegen's ns_stem / stem_for_namespace logic.
    std::string
    ns_stem(const std::string& xml_ns) {
      if (xml_ns.empty()) return "generated";
      std::string cleaned = xml_ns;
      while (!cleaned.empty() && cleaned.back() == '#')
        cleaned.pop_back();
      auto last_slash = cleaned.rfind('/');
      auto last_colon = cleaned.rfind(':');
      auto last_sep = std::string::npos;
      if (last_slash != std::string::npos && last_colon != std::string::npos)
        last_sep = std::max(last_slash, last_colon);
      else if (last_slash != std::string::npos)
        last_sep = last_slash;
      else
        last_sep = last_colon;
      if (last_sep != std::string::npos)
        return to_snake_case(cleaned.substr(last_sep + 1));
      return to_snake_case(cleaned);
    }

    // Derive C++ namespace from XML namespace URI using the short-name
    // strategy (last URI segment).  Matches codegen's default output.
    std::string
    cpp_ns(const std::string& xml_ns) {
      if (xml_ns.empty()) return {};
      return ns_stem(xml_ns);
    }

    std::string
    qualified_type(const qname& type_name) {
      std::string ns = cpp_ns(type_name.namespace_uri());
      std::string type = cpp_type_name(type_name);
      if (ns.empty()) return type;
      return ns + "::" + type;
    }

    std::string
    escape_string_literal(const std::string& s) {
      std::string result;
      result.reserve(s.size() + 2);
      result += '"';
      for (char c : s) {
        switch (c) {
          case '"':
            result += "\\\"";
            break;
          case '\\':
            result += "\\\\";
            break;
          case '\n':
            result += "\\n";
            break;
          case '\t':
            result += "\\t";
            break;
          default:
            result += c;
        }
      }
      result += '"';
      return result;
    }

    // Check if a C++ type is a string type (needs quoting).
    bool
    is_string_cpp_type(const std::string& cpp_type) {
      return cpp_type == "std::string" || cpp_type == "std::string_view";
    }

    // Format a value literal appropriate for its C++ type.
    std::string
    format_value_literal(const std::string& xml_text,
                         const std::string& cpp_type) {
      if (is_string_cpp_type(cpp_type)) return escape_string_literal(xml_text);
      if (cpp_type == "bool") {
        return (xml_text == "true" || xml_text == "1") ? "true" : "false";
      }
      // Numeric types: XML text is a valid C++ literal
      if (xml_text.empty()) return "0";
      return xml_text;
    }

    // Look up the C++ type for a field by finding it in the parent complex
    // type.
    std::string
    resolve_field_cpp_type(const qname& field_name, const qname& parent_type,
                           const schema_set& schemas) {
      static auto types = type_map::defaults();

      // Check if it's an attribute or element of the parent type
      const auto* ct = schemas.find_complex_type(parent_type);
      if (!ct) return "std::string";

      // Check attributes
      for (const auto& attr : ct->attributes()) {
        if (attr.name == field_name) {
          auto* mapping = types.find(attr.type_name.local_name());
          if (mapping) return mapping->cpp_type;
          return "std::string";
        }
      }

      // Check elements in the content model
      const auto& content = ct->content();
      if (content.kind != content_kind::element_only &&
          content.kind != content_kind::mixed)
        return "std::string";
      const auto* cc = std::get_if<complex_content>(&content.detail);
      if (!cc || !cc->content_model) return "std::string";

      for (const auto& p : cc->content_model->particles()) {
        if (const auto* elem = std::get_if<element_decl>(&p.term)) {
          if (elem->name() == field_name) {
            if (elem->type_name().namespace_uri() ==
                "http://www.w3.org/2001/XMLSchema") {
              auto* mapping = types.find(elem->type_name().local_name());
              if (mapping) return mapping->cpp_type;
            }
            return "std::string";
          }
        }
      }
      return "std::string";
    }

    void
    write_field_assignment(std::ostream& out, const std::string& var,
                           const test_field_value& field,
                           const qname& parent_type,
                           const schema_set& schemas) {
      const auto& fname = cpp_field_name(field.field_name);
      auto cpp_type =
          resolve_field_cpp_type(field.field_name, parent_type, schemas);

      if (const auto* val = std::get_if<std::string>(&field.content)) {
        out << "  " << var << "." << fname << " = "
            << format_value_literal(*val, cpp_type) << ";\n";
      } else if (const auto* repeated =
                     std::get_if<std::vector<std::vector<test_field_value>>>(
                         &field.content)) {
        for (const auto& instance : *repeated) {
          for (const auto& f : instance) {
            if (const auto* v = std::get_if<std::string>(&f.content)) {
              out << "  " << var << "." << fname << ".push_back("
                  << format_value_literal(*v, cpp_type) << ");\n";
            }
          }
        }
      }
    }

    void
    write_test_case(std::ostream& out, const test_vector& vec,
                    const qname& element_name, const qname& type_name,
                    const schema_set& schemas) {
      std::string type_str = qualified_type(type_name);
      std::string write_fn = "write_" + cpp_type_name(type_name);
      std::string read_fn = "read_" + cpp_type_name(type_name);
      std::string ns = cpp_ns(type_name.namespace_uri());
      if (!ns.empty()) {
        write_fn = ns + "::" + write_fn;
        read_fn = ns + "::" + read_fn;
      }
      std::string elem_ns = element_name.namespace_uri();
      std::string elem_local = element_name.local_name();

      std::string sanitized_label = vec.label;
      for (auto& c : sanitized_label) {
        if (c == '"') c = '\'';
      }

      out << "TEST_CASE("
          << escape_string_literal(elem_local + ": " + sanitized_label)
          << ", \"[auto][" << elem_local << "]\") {\n";

      // Construct the object
      out << "  " << type_str << " original;\n";
      for (const auto& field : vec.fields)
        write_field_assignment(out, "original", field, type_name, schemas);

      out << "\n";

      // Serialize
      out << "  std::ostringstream os;\n";
      out << "  {\n";
      out << "    xb::ostream_writer writer(os);\n";
      out << "    writer.start_element(xb::qname{"
          << escape_string_literal(elem_ns) << ", "
          << escape_string_literal(elem_local) << "});\n";
      out << "    writer.namespace_declaration(\"\", "
          << escape_string_literal(elem_ns) << ");\n";
      out << "    " << write_fn << "(original, writer);\n";
      out << "    writer.end_element();\n";
      out << "  }\n\n";

      // Deserialize
      out << "  xb::expat_reader reader(os.str());\n";
      out << "  reader.read();\n";
      out << "  auto parsed = " << read_fn << "(reader);\n\n";

      // Compare
      out << "  REQUIRE(original == parsed);\n";
      out << "}\n\n";
    }

  } // namespace

  test_harness_generator::test_harness_generator(const schema_set& schemas)
      : schemas_(schemas) {}

  void
  test_harness_generator::generate(const qname& element_name, std::ostream& out,
                                   const test_vector_options& opts) const {
    const auto* elem = schemas_.find_element(element_name);
    if (!elem) return;

    test_value_generator val_gen(schemas_);
    test_vector_generator vec_gen(schemas_, val_gen);
    auto vectors = vec_gen.generate(element_name, opts);
    if (vectors.empty()) return;

    // Derive include path from the element's namespace (matches codegen output)
    std::string type_header =
        ns_stem(elem->type_name().namespace_uri()) + ".hpp";
    std::string ns = cpp_ns(elem->type_name().namespace_uri());

    // Write file header
    out << "// Auto-generated by xb test-vectors\n";
    out << "// DO NOT EDIT — regenerate from schema\n\n";

    out << "#include <catch2/catch_test_macros.hpp>\n";
    out << "#include <xb/ostream_writer.hpp>\n";
    out << "#include <xb/expat_reader.hpp>\n";
    out << "#include <xb/qname.hpp>\n";
    out << "#include <sstream>\n";
    out << "#include <string>\n";

    // Include the generated type header
    if (!ns.empty()) {
      out << "#include \"" << type_header << "\"\n";
    } else {
      out << "#include \"" << type_header << "\"\n";
    }
    out << "\n";

    // Write test cases
    for (const auto& vec : vectors)
      write_test_case(out, vec, element_name, elem->type_name(), schemas_);
  }

} // namespace xb
