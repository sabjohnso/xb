#pragma once

#include <xb/soap_header.hpp>
#include <xb/wsa.hpp>
#include <xb/wsa_headers.hpp>

namespace xb::wsa {

  inline void
  register_wsa_handlers(soap::header_pipeline& pipeline,
                        addressing_headers& target) {
    pipeline.add_handler(
        qname(wsa_ns, "To"), [&target](const soap::header_block& hb) {
          target.to =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}}).to;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "Action"), [&target](const soap::header_block& hb) {
          target.action =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}}).action;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "MessageID"), [&target](const soap::header_block& hb) {
          target.message_id =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}})
                  .message_id;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "ReplyTo"), [&target](const soap::header_block& hb) {
          target.reply_to =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}}).reply_to;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "FaultTo"), [&target](const soap::header_block& hb) {
          target.fault_to =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}}).fault_to;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "From"), [&target](const soap::header_block& hb) {
          target.from =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}}).from;
          return true;
        });

    pipeline.add_handler(
        qname(wsa_ns, "RelatesTo"), [&target](const soap::header_block& hb) {
          auto extracted =
              extract_addressing_headers(soap::envelope{{}, {hb}, {}});
          for (auto& rt : extracted.relates_to_list) {
            target.relates_to_list.push_back(std::move(rt));
          }
          return true;
        });
  }

} // namespace xb::wsa
