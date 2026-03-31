#pragma once

#include <xb/xml_reader.hpp>

#include <functional>
#include <memory>
#include <string>

namespace xb {

  /// Resolver callback: given an href, return the content as a string.
  /// Throw to signal that the resource is unavailable (triggers fallback).
  using xinclude_resolver = std::function<std::string(const std::string& href)>;

  /// A decorator over xml_reader that transparently processes XInclude
  /// (http://www.w3.org/2001/XInclude) elements.
  ///
  /// When the inner reader encounters <xi:include href="...">, the
  /// resolver is called to obtain the included content.  For parse="xml"
  /// (default), the content is parsed and its events are forwarded.  For
  /// parse="text", the content is emitted as a characters event.
  ///
  /// Circular includes are detected and reported as std::runtime_error.
  /// If inclusion fails and <xi:fallback> is present, its content is used.
  class xinclude_reader : public xml_reader {
  public:
    xinclude_reader(xml_reader& inner, xinclude_resolver resolver);
    ~xinclude_reader() override;

    xinclude_reader(const xinclude_reader&) = delete;
    xinclude_reader&
    operator=(const xinclude_reader&) = delete;

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
    attribute_value(const qname& attr_name) const override;
    std::string_view
    text() const override;
    std::size_t
    depth() const override;
    std::string_view
    namespace_uri_for_prefix(std::string_view prefix) const override;
    std::size_t
    line() const override;
    std::size_t
    column() const override;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb
