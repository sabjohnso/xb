#include <xb/soap_envelope.hpp>

#include "soap_common.hpp"

#include <xb/xml_io.hpp>

#include <stdexcept>

namespace xb::soap {

  namespace {

    using detail::is_soap_element;
    using detail::namespace_for;
    using detail::opt_attr;
    using detail::read_skip_ws;
    using detail::soap_1_1_ns;
    using detail::soap_1_2_ns;
    using detail::soap_prefix;

    soap_version
    detect_version(const std::string& ns) {
      if (ns == soap_1_1_ns) return soap_version::v1_1;
      if (ns == soap_1_2_ns) return soap_version::v1_2;
      throw std::runtime_error("read_envelope: unknown SOAP namespace '" + ns +
                               "'");
    }

    header_block
    parse_header_block(xml_reader& reader, const std::string& soap_ns) {
      header_block hb;

      std::string mu = opt_attr(reader, qname(soap_ns, "mustUnderstand"));
      hb.must_understand = (mu == "1" || mu == "true");

      if (soap_ns == soap_1_2_ns) {
        hb.role = opt_attr(reader, qname(soap_ns, "role"));
      } else {
        hb.role = opt_attr(reader, qname(soap_ns, "actor"));
      }

      any_element raw(reader);

      std::vector<any_attribute> filtered_attrs;
      for (const auto& attr : raw.attributes()) {
        if (attr.name().namespace_uri() != soap_ns) {
          filtered_attrs.push_back(attr);
        }
      }

      hb.content =
          any_element(raw.name(), std::move(filtered_attrs), raw.children());
      return hb;
    }

    void
    parse_header(xml_reader& reader, const std::string& soap_ns,
                 envelope& env) {
      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          env.headers.push_back(parse_header_block(reader, soap_ns));
        }
      }
    }

    void
    parse_body(xml_reader& reader, envelope& env) {
      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          env.body.emplace_back(reader);
        }
      }
    }

    // Build a merged any_element with SOAP attributes injected, then
    // delegate to any_element::write() for proper namespace management.
    void
    write_header_block(xml_writer& writer, const header_block& hb,
                       const std::string& soap_ns) {
      std::vector<any_attribute> attrs;
      if (hb.must_understand) {
        attrs.emplace_back(qname(soap_ns, "mustUnderstand"), "1");
      }
      if (!hb.role.empty()) {
        if (soap_ns == soap_1_2_ns) {
          attrs.emplace_back(qname(soap_ns, "role"), hb.role);
        } else {
          attrs.emplace_back(qname(soap_ns, "actor"), hb.role);
        }
      }
      for (const auto& attr : hb.content.attributes()) {
        attrs.push_back(attr);
      }

      any_element merged(hb.content.name(), std::move(attrs),
                         hb.content.children());
      merged.write(writer);
    }

  } // namespace

  envelope
  read_envelope(xml_reader& reader) {
    envelope env;
    std::string soap_ns = reader.name().namespace_uri();
    env.version = detect_version(soap_ns);

    if (reader.name().local_name() != "Envelope") {
      throw std::runtime_error("read_envelope: expected <Envelope> element");
    }

    auto root_depth = reader.depth();
    while (read_skip_ws(reader)) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == root_depth) {
        return env;
      }
      if (reader.node_type() == xml_node_type::start_element) {
        if (is_soap_element(reader.name(), soap_ns, "Header")) {
          parse_header(reader, soap_ns, env);
        } else if (is_soap_element(reader.name(), soap_ns, "Body")) {
          parse_body(reader, env);
        } else {
          skip_element(reader);
        }
      }
    }
    throw std::runtime_error("read_envelope: unexpected end of input");
  }

  void
  write_envelope(xml_writer& writer, const envelope& env) {
    const auto& soap_ns = namespace_for(env.version);

    qname envelope_name(soap_ns, "Envelope");
    qname header_name(soap_ns, "Header");
    qname body_name(soap_ns, "Body");

    writer.start_element(envelope_name);
    writer.namespace_declaration(soap_prefix, soap_ns);

    if (!env.headers.empty()) {
      writer.start_element(header_name);
      for (const auto& hb : env.headers) {
        write_header_block(writer, hb, soap_ns);
      }
      writer.end_element(); // Header
    }

    writer.start_element(body_name);
    for (const auto& child : env.body) {
      child.write(writer);
    }
    writer.end_element(); // Body

    writer.end_element(); // Envelope
  }

} // namespace xb::soap
