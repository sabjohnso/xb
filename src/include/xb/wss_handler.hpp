#pragma once

#include <xb/soap_header.hpp>
#include <xb/wss.hpp>
#include <xb/wss_headers.hpp>

namespace xb::wss {

  inline void
  register_wss_handlers(soap::header_pipeline& pipeline,
                        security_header& target) {
    pipeline.add_handler(
        qname(wsse_ns, "Security"), [&target](const soap::header_block& hb) {
          target = extract_security_header(soap::envelope{{}, {hb}, {}});
          return true;
        });
  }

} // namespace xb::wss
