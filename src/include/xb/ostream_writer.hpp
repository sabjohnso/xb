#pragma once

#include <xb/xml_writer.hpp>

#include <memory>
#include <ostream>

namespace xb {

  class ostream_writer : public xml_writer {
  public:
    explicit ostream_writer(std::ostream& os);
    ~ostream_writer() override;

    ostream_writer(const ostream_writer&) = delete;
    ostream_writer&
    operator=(const ostream_writer&) = delete;
    ostream_writer(ostream_writer&&) noexcept;
    ostream_writer&
    operator=(ostream_writer&&) noexcept;

    void
    start_element(const qname& name) override;

    void
    end_element() override;

    void
    attribute(const qname& name, std::string_view value) override;

    void
    characters(std::string_view text) override;

    void
    namespace_declaration(std::string_view prefix,
                          std::string_view uri) override;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
  };

} // namespace xb
