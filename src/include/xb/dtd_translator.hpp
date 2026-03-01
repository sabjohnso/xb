#pragma once

#include <xb/dtd_model.hpp>
#include <xb/schema_set.hpp>

namespace xb {

  schema_set
  dtd_translate(const dtd::document& doc);

} // namespace xb
