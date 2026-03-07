#pragma once

#include <xb/dtd_model.hpp>
#include <xb/schema_set.hpp>

namespace xb {

  dtd::document
  schema_to_dtd(const schema_set& schemas);

} // namespace xb
