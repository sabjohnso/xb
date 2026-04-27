#pragma once

#include <xb/xml_reader.hpp>

#include <cstddef>
#include <memory>
#include <string_view>

namespace xb {

  /// Configuration knobs for @ref expat_reader.
  ///
  /// Defaults are chosen to be safe for parsing untrusted XML:
  /// external entity references are rejected, parameter entities are not
  /// parsed, and runaway entity expansion (billion-laughs) is bounded.
  /// Callers that intentionally need to process documents with external
  /// entities (e.g. specific legacy schemas) opt in via
  /// @ref allow_external_entities.
  struct expat_reader_options {
    /// When @c true, external entity references (SYSTEM / PUBLIC,
    /// external parameter entities, foreign DTD subsets) are not actively
    /// rejected. The expat default behaviour applies — namely, the
    /// reference is silently skipped unless the caller has installed a
    /// resolver via expat's lower-level API.
    ///
    /// When @c false (the default), an external entity reference causes
    /// the parse to fail.
    ///
    /// Internal entity declarations in the document's internal DTD subset
    /// are honoured in either mode.
    bool allow_external_entities = false;

    /// Maximum permitted ratio of expanded-output bytes to input bytes
    /// during entity expansion (XML billion-laughs / quadratic-blowup
    /// mitigation). The check is only enforced once the expanded output
    /// exceeds @ref billion_laughs_activation_threshold bytes.
    double max_billion_laughs_amplification = 100.0;

    /// Output-byte threshold below which the amplification check is not
    /// applied. Avoids false positives for small documents that
    /// legitimately use entities.
    std::size_t billion_laughs_activation_threshold = 1024UL * 1024UL;
  };

  class expat_reader : public xml_reader {
  public:
    /// Construct a reader for @p xml with the default (hardened) options.
    explicit expat_reader(std::string_view xml);

    /// Construct a reader for @p xml with caller-supplied options.
    expat_reader(std::string_view xml, const expat_reader_options& options);

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
