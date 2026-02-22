#pragma once

#include <xb/xml_reader.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace xb {

  struct type_mapping {
    std::string cpp_type;
    std::string cpp_header;
  };

  class type_map {
    std::unordered_map<std::string, type_mapping> entries_;

  public:
    type_map() = default;

    static type_map
    defaults();

    static type_map
    load(xml_reader& reader);

    void
    merge(const type_map& overrides);

    const type_mapping*
    find(const std::string& xsd_type) const;

    void
    set(std::string xsd_type, type_mapping mapping);

    std::size_t
    size() const;

    bool
    contains(const std::string& xsd_type) const;
  };

} // namespace xb
