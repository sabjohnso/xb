#include <xb/wsa_headers.hpp>

#include <xb/any_element.hpp>

namespace xb::wsa {

  namespace {

    soap::header_block
    make_text_header(const std::string& local_name, const std::string& text) {
      soap::header_block hb;
      hb.must_understand = true;
      hb.content =
          any_element(qname(wsa_ns, local_name), {}, {std::string(text)});
      return hb;
    }

    soap::header_block
    make_epr_header(const std::string& local_name,
                    const endpoint_reference& epr) {
      any_element address(qname(wsa_ns, "Address"), {},
                          {std::string(epr.address)});

      soap::header_block hb;
      hb.must_understand = true;
      hb.content =
          any_element(qname(wsa_ns, local_name), {}, {std::move(address)});
      return hb;
    }

    std::string
    text_content(const any_element& elem) {
      std::string result;
      for (const auto& child : elem.children()) {
        if (auto* text = std::get_if<std::string>(&child)) { result += *text; }
      }
      return result;
    }

    std::optional<endpoint_reference>
    extract_epr(const any_element& elem) {
      for (const auto& child : elem.children()) {
        if (auto* addr = std::get_if<any_element>(&child)) {
          if (addr->name().namespace_uri() == wsa_ns &&
              addr->name().local_name() == "Address") {
            return endpoint_reference{text_content(*addr)};
          }
        }
      }
      return std::nullopt;
    }

  } // namespace

  void
  add_addressing_headers(soap::envelope& env, const addressing_headers& h) {
    if (h.to) { env.headers.push_back(make_text_header("To", *h.to)); }
    if (h.action) {
      env.headers.push_back(make_text_header("Action", *h.action));
    }
    if (h.message_id) {
      env.headers.push_back(make_text_header("MessageID", *h.message_id));
    }
    if (h.reply_to) {
      env.headers.push_back(make_epr_header("ReplyTo", *h.reply_to));
    }
    if (h.fault_to) {
      env.headers.push_back(make_epr_header("FaultTo", *h.fault_to));
    }
    if (h.from) { env.headers.push_back(make_epr_header("From", *h.from)); }
    for (const auto& rt : h.relates_to_list) {
      auto hb = make_text_header("RelatesTo", rt.uri);
      if (rt.relationship_type != reply_relationship) {
        // Add RelationshipType attribute for non-default relationship
        auto content = hb.content;
        std::vector<any_attribute> attrs = content.attributes();
        attrs.emplace_back(qname(wsa_ns, "RelationshipType"),
                           rt.relationship_type);
        hb.content =
            any_element(content.name(), std::move(attrs), content.children());
      }
      env.headers.push_back(std::move(hb));
    }
  }

  addressing_headers
  extract_addressing_headers(const soap::envelope& env) {
    addressing_headers result;

    for (const auto& hb : env.headers) {
      const auto& name = hb.content.name();
      if (name.namespace_uri() != wsa_ns) continue;

      const auto& local = name.local_name();
      if (local == "To") {
        result.to = text_content(hb.content);
      } else if (local == "Action") {
        result.action = text_content(hb.content);
      } else if (local == "MessageID") {
        result.message_id = text_content(hb.content);
      } else if (local == "ReplyTo") {
        result.reply_to = extract_epr(hb.content);
      } else if (local == "FaultTo") {
        result.fault_to = extract_epr(hb.content);
      } else if (local == "From") {
        result.from = extract_epr(hb.content);
      } else if (local == "RelatesTo") {
        relates_to rt;
        rt.uri = text_content(hb.content);
        // Check for RelationshipType attribute
        for (const auto& attr : hb.content.attributes()) {
          if (attr.name().local_name() == "RelationshipType") {
            rt.relationship_type = std::string(attr.value());
          }
        }
        result.relates_to_list.push_back(std::move(rt));
      }
    }

    return result;
  }

} // namespace xb::wsa
