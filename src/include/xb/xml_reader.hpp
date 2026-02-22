#pragma once

#include <xb/qname.hpp>

#include <cstddef>
#include <string_view>

namespace xb {

  enum class xml_node_type {
    start_element,
    end_element,
    characters,
  };

  class xml_reader {
  public:
    virtual ~xml_reader() = default;

    virtual bool
    read() = 0;

    virtual xml_node_type
    node_type() const = 0;

    virtual const qname&
    name() const = 0;

    virtual std::size_t
    attribute_count() const = 0;

    virtual const qname&
    attribute_name(std::size_t index) const = 0;

    virtual std::string_view
    attribute_value(std::size_t index) const = 0;

    virtual std::string_view
    attribute_value(const qname& name) const = 0;

    virtual std::string_view
    text() const = 0;

    virtual std::size_t
    depth() const = 0;

    virtual std::string_view
    namespace_uri_for_prefix(std::string_view prefix) const = 0;
  };

} // namespace xb
