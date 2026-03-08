#include "json_codegen.hpp"

#include <xb/naming.hpp>

namespace xb {

  // --- Helpers ---

  static std::string
  extract_inner_type(const std::string& type, const std::string& wrapper) {
    auto prefix = wrapper + "<";
    if (type.size() <= prefix.size() + 1) return "";
    if (type.substr(0, prefix.size()) != prefix) return "";
    if (type.back() != '>') return "";
    return type.substr(prefix.size(), type.size() - prefix.size() - 1);
  }

  static bool
  starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
  }

  static std::vector<std::string>
  parse_variant_alternatives(const std::string& type) {
    auto inner = extract_inner_type(type, "std::variant");
    if (inner.empty()) return {};

    std::vector<std::string> result;
    int depth = 0;
    std::string current;
    for (char c : inner) {
      if (c == '<')
        ++depth;
      else if (c == '>')
        --depth;

      if (c == ',' && depth == 0) {
        auto pos = current.find_first_not_of(' ');
        if (pos != std::string::npos) current = current.substr(pos);
        result.push_back(current);
        current.clear();
      } else {
        current += c;
      }
    }
    auto pos = current.find_first_not_of(' ');
    if (pos != std::string::npos) current = current.substr(pos);
    if (!current.empty()) result.push_back(current);
    return result;
  }

  static std::string
  short_type_name(const std::string& type) {
    auto pos = type.rfind("::");
    if (pos != std::string::npos) return type.substr(pos + 2);
    return type;
  }

  // Emit to_json for a single struct field
  static void
  emit_to_json_field(std::string& body, const cpp_field& field) {
    std::string inner;

    if ((inner = extract_inner_type(field.type, "std::optional")) != "") {
      body += "  if (value." + field.name + ") j[\"" + field.name +
              "\"] = *value." + field.name + ";\n";
    } else if ((inner = extract_inner_type(field.type, "std::unique_ptr")) !=
               "") {
      body += "  if (value." + field.name + ") j[\"" + field.name +
              "\"] = *value." + field.name + ";\n";
    } else if (starts_with(field.type, "std::variant<")) {
      auto alts = parse_variant_alternatives(field.type);
      body += "  std::visit([&j](const auto& v) {\n";
      body += "    using T = std::decay_t<decltype(v)>;\n";
      bool first = true;
      for (const auto& alt : alts) {
        std::string kw = first ? "if" : "else if";
        body += "    " + kw + " constexpr (std::is_same_v<T, " + alt + ">) {\n";
        body += "      j[\"" + field.name + "_type\"] = \"" +
                short_type_name(alt) + "\";\n";
        body += "      j[\"" + field.name + "\"] = v;\n";
        body += "    }\n";
        first = false;
      }
      body += "  }, value." + field.name + ");\n";
    } else {
      body += "  j[\"" + field.name + "\"] = value." + field.name + ";\n";
    }
  }

  // Emit from_json for a single struct field
  static void
  emit_from_json_field(std::string& body, const cpp_field& field) {
    std::string inner;

    if ((inner = extract_inner_type(field.type, "std::optional")) != "") {
      body += "  if (j.contains(\"" + field.name + "\")) value." + field.name +
              " = j[\"" + field.name + "\"].get<" + inner + ">();\n";
    } else if ((inner = extract_inner_type(field.type, "std::unique_ptr")) !=
               "") {
      body += "  if (j.contains(\"" + field.name + "\")) value." + field.name +
              " = std::make_unique<" + inner + ">(j[\"" + field.name +
              "\"].get<" + inner + ">());\n";
    } else if (starts_with(field.type, "std::variant<")) {
      auto alts = parse_variant_alternatives(field.type);
      body += "  if (j.contains(\"" + field.name + "_type\")) {\n";
      body += "    auto type_tag = j[\"" + field.name +
              "_type\"].get<std::string>();\n";
      bool first = true;
      for (const auto& alt : alts) {
        std::string kw = first ? "if" : "else if";
        body += "    " + kw + " (type_tag == \"" + short_type_name(alt) +
                "\") value." + field.name + " = j[\"" + field.name +
                "\"].get<" + alt + ">();\n";
        first = false;
      }
      body += "  }\n";
    } else {
      body +=
          "  j.at(\"" + field.name + "\").get_to(value." + field.name + ");\n";
    }
  }

  // --- Enum JSON functions ---

  std::optional<cpp_function>
  generate_enum_to_json(const cpp_enum& e) {
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "to_json";
    fn.parameters = "nlohmann::json& j, " + e.name + " v";
    fn.body = "  j = to_string(v);\n";
    return fn;
  }

  std::optional<cpp_function>
  generate_enum_from_json(const cpp_enum& e) {
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "from_json";
    fn.parameters = "const nlohmann::json& j, " + e.name + "& v";
    fn.body = "  v = " + e.name + "_from_string(j.get<std::string>());\n";
    return fn;
  }

  // --- Union JSON functions ---

  std::optional<cpp_function>
  generate_union_to_json(const simple_type& st, const type_resolver& resolver,
                         std::set<std::string>& seen_variant_types) {
    if (st.variety() != simple_type_variety::union_type ||
        st.member_type_names().empty())
      return std::nullopt;

    auto base = st.base_type_name();
    if (!base.namespace_uri().empty() &&
        base.namespace_uri() != "http://www.w3.org/2001/XMLSchema") {
      auto* base_st = resolver.schemas.find_simple_type(base);
      if (base_st && base_st->variety() == simple_type_variety::union_type)
        return std::nullopt;
    }

    std::string variant_key;
    for (const auto& member : st.member_type_names()) {
      if (!variant_key.empty()) variant_key += ",";
      variant_key += resolver.resolve(member);
    }
    if (seen_variant_types.count(variant_key)) return std::nullopt;
    seen_variant_types.insert(variant_key);

    std::string name = resolver.type_name(st.name().local_name());
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "to_json";
    fn.parameters = "nlohmann::json& j, const " + name + "& v";
    fn.body = "  std::visit([&j](const auto& x) { j = x; }, v);\n";
    return fn;
  }

  std::optional<cpp_function>
  generate_union_from_json(const simple_type& st, const type_resolver& resolver,
                           std::set<std::string>& seen_variant_types) {
    if (st.variety() != simple_type_variety::union_type ||
        st.member_type_names().empty())
      return std::nullopt;

    auto base = st.base_type_name();
    if (!base.namespace_uri().empty() &&
        base.namespace_uri() != "http://www.w3.org/2001/XMLSchema") {
      auto* base_st = resolver.schemas.find_simple_type(base);
      if (base_st && base_st->variety() == simple_type_variety::union_type)
        return std::nullopt;
    }

    std::string variant_key;
    for (const auto& member : st.member_type_names()) {
      if (!variant_key.empty()) variant_key += ",";
      variant_key += resolver.resolve(member);
    }
    if (seen_variant_types.count(variant_key)) return std::nullopt;
    seen_variant_types.insert(variant_key);

    std::string name = resolver.type_name(st.name().local_name());
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "from_json";
    fn.parameters = "const nlohmann::json& j, " + name + "& v";

    std::string body;
    for (const auto& member : st.member_type_names()) {
      std::string cpp_type = resolver.resolve(member);
      auto* member_st = resolver.schemas.find_simple_type(member);
      bool is_enum = member_st && !member_st->facets().enumeration.empty();

      if (is_enum) {
        std::string enum_name =
            resolver.type_name(member_st->name().local_name());
        body += "  if (j.is_string()) {\n";
        body += "    try { v = " + enum_name +
                "_from_string(j.get<std::string>()); return; }\n";
        body += "    catch (...) {}\n";
        body += "  }\n";
      } else if (cpp_type == "std::string") {
        body += "  if (j.is_string()) { v = j.get<std::string>(); return; }\n";
      } else if (cpp_type == "bool") {
        body += "  if (j.is_boolean()) { v = j.get<bool>(); return; }\n";
      } else if (cpp_type == "double" || cpp_type == "float") {
        body +=
            "  if (j.is_number()) { v = j.get<" + cpp_type + ">(); return; }\n";
      } else if (cpp_type == "int32_t" || cpp_type == "int64_t" ||
                 cpp_type == "int16_t" || cpp_type == "int8_t" ||
                 cpp_type == "uint32_t" || cpp_type == "uint64_t" ||
                 cpp_type == "uint16_t" || cpp_type == "uint8_t") {
        body += "  if (j.is_number_integer()) { v = j.get<" + cpp_type +
                ">(); return; }\n";
      }
    }
    body += "  v = j.get<std::string>();\n";
    fn.body = body;
    return fn;
  }

  // --- Complex type JSON functions ---

  cpp_function
  generate_to_json_function(const cpp_struct& s) {
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "to_json";
    fn.parameters = "nlohmann::json& j, const " + s.name + "& value";

    std::string body;
    for (const auto& field : s.fields)
      emit_to_json_field(body, field);

    fn.body = body;
    return fn;
  }

  cpp_function
  generate_from_json_function(const cpp_struct& s) {
    cpp_function fn;
    fn.return_type = "void";
    fn.name = "from_json";
    fn.parameters = "const nlohmann::json& j, " + s.name + "& value";

    std::string body;
    for (const auto& field : s.fields)
      emit_from_json_field(body, field);

    fn.body = body;
    return fn;
  }

} // namespace xb
