#pragma once

#include <xb/qname.hpp>

#include <optional>
#include <string>

namespace xb {

  struct type_alternative {
    std::optional<std::string> test; // XPath expression (nullopt = default)
    qname type_name;

    bool
    operator==(const type_alternative&) const = default;
  };

} // namespace xb
