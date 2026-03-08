#include <xb/naming.hpp>

#include <cctype>
#include <cstdio>
#include <regex>
#include <unordered_set>
#include <vector>

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

  namespace {

    // Split a name into words at underscores and case boundaries.
    // "order_type" -> ["order", "type"]
    // "OrderType"  -> ["Order", "Type"]
    // "HTMLParser" -> ["HTML", "Parser"]
    std::vector<std::string>
    split_words(std::string_view name) {
      // First normalize to snake_case, which handles all boundary detection
      std::string snake = to_snake_case(name);
      std::vector<std::string> words;
      std::string word;
      for (char c : snake) {
        if (c == '_') {
          if (!word.empty()) {
            words.push_back(std::move(word));
            word.clear();
          }
        } else {
          word += c;
        }
      }
      if (!word.empty()) words.push_back(std::move(word));
      return words;
    }

    char
    to_upper(char c) {
      if (is_lower(c)) return static_cast<char>(c - 'a' + 'A');
      return c;
    }

    std::string
    capitalize(const std::string& word) {
      if (word.empty()) return word;
      std::string result = word;
      result[0] = to_upper(result[0]);
      return result;
    }

  } // namespace

  std::string
  to_pascal_case(std::string_view name) {
    auto words = split_words(name);
    std::string result;
    for (const auto& w : words)
      result += capitalize(w);
    return result;
  }

  std::string
  to_camel_case(std::string_view name) {
    auto words = split_words(name);
    std::string result;
    for (std::size_t i = 0; i < words.size(); ++i) {
      if (i == 0)
        result += words[i]; // first word stays lowercase
      else
        result += capitalize(words[i]);
    }
    return result;
  }

  std::string
  to_upper_snake_case(std::string_view name) {
    std::string snake = to_snake_case(name);
    for (char& c : snake)
      c = to_upper(c);
    return snake;
  }

  namespace {

    bool
    is_identifier_char(char c) {
      return is_lower(c) || is_upper(c) || is_digit(c) || c == '_';
    }

    // Sanitize a string to be a valid C++ identifier, without changing case.
    // Replaces non-identifier characters, prefixes digits, escapes keywords.
    std::string
    sanitize_cpp_identifier(std::string_view input,
                            std::string_view original_for_fallback) {
      std::string result;
      result.reserve(input.size());
      for (char c : input) {
        if (is_identifier_char(c)) {
          result += c;
        } else {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "_x%02x",
                        static_cast<unsigned char>(c));
          result += buf;
        }
      }

      if (result.empty()) {
        for (char c : original_for_fallback) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "_x%02x",
                        static_cast<unsigned char>(c));
          result += buf;
        }
      }

      if (!result.empty() && is_digit(result[0]))
        result.insert(result.begin(), '_');

      if (cpp_keywords().count(result)) result += '_';

      return result;
    }

  } // namespace

  std::string
  apply_naming_style(std::string_view name, naming_style style) {
    switch (style) {
      case naming_style::snake_case:
        return to_snake_case(name);
      case naming_style::pascal_case:
        return to_pascal_case(name);
      case naming_style::camel_case:
        return to_camel_case(name);
      case naming_style::upper_snake:
        return to_upper_snake_case(name);
      case naming_style::original:
        return std::string(name);
    }
    return std::string(name);
  }

  std::string
  apply_naming(std::string_view name, naming_category category,
               const naming_options& opts) {
    naming_style style{};
    const std::vector<std::pair<std::string, std::string>>* rules = nullptr;

    switch (category) {
      case naming_category::type_:
        style = opts.type_style;
        rules = &opts.type_rules;
        break;
      case naming_category::field:
        style = opts.field_style;
        rules = &opts.field_rules;
        break;
      case naming_category::enum_value:
        style = opts.enum_style;
        break;
      case naming_category::function:
        style = opts.function_style;
        break;
    }

    std::string result = apply_naming_style(name, style);

    // Apply regex rules if any
    if (rules) {
      for (const auto& [pattern, replacement] : *rules) {
        std::regex re(pattern);
        result = std::regex_replace(result, re, replacement);
      }
    }

    // Final sanitization: ensure valid C++ identifier (without re-casing)
    return sanitize_cpp_identifier(result, name);
  }

  std::string
  to_cpp_identifier(std::string_view xsd_name) {
    std::string snake = to_snake_case(xsd_name);
    return sanitize_cpp_identifier(snake, xsd_name);
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

  namespace {

    // Split a namespace URI into segments (by /, :, .)
    std::vector<std::string>
    uri_segments(std::string_view uri) {
      // Strip scheme
      if (auto pos = uri.find("://"); pos != std::string_view::npos)
        uri = uri.substr(pos + 3);
      else if (uri.starts_with("urn:"))
        uri = uri.substr(4);

      // Strip leading "www."
      if (uri.starts_with("www.")) uri = uri.substr(4);

      std::vector<std::string> segments;
      std::string segment;
      for (char c : uri) {
        if (c == '/' || c == ':' || c == '.') {
          if (!segment.empty()) {
            segments.push_back(std::move(segment));
            segment.clear();
          }
        } else {
          segment += c;
        }
      }
      if (!segment.empty()) segments.push_back(std::move(segment));
      return segments;
    }

  } // namespace

  std::string
  default_namespace_for(const std::string& xml_namespace,
                        const std::set<std::string>& all_namespaces,
                        const codegen_options& opts) {
    if (xml_namespace.empty()) return {};

    // Check explicit mapping first
    auto it = opts.namespace_map.find(xml_namespace);
    if (it != opts.namespace_map.end()) return it->second;

    auto segments = uri_segments(xml_namespace);
    if (segments.empty()) return {};

    // Start with the last segment
    std::string candidate = to_cpp_identifier(segments.back());

    // Check for collisions with other namespaces
    bool has_collision = false;
    for (const auto& other_ns : all_namespaces) {
      if (other_ns == xml_namespace) continue;
      auto other_segs = uri_segments(other_ns);
      if (!other_segs.empty() &&
          to_cpp_identifier(other_segs.back()) == candidate) {
        has_collision = true;
        break;
      }
    }

    if (has_collision) {
      // Progressively prepend parent segments until unique
      for (int i = static_cast<int>(segments.size()) - 2; i >= 0; --i) {
        candidate = to_cpp_identifier(segments[i]) + "::" + candidate;

        bool still_collides = false;
        for (const auto& other_ns : all_namespaces) {
          if (other_ns == xml_namespace) continue;
          auto other_segs = uri_segments(other_ns);

          // Build the same depth candidate for the other namespace
          int depth = static_cast<int>(segments.size()) - i;
          if (static_cast<int>(other_segs.size()) >= depth) {
            std::string other_candidate;
            for (int j = static_cast<int>(other_segs.size()) - depth;
                 j < static_cast<int>(other_segs.size()); ++j) {
              if (!other_candidate.empty()) other_candidate += "::";
              other_candidate += to_cpp_identifier(other_segs[j]);
            }
            if (other_candidate == candidate) {
              still_collides = true;
              break;
            }
          }
        }
        if (!still_collides) break;
      }
    }

    return candidate;
  }

} // namespace xb
