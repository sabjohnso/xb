#pragma once

#include <xb/qname.hpp>

#include <string_view>

namespace xb {

  class xml_writer {
  public:
    virtual ~xml_writer() = default;

    virtual void
    start_element(const qname& name) = 0;

    virtual void
    end_element() = 0;

    virtual void
    attribute(const qname& name, std::string_view value) = 0;

    virtual void
    characters(std::string_view text) = 0;

    virtual void
    namespace_declaration(std::string_view prefix, std::string_view uri) = 0;
  };

} // namespace xb
