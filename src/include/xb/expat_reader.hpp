#pragma once

#include <xb/xml_reader.hpp>

#include <memory>
#include <string_view>

namespace xb {

  class expat_reader : public xml_reader {
  public:
    explicit expat_reader(std::string_view xml);
    ~expat_reader() override;

    expat_reader(const expat_reader&) = delete;
    expat_reader&
    operator=(const expat_reader&) = delete;
    expat_reader(expat_reader&&) noexcept;
    expat_reader&
    operator=(expat_reader&&) noexcept;

    bool
    read() override;

    xml_node_type
    node_type() const override;

    const qname&
    name() const override;

    std::size_t
    attribute_count() const override;

    const qname&
    attribute_name(std::size_t index) const override;

    std::string_view
    attribute_value(std::size_t index) const override;

    std::string_view
    attribute_value(const qname& name) const override;

    std::string_view
    text() const override;

    std::size_t
    depth() const override;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb
