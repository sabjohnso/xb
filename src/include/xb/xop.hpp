#pragma once

#include <xb/mime_multipart.hpp>
#include <xb/soap_model.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace xb::xop {

  inline constexpr auto xop_ns = "http://www.w3.org/2004/08/xop/include";

  struct attachment {
    std::string content_id;
    std::string content_type;
    std::vector<std::byte> data;

    bool
    operator==(const attachment&) const = default;
  };

  struct mtom_message {
    soap::envelope envelope;
    std::vector<attachment> attachments;

    bool
    operator==(const mtom_message&) const = default;
  };

  // Replace large base64 text with <xop:Include href="cid:..."/> + attachment
  mtom_message
  optimize(const soap::envelope& env, std::size_t threshold = 1024);

  // Replace <xop:Include> with inline base64 from matching attachment
  soap::envelope
  deoptimize(const mtom_message& msg);

  // Convert to/from MIME multipart (first part = XML envelope)
  mime::multipart_message
  to_multipart(const mtom_message& msg);

  mtom_message
  from_multipart(const mime::multipart_message& mp);

} // namespace xb::xop
