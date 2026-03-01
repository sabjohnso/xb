#pragma once

#include <xb/schema_set.hpp>
#include <xb/schematron_model.hpp>

#include <string>
#include <vector>

namespace xb {

  struct overlay_result {
    int rules_matched = 0;
    int rules_unmatched = 0;
    std::vector<std::string> warnings;
  };

  overlay_result
  schematron_overlay(schema_set& schemas, const schematron::schema& sch);

} // namespace xb
