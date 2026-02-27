#pragma once

#include <xb/qname.hpp>
#include <xb/schema_fwd.hpp>

#include <string>
#include <vector>

namespace xb {

  struct wildcard {
    wildcard_ns_constraint ns_constraint = wildcard_ns_constraint::any;
    std::vector<std::string> namespaces;
    process_contents process = process_contents::strict;
    std::vector<qname> except_names;
    std::vector<std::string> except_namespaces;

    bool
    operator==(const wildcard&) const = default;
  };

} // namespace xb
