#include <xb/soap_fault.hpp>

#include "soap_common.hpp"

#include <xb/xml_io.hpp>

#include <stdexcept>

namespace xb::soap {

  namespace {

    using detail::is_soap_element;
    using detail::namespace_for;
    using detail::opt_attr;
    using detail::read_skip_ws;
    using detail::read_text_content;
    using detail::soap_prefix;

    // Parse the first child element from a container (detail/Detail).
    // Consumes all remaining children and the container's end element.
    std::optional<any_element>
    read_detail_content(xml_reader& reader) {
      std::optional<any_element> result;
      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return result;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (!result) {
            result = any_element(reader);
          } else {
            skip_element(reader);
          }
        }
      }
      return result;
    }

    // --- SOAP 1.1 Fault parsing ---

    fault_1_1
    read_fault_1_1(xml_reader& reader) {
      fault_1_1 f;
      auto fault_depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == fault_depth) {
          return f;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          auto local = reader.name().local_name();
          if (local == "faultcode") {
            f.fault_code = read_text_content(reader);
          } else if (local == "faultstring") {
            f.fault_string = read_text_content(reader);
          } else if (local == "faultactor") {
            f.fault_actor = read_text_content(reader);
          } else if (local == "detail") {
            f.detail = read_detail_content(reader);
          } else {
            skip_element(reader);
          }
        }
      }
      return f;
    }

    // --- SOAP 1.2 Fault parsing ---

    fault_subcode
    read_subcode(xml_reader& reader, const std::string& soap_ns) {
      fault_subcode sc;
      auto depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return sc;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_soap_element(reader.name(), soap_ns, "Value")) {
            sc.value = read_text_content(reader);
          } else if (is_soap_element(reader.name(), soap_ns, "Subcode")) {
            sc.subcode =
                std::make_unique<fault_subcode>(read_subcode(reader, soap_ns));
          } else {
            skip_element(reader);
          }
        }
      }
      return sc;
    }

    fault_code
    read_code(xml_reader& reader, const std::string& soap_ns) {
      fault_code fc;
      auto depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return fc;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_soap_element(reader.name(), soap_ns, "Value")) {
            fc.value = read_text_content(reader);
          } else if (is_soap_element(reader.name(), soap_ns, "Subcode")) {
            fc.subcode = read_subcode(reader, soap_ns);
          } else {
            skip_element(reader);
          }
        }
      }
      return fc;
    }

    const qname xml_lang_attr{"http://www.w3.org/XML/1998/namespace", "lang"};

    fault_1_2
    read_fault_1_2(xml_reader& reader, const std::string& soap_ns) {
      fault_1_2 f;
      auto fault_depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == fault_depth) {
          return f;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_soap_element(reader.name(), soap_ns, "Code")) {
            f.code = read_code(reader, soap_ns);
          } else if (is_soap_element(reader.name(), soap_ns, "Reason")) {
            auto reason_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == reason_depth) {
                break;
              }
              if (reader.node_type() == xml_node_type::start_element &&
                  is_soap_element(reader.name(), soap_ns, "Text")) {
                fault_reason_text rt;
                rt.lang = opt_attr(reader, xml_lang_attr);
                rt.text = read_text_content(reader);
                f.reason.push_back(std::move(rt));
              } else if (reader.node_type() == xml_node_type::start_element) {
                skip_element(reader);
              }
            }
          } else if (is_soap_element(reader.name(), soap_ns, "Node")) {
            f.node = read_text_content(reader);
          } else if (is_soap_element(reader.name(), soap_ns, "Role")) {
            f.role = read_text_content(reader);
          } else if (is_soap_element(reader.name(), soap_ns, "Detail")) {
            f.detail = read_detail_content(reader);
          } else {
            skip_element(reader);
          }
        }
      }
      return f;
    }

    // --- SOAP 1.1 Fault writing ---

    void
    write_fault_1_1(xml_writer& writer, const fault_1_1& f,
                    const std::string& soap_ns) {
      writer.start_element(qname(soap_ns, "Fault"));
      writer.namespace_declaration(soap_prefix, soap_ns);

      writer.start_element(qname("", "faultcode"));
      writer.characters(f.fault_code);
      writer.end_element();

      writer.start_element(qname("", "faultstring"));
      writer.characters(f.fault_string);
      writer.end_element();

      if (!f.fault_actor.empty()) {
        writer.start_element(qname("", "faultactor"));
        writer.characters(f.fault_actor);
        writer.end_element();
      }

      if (f.detail.has_value()) {
        writer.start_element(qname("", "detail"));
        f.detail->write(writer);
        writer.end_element();
      }

      writer.end_element(); // Fault
    }

    // --- SOAP 1.2 Fault writing ---

    void
    write_subcode(xml_writer& writer, const fault_subcode& sc,
                  const std::string& soap_ns) {
      writer.start_element(qname(soap_ns, "Subcode"));

      writer.start_element(qname(soap_ns, "Value"));
      writer.characters(sc.value);
      writer.end_element();

      if (sc.subcode) { write_subcode(writer, *sc.subcode, soap_ns); }

      writer.end_element(); // Subcode
    }

    void
    write_fault_1_2(xml_writer& writer, const fault_1_2& f,
                    const std::string& soap_ns) {
      writer.start_element(qname(soap_ns, "Fault"));
      writer.namespace_declaration(soap_prefix, soap_ns);
      writer.namespace_declaration("xml",
                                   "http://www.w3.org/XML/1998/namespace");

      // Code
      writer.start_element(qname(soap_ns, "Code"));
      writer.start_element(qname(soap_ns, "Value"));
      writer.characters(f.code.value);
      writer.end_element(); // Value
      if (f.code.subcode.has_value()) {
        write_subcode(writer, *f.code.subcode, soap_ns);
      }
      writer.end_element(); // Code

      // Reason
      writer.start_element(qname(soap_ns, "Reason"));
      for (const auto& rt : f.reason) {
        writer.start_element(qname(soap_ns, "Text"));
        writer.attribute(qname("http://www.w3.org/XML/1998/namespace", "lang"),
                         rt.lang);
        writer.characters(rt.text);
        writer.end_element(); // Text
      }
      writer.end_element(); // Reason

      // Node (optional)
      if (!f.node.empty()) {
        writer.start_element(qname(soap_ns, "Node"));
        writer.characters(f.node);
        writer.end_element();
      }

      // Role (optional)
      if (!f.role.empty()) {
        writer.start_element(qname(soap_ns, "Role"));
        writer.characters(f.role);
        writer.end_element();
      }

      // Detail (optional)
      if (f.detail.has_value()) {
        writer.start_element(qname(soap_ns, "Detail"));
        f.detail->write(writer);
        writer.end_element();
      }

      writer.end_element(); // Fault
    }

  } // namespace

  fault
  read_fault(xml_reader& reader, soap_version version) {
    const auto& soap_ns = namespace_for(version);
    if (!is_soap_element(reader.name(), soap_ns, "Fault")) {
      throw std::runtime_error("read_fault: expected <Fault> element, got <" +
                               reader.name().local_name() + ">");
    }

    if (version == soap_version::v1_1) { return read_fault_1_1(reader); }
    return read_fault_1_2(reader, soap_ns);
  }

  void
  write_fault(xml_writer& writer, const fault& f, soap_version version) {
    bool is_1_1 = std::holds_alternative<fault_1_1>(f);
    if (version == soap_version::v1_1 && !is_1_1) {
      throw std::runtime_error(
          "write_fault: version is 1.1 but fault variant holds fault_1_2");
    }
    if (version == soap_version::v1_2 && is_1_1) {
      throw std::runtime_error(
          "write_fault: version is 1.2 but fault variant holds fault_1_1");
    }

    const auto& soap_ns = namespace_for(version);

    std::visit(
        [&](const auto& fv) {
          using T = std::decay_t<decltype(fv)>;
          if constexpr (std::is_same_v<T, fault_1_1>) {
            write_fault_1_1(writer, fv, soap_ns);
          } else {
            write_fault_1_2(writer, fv, soap_ns);
          }
        },
        f);
  }

  bool
  is_fault(const any_element& body_child, soap_version version) {
    const auto& soap_ns = namespace_for(version);
    return body_child.name().namespace_uri() == soap_ns &&
           body_child.name().local_name() == "Fault";
  }

} // namespace xb::soap
