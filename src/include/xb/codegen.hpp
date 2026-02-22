#pragma once

#include <xb/cpp_code.hpp>
#include <xb/naming.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <vector>

namespace xb {

  class codegen {
    const schema_set& schemas_;
    const type_map& types_;
    codegen_options options_;

  public:
    codegen(const schema_set& schemas, const type_map& types,
            codegen_options options = {});

    std::vector<cpp_file>
    generate() const;
  };

} // namespace xb
