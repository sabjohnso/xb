#include <xb/wsdl2_parser.hpp>

#include <xb/schema_parser.hpp>
#include <xb/xml_io.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace xb {

  namespace {

    // -- Namespace constants --------------------------------------------------

    const std::string wsdl2_ns = "http://www.w3.org/ns/wsdl";
    const std::string wsoap_ns = "http://www.w3.org/ns/wsdl/soap";
    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    // -- MEP URI suffixes -----------------------------------------------------

    const std::string mep_in_only = "in-only";
    const std::string mep_robust_in_only = "robust-in-only";
    const std::string mep_in_out = "in-out";
    const std::string mep_in_optional_out = "in-opt-out";

    // -- Namespace predicates -------------------------------------------------

    bool
    is_wsdl2(const qname& name, const std::string& local) {
      return name.namespace_uri() == wsdl2_ns && name.local_name() == local;
    }

    // -- Attribute helpers ----------------------------------------------------

    std::string
    opt_attr(xml_reader& reader, const std::string& local) {
      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        if (reader.attribute_name(i).local_name() == local &&
            reader.attribute_name(i).namespace_uri().empty()) {
          return std::string(reader.attribute_value(i));
        }
      }
      return "";
    }

    std::string
    opt_ns_attr(xml_reader& reader, const std::string& ns,
                const std::string& local) {
      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        if (reader.attribute_name(i).local_name() == local &&
            reader.attribute_name(i).namespace_uri() == ns) {
          return std::string(reader.attribute_value(i));
        }
      }
      return "";
    }

    // -- Whitespace-skipping read ---------------------------------------------

    bool
    read_skip_ws(xml_reader& reader) {
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::characters) {
          auto text = reader.text();
          bool all_ws = true;
          for (char c : text) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
              all_ws = false;
              break;
            }
          }
          if (all_ws) continue;
        }
        return true;
      }
      return false;
    }

    // -- QName resolution -----------------------------------------------------

    qname
    resolve_qname(xml_reader& reader, std::string_view value) {
      auto colon = value.find(':');
      if (colon == std::string_view::npos) return qname("", std::string(value));
      return qname(
          std::string(reader.namespace_uri_for_prefix(value.substr(0, colon))),
          std::string(value.substr(colon + 1)));
    }

    // -- Element token parsing ------------------------------------------------

    std::optional<wsdl2::element_token>
    parse_element_token(const std::string& value) {
      if (value == "#any") return wsdl2::element_token::any;
      if (value == "#none") return wsdl2::element_token::none;
      if (value == "#other") return wsdl2::element_token::other;
      return std::nullopt;
    }

    // -- MEP parsing ----------------------------------------------------------

    wsdl2::mep
    parse_mep_uri(const std::string& uri) {
      // Extract suffix after last '/'
      auto pos = uri.rfind('/');
      std::string suffix =
          (pos != std::string::npos) ? uri.substr(pos + 1) : uri;
      if (suffix == mep_in_only) return wsdl2::mep::in_only;
      if (suffix == mep_robust_in_only) return wsdl2::mep::robust_in_only;
      if (suffix == mep_in_out) return wsdl2::mep::in_out;
      if (suffix == mep_in_optional_out) return wsdl2::mep::in_optional_out;
      return wsdl2::mep::in_out; // default
    }

    // -- Element parsers ------------------------------------------------------

    wsdl2::operation
    parse_operation(xml_reader& reader) {
      wsdl2::operation op;
      op.name = opt_attr(reader, "name");

      std::string pattern_uri = opt_attr(reader, "pattern");
      bool has_pattern = !pattern_uri.empty();
      if (has_pattern) { op.pattern = parse_mep_uri(pattern_uri); }

      bool has_input = false;
      bool has_output = false;

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          break;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl2(reader.name(), "input")) {
            std::string elem = opt_attr(reader, "element");
            if (!elem.empty()) {
              auto tok = parse_element_token(elem);
              if (tok) {
                op.input_token = *tok;
              } else {
                op.input_element = resolve_qname(reader, elem);
              }
            }
            op.input_message_label = opt_attr(reader, "messageLabel");
            has_input = true;
            skip_element(reader);
          } else if (is_wsdl2(reader.name(), "output")) {
            std::string elem = opt_attr(reader, "element");
            if (!elem.empty()) {
              auto tok = parse_element_token(elem);
              if (tok) {
                op.output_token = *tok;
              } else {
                op.output_element = resolve_qname(reader, elem);
              }
            }
            op.output_message_label = opt_attr(reader, "messageLabel");
            has_output = true;
            skip_element(reader);
          } else if (is_wsdl2(reader.name(), "infault")) {
            wsdl2::infault_ref inf;
            std::string ref_str = opt_attr(reader, "ref");
            if (!ref_str.empty()) inf.ref = resolve_qname(reader, ref_str);
            inf.message_label = opt_attr(reader, "messageLabel");
            op.infaults.push_back(std::move(inf));
            skip_element(reader);
          } else if (is_wsdl2(reader.name(), "outfault")) {
            wsdl2::outfault_ref outf;
            std::string ref_str = opt_attr(reader, "ref");
            if (!ref_str.empty()) outf.ref = resolve_qname(reader, ref_str);
            outf.message_label = opt_attr(reader, "messageLabel");
            op.outfaults.push_back(std::move(outf));
            skip_element(reader);
          } else {
            skip_element(reader);
          }
        }
      }

      // Infer MEP from children if no pattern attribute
      if (!has_pattern) {
        if (has_input && has_output) {
          op.pattern = wsdl2::mep::in_out;
        } else if (has_input) {
          op.pattern = wsdl2::mep::in_only;
        }
      }

      return op;
    }

    wsdl2::interface
    parse_interface(xml_reader& reader) {
      wsdl2::interface iface;
      iface.name = opt_attr(reader, "name");

      std::string extends_str = opt_attr(reader, "extends");
      if (!extends_str.empty()) {
        // Space-separated list of QNames
        std::string_view sv(extends_str);
        while (!sv.empty()) {
          auto space = sv.find(' ');
          auto token = sv.substr(0, space);
          if (!token.empty()) {
            iface.extends.push_back(resolve_qname(reader, token));
          }
          if (space == std::string_view::npos) break;
          sv = sv.substr(space + 1);
        }
      }

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return iface;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl2(reader.name(), "fault")) {
            wsdl2::interface_fault f;
            f.name = opt_attr(reader, "name");
            std::string elem = opt_attr(reader, "element");
            if (!elem.empty()) {
              auto tok = parse_element_token(elem);
              if (tok) {
                f.token = *tok;
              } else {
                f.element = resolve_qname(reader, elem);
              }
            }
            skip_element(reader);
            iface.faults.push_back(std::move(f));
          } else if (is_wsdl2(reader.name(), "operation")) {
            iface.operations.push_back(parse_operation(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return iface;
    }

    wsdl2::binding
    parse_binding(xml_reader& reader) {
      wsdl2::binding b;
      b.name = opt_attr(reader, "name");

      std::string iface_str = opt_attr(reader, "interface");
      if (!iface_str.empty()) {
        b.interface_ref = resolve_qname(reader, iface_str);
      }

      b.type = opt_attr(reader, "type");
      b.soap.protocol = opt_ns_attr(reader, wsoap_ns, "protocol");
      b.soap.mep_default = opt_ns_attr(reader, wsoap_ns, "mepDefault");
      b.soap.version = opt_ns_attr(reader, wsoap_ns, "version");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return b;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl2(reader.name(), "operation")) {
            wsdl2::binding_operation bop;
            std::string ref_str = opt_attr(reader, "ref");
            if (!ref_str.empty()) bop.ref = resolve_qname(reader, ref_str);
            bop.soap.soap_action = opt_ns_attr(reader, wsoap_ns, "action");
            bop.soap.soap_mep = opt_ns_attr(reader, wsoap_ns, "mep");

            auto op_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == op_depth)
                break;
              if (reader.node_type() == xml_node_type::start_element) {
                if (is_wsdl2(reader.name(), "input")) {
                  wsdl2::binding_message_ref m;
                  m.message_label = opt_attr(reader, "messageLabel");
                  skip_element(reader);
                  bop.inputs.push_back(std::move(m));
                } else if (is_wsdl2(reader.name(), "output")) {
                  wsdl2::binding_message_ref m;
                  m.message_label = opt_attr(reader, "messageLabel");
                  skip_element(reader);
                  bop.outputs.push_back(std::move(m));
                } else if (is_wsdl2(reader.name(), "infault")) {
                  wsdl2::binding_message_ref m;
                  m.message_label = opt_attr(reader, "messageLabel");
                  skip_element(reader);
                  bop.infaults.push_back(std::move(m));
                } else if (is_wsdl2(reader.name(), "outfault")) {
                  wsdl2::binding_message_ref m;
                  m.message_label = opt_attr(reader, "messageLabel");
                  skip_element(reader);
                  bop.outfaults.push_back(std::move(m));
                } else {
                  skip_element(reader);
                }
              }
            }
            b.operations.push_back(std::move(bop));
          } else if (is_wsdl2(reader.name(), "fault")) {
            wsdl2::binding_fault bf;
            std::string ref_str = opt_attr(reader, "ref");
            if (!ref_str.empty()) bf.ref = resolve_qname(reader, ref_str);
            bf.soap_code = opt_ns_attr(reader, wsoap_ns, "code");
            skip_element(reader);
            b.faults.push_back(std::move(bf));
          } else {
            skip_element(reader);
          }
        }
      }
      return b;
    }

    wsdl2::service
    parse_service(xml_reader& reader) {
      wsdl2::service svc;
      svc.name = opt_attr(reader, "name");

      std::string iface_str = opt_attr(reader, "interface");
      if (!iface_str.empty()) {
        svc.interface_ref = resolve_qname(reader, iface_str);
      }

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return svc;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl2(reader.name(), "endpoint")) {
            wsdl2::endpoint ep;
            ep.name = opt_attr(reader, "name");
            std::string bind_str = opt_attr(reader, "binding");
            if (!bind_str.empty()) {
              ep.binding_ref = resolve_qname(reader, bind_str);
            }
            ep.address = opt_attr(reader, "address");
            skip_element(reader);
            svc.endpoints.push_back(std::move(ep));
          } else {
            skip_element(reader);
          }
        }
      }
      return svc;
    }

    void
    parse_types(xml_reader& reader, wsdl2::description& desc) {
      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return;
        if (reader.node_type() == xml_node_type::start_element) {
          if (reader.name().namespace_uri() == xs_ns &&
              reader.name().local_name() == "schema") {
            schema_parser sp;
            desc.types.add(sp.parse_at_element(reader));
          } else {
            skip_element(reader);
          }
        }
      }
    }

    wsdl2::description
    parse_description(xml_reader& reader) {
      wsdl2::description desc;
      desc.target_namespace = opt_attr(reader, "targetNamespace");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return desc;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl2(reader.name(), "import")) {
            wsdl2::import imp;
            imp.namespace_uri = opt_attr(reader, "namespace");
            imp.location = opt_attr(reader, "location");
            desc.imports.push_back(std::move(imp));
            skip_element(reader);
          } else if (is_wsdl2(reader.name(), "include")) {
            wsdl2::include inc;
            inc.location = opt_attr(reader, "location");
            desc.includes.push_back(std::move(inc));
            skip_element(reader);
          } else if (is_wsdl2(reader.name(), "types")) {
            parse_types(reader, desc);
          } else if (is_wsdl2(reader.name(), "interface")) {
            desc.interfaces.push_back(parse_interface(reader));
          } else if (is_wsdl2(reader.name(), "binding")) {
            desc.bindings.push_back(parse_binding(reader));
          } else if (is_wsdl2(reader.name(), "service")) {
            desc.services.push_back(parse_service(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return desc;
    }

  } // namespace

  wsdl2::description
  wsdl2_parser::parse(xml_reader& reader) {
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) break;
    }

    if (!is_wsdl2(reader.name(), "description")) {
      throw std::runtime_error(
          "wsdl2_parser: expected <wsdl:description> root element");
    }

    return parse_description(reader);
  }

} // namespace xb
