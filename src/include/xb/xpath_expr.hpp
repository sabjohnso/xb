#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xb {

  struct xpath_context {
    std::string value_prefix; // "value." for complex, "value" for simple
  };

  std::optional<std::string>
  translate_xpath_assertion(std::string_view xpath, const xpath_context& ctx);

} // namespace xb
