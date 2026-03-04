#include <xb/xop.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/xml_value.hpp>

#include <sstream>

namespace xb::xop {

  namespace {

    std::string
    generate_content_id() {
      static int counter = 0;
      return "part-" + std::to_string(++counter) + "@xb.generated";
    }

    bool
    is_xop_include(const any_element& elem) {
      return elem.name().namespace_uri() == xop_ns &&
             elem.name().local_name() == "Include";
    }

    std::string
    get_xop_href(const any_element& elem) {
      for (const auto& attr : elem.attributes()) {
        if (attr.name().local_name() == "href") {
          auto href = std::string(attr.value());
          // Strip "cid:" prefix
          if (href.size() > 4 && href.substr(0, 4) == "cid:") {
            return href.substr(4);
          }
          return href;
        }
      }
      return {};
    }

    // Walk any_element tree, replacing large base64 text with xop:Include
    any_element
    optimize_element(const any_element& elem, std::size_t threshold,
                     std::vector<attachment>& attachments) {
      std::vector<any_element::child> new_children;

      for (const auto& child : elem.children()) {
        if (auto* sub_elem = std::get_if<any_element>(&child)) {
          new_children.push_back(
              optimize_element(*sub_elem, threshold, attachments));
        } else if (auto* text = std::get_if<std::string>(&child)) {
          // Try to detect base64 content: only if it looks like base64
          // and exceeds threshold
          if (text->size() >= threshold) {
            // Try to parse as base64 to verify it's valid
            try {
              auto bytes = parse_base64_binary(*text);
              if (!bytes.empty()) {
                auto cid = generate_content_id();

                attachment att;
                att.content_id = cid;
                att.content_type = "application/octet-stream";
                att.data = std::move(bytes);
                attachments.push_back(std::move(att));

                // Create xop:Include element
                std::vector<any_attribute> attrs;
                attrs.emplace_back(qname("", "href"), "cid:" + cid);
                any_element include(qname(xop_ns, "Include"), std::move(attrs),
                                    {});
                new_children.push_back(std::move(include));
                continue;
              }
            } catch (...) {
              // Not valid base64, keep as text
            }
          }
          new_children.push_back(std::string(*text));
        }
      }

      return any_element(elem.name(), elem.attributes(),
                         std::move(new_children));
    }

    // Walk any_element tree, replacing xop:Include with base64 content
    any_element
    deoptimize_element(const any_element& elem,
                       const std::vector<attachment>& attachments) {
      std::vector<any_element::child> new_children;

      for (const auto& child : elem.children()) {
        if (auto* sub_elem = std::get_if<any_element>(&child)) {
          if (is_xop_include(*sub_elem)) {
            auto cid = get_xop_href(*sub_elem);
            for (const auto& att : attachments) {
              if (att.content_id == cid) {
                new_children.push_back(format_base64_binary(att.data));
                break;
              }
            }
          } else {
            new_children.push_back(deoptimize_element(*sub_elem, attachments));
          }
        } else {
          new_children.push_back(child);
        }
      }

      return any_element(elem.name(), elem.attributes(),
                         std::move(new_children));
    }

    std::string
    serialize_envelope(const soap::envelope& env) {
      std::ostringstream os;
      ostream_writer writer(os);
      soap::write_envelope(writer, env);
      return os.str();
    }

    soap::envelope
    parse_envelope(const std::string& xml) {
      expat_reader reader(xml);
      reader.read();
      return soap::read_envelope(reader);
    }

  } // namespace

  mtom_message
  optimize(const soap::envelope& env, std::size_t threshold) {
    mtom_message result;
    result.envelope.version = env.version;

    // Optimize headers
    for (const auto& hb : env.headers) {
      soap::header_block new_hb;
      new_hb.must_understand = hb.must_understand;
      new_hb.role = hb.role;
      new_hb.content =
          optimize_element(hb.content, threshold, result.attachments);
      result.envelope.headers.push_back(std::move(new_hb));
    }

    // Optimize body elements
    for (const auto& body_elem : env.body) {
      result.envelope.body.push_back(
          optimize_element(body_elem, threshold, result.attachments));
    }

    return result;
  }

  soap::envelope
  deoptimize(const mtom_message& msg) {
    soap::envelope result;
    result.version = msg.envelope.version;

    for (const auto& hb : msg.envelope.headers) {
      soap::header_block new_hb;
      new_hb.must_understand = hb.must_understand;
      new_hb.role = hb.role;
      new_hb.content = deoptimize_element(hb.content, msg.attachments);
      result.headers.push_back(std::move(new_hb));
    }

    for (const auto& body_elem : msg.envelope.body) {
      result.body.push_back(deoptimize_element(body_elem, msg.attachments));
    }

    return result;
  }

  mime::multipart_message
  to_multipart(const mtom_message& msg) {
    mime::multipart_message mp;
    mp.boundary = mime::generate_boundary();

    // First part: serialized SOAP envelope
    auto xml = serialize_envelope(msg.envelope);
    std::vector<std::byte> xml_bytes;
    for (char c : xml) {
      xml_bytes.push_back(static_cast<std::byte>(c));
    }

    mime::mime_part root;
    root.content_type = "application/xop+xml; charset=utf-8; "
                        "type=\"application/soap+xml\"";
    root.content_id = "root@xb.generated";
    root.content_transfer_encoding = "8bit";
    root.body = std::move(xml_bytes);
    mp.parts.push_back(std::move(root));

    // Attachment parts
    for (const auto& att : msg.attachments) {
      mime::mime_part part;
      part.content_type = att.content_type;
      part.content_id = att.content_id;
      part.content_transfer_encoding = "binary";
      part.body = att.data;
      mp.parts.push_back(std::move(part));
    }

    return mp;
  }

  mtom_message
  from_multipart(const mime::multipart_message& mp) {
    mtom_message result;

    if (mp.parts.empty()) return result;

    // First part is the SOAP envelope
    const auto& root = mp.parts[0];
    std::string xml;
    for (auto b : root.body) {
      xml.push_back(static_cast<char>(b));
    }
    result.envelope = parse_envelope(xml);

    // Remaining parts are attachments
    for (std::size_t i = 1; i < mp.parts.size(); ++i) {
      const auto& part = mp.parts[i];
      attachment att;
      att.content_id = part.content_id;
      att.content_type = part.content_type;
      att.data = part.body;
      result.attachments.push_back(std::move(att));
    }

    return result;
  }

} // namespace xb::xop
