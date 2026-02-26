#pragma once

#include <nlohmann/json.hpp>

namespace xb_cli {

  int
  run(const nlohmann::json& config);

} // namespace xb_cli
