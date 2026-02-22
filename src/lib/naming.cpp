#include <xb/naming.hpp>

#include <cctype>
#include <unordered_set>

namespace xb {

  namespace {

    bool
    is_upper(char c) {
      return c >= 'A' && c <= 'Z';
    }

    bool
    is_lower(char c) {
      return c >= 'a' && c <= 'z';
    }

    bool
    is_digit(char c) {
      return c >= '0' && c <= '9';
    }

    char
    to_lower(char c) {
      if (is_upper(c)) return static_cast<char>(c - 'A' + 'a');
      return c;
    }

    const std::unordered_set<std::string>&
    cpp_keywords() {
      static const std::unordered_set<std::string> keywords = {
          "alignas",       "alignof",     "and",
          "and_eq",        "asm",         "auto",
          "bitand",        "bitor",       "bool",
          "break",         "case",        "catch",
          "char",          "char8_t",     "char16_t",
          "char32_t",      "class",       "compl",
          "concept",       "const",       "consteval",
          "constexpr",     "constinit",   "const_cast",
          "continue",      "co_await",    "co_return",
          "co_yield",      "decltype",    "default",
          "delete",        "do",          "double",
          "dynamic_cast",  "else",        "enum",
          "explicit",      "export",      "extern",
          "false",         "float",       "for",
          "friend",        "goto",        "if",
          "inline",        "int",         "long",
          "mutable",       "namespace",   "new",
          "noexcept",      "not",         "not_eq",
          "nullptr",       "operator",    "or",
          "or_eq",         "private",     "protected",
          "public",        "register",    "reinterpret_cast",
          "requires",      "return",      "short",
          "signed",        "sizeof",      "static",
          "static_assert", "static_cast", "struct",
          "switch",        "template",    "this",
          "thread_local",  "throw",       "true",
          "try",           "typedef",     "typeid",
          "typename",      "union",       "unsigned",
          "using",         "virtual",     "void",
          "volatile",      "wchar_t",     "while",
          "xor",           "xor_eq",
      };
      return keywords;
    }

  } // namespace

  std::string
  to_snake_case(std::string_view name) {
    if (name.empty()) return {};

    std::string result;
    result.reserve(name.size() + 4);

    for (std::size_t i = 0; i < name.size(); ++i) {
      char c = name[i];

      // Replace hyphens and dots with underscores
      if (c == '-' || c == '.') {
        result += '_';
        continue;
      }

      if (is_upper(c)) {
        // Insert underscore before:
        // - an uppercase letter preceded by a lowercase letter (camelCase)
        // - an uppercase letter that starts a new word after an abbreviation
        //   run (e.g. the 'P' in "HTMLParser")
        if (!result.empty() && result.back() != '_') {
          bool prev_lower = is_lower(name[i - 1]);
          bool prev_upper = is_upper(name[i - 1]);
          bool next_lower = (i + 1 < name.size()) && is_lower(name[i + 1]);

          if (prev_lower || (prev_upper && next_lower)) result += '_';
        }
        result += to_lower(c);
      } else {
        result += c;
      }
    }

    return result;
  }

  std::string
  to_cpp_identifier(std::string_view xsd_name) {
    std::string result = to_snake_case(xsd_name);

    // Prefix with underscore if starts with a digit
    if (!result.empty() && is_digit(result[0]))
      result.insert(result.begin(), '_');

    // Append underscore if it's a C++ keyword
    if (cpp_keywords().count(result)) result += '_';

    return result;
  }

  std::string
  cpp_namespace_for(const std::string& xml_namespace,
                    const codegen_options& opts) {
    if (xml_namespace.empty()) return {};

    // Check explicit mapping first
    auto it = opts.namespace_map.find(xml_namespace);
    if (it != opts.namespace_map.end()) return it->second;

    // Auto-derive from URI
    std::string_view uri = xml_namespace;

    // Strip scheme (http://, https://, urn:)
    if (auto pos = uri.find("://"); pos != std::string_view::npos)
      uri = uri.substr(pos + 3);
    else if (uri.starts_with("urn:"))
      uri = uri.substr(4);

    // Strip leading "www."
    if (uri.starts_with("www.")) uri = uri.substr(4);

    // Split on / and : separators, convert each segment
    std::string result;
    std::string segment;

    for (char c : uri) {
      if (c == '/' || c == ':') {
        if (!segment.empty()) {
          if (!result.empty()) result += "::";
          result += to_snake_case(segment);
          segment.clear();
        }
      } else if (c == '.') {
        // Dots within a host part become separators
        if (!segment.empty()) {
          if (!result.empty()) result += "::";
          result += to_snake_case(segment);
          segment.clear();
        }
      } else {
        segment += c;
      }
    }

    if (!segment.empty()) {
      if (!result.empty()) result += "::";
      result += to_snake_case(segment);
    }

    return result;
  }

} // namespace xb
