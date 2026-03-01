#pragma once

#include <string>
#include <vector>

namespace xb::schematron {

  struct assert_or_report {
    bool is_assert = true;
    std::string test;
    std::string message;
    std::string diagnostics;
  };

  struct rule {
    std::string context;
    std::vector<assert_or_report> checks;
  };

  struct pattern {
    std::string id;
    std::string name;
    std::vector<rule> rules;
  };

  struct namespace_binding {
    std::string prefix;
    std::string uri;
  };

  struct phase {
    std::string id;
    std::vector<std::string> active_patterns;
  };

  struct schema {
    std::string title;
    std::vector<namespace_binding> namespaces;
    std::vector<pattern> patterns;
    std::vector<phase> phases;
  };

} // namespace xb::schematron
