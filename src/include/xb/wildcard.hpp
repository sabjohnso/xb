#pragma once

#include <xb/schema_fwd.hpp>

#include <string>
#include <vector>

namespace xb {

  struct wildcard {
    wildcard_ns_constraint ns_constraint = wildcard_ns_constraint::any;
    std::vector<std::string> namespaces;
    process_contents process = process_contents::strict;

    bool
    operator==(const wildcard&) const = default;
  };

} // namespace xb
