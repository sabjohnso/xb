#include <xb/wsdl_parser.hpp>

#include <xb/schema_parser.hpp>
#include <xb/xml_io.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace xb {

  namespace {

    // -- Namespace constants --------------------------------------------------

    const std::string wsdl_ns = "http://schemas.xmlsoap.org/wsdl/";
    const std::string soap11b_ns = "http://schemas.xmlsoap.org/wsdl/soap/";
    const std::string soap12b_ns = "http://schemas.xmlsoap.org/wsdl/soap12/";
    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    // -- Namespace predicates -------------------------------------------------

    bool
    is_wsdl(const qname& name, const std::string& local) {
      return name.namespace_uri() == wsdl_ns && name.local_name() == local;
    }

    bool
    is_soap_binding(const qname& name, const std::string& local) {
      return (name.namespace_uri() == soap11b_ns ||
              name.namespace_uri() == soap12b_ns) &&
             name.local_name() == local;
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

    // -- QName resolution in attribute values ---------------------------------

    qname
    resolve_qname(xml_reader& reader, std::string_view value) {
      auto colon = value.find(':');
      if (colon == std::string_view::npos) return qname("", std::string(value));
      return qname(
          std::string(reader.namespace_uri_for_prefix(value.substr(0, colon))),
          std::string(value.substr(colon + 1)));
    }

    // -- Style/use string conversion ------------------------------------------

    wsdl::binding_style
    parse_style(const std::string& s) {
      if (s == "rpc") return wsdl::binding_style::rpc;
      return wsdl::binding_style::document;
    }

    wsdl::body_use
    parse_use(const std::string& s) {
      if (s == "encoded") return wsdl::body_use::encoded;
      return wsdl::body_use::literal;
    }

    // -- Forward declarations -------------------------------------------------

    wsdl::document
    parse_definitions(xml_reader&);
    void
    parse_types(xml_reader&, wsdl::document&);
    wsdl::message
    parse_message(xml_reader&);
    wsdl::port_type
    parse_port_type(xml_reader&);
    wsdl::operation
    parse_pt_op(xml_reader&);
    wsdl::binding
    parse_binding(xml_reader&);
    wsdl::binding_operation
    parse_binding_op(xml_reader&);
    wsdl::service
    parse_service(xml_reader&);

    // -- <wsdl:types> ---------------------------------------------------------

    void
    parse_types(xml_reader& reader, wsdl::document& doc) {
      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return;
        if (reader.node_type() == xml_node_type::start_element) {
          if (reader.name().namespace_uri() == xs_ns &&
              reader.name().local_name() == "schema") {
            // Reader is already at the xs:schema start element;
            // use parse_at_element to avoid a redundant advance.
            schema_parser sp;
            doc.types.add(sp.parse_at_element(reader));
          } else {
            skip_element(reader);
          }
        }
      }
    }

    // -- <wsdl:message> -------------------------------------------------------

    wsdl::message
    parse_message(xml_reader& reader) {
      wsdl::message msg;
      msg.name = opt_attr(reader, "name");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return msg;
        if (reader.node_type() == xml_node_type::start_element &&
            is_wsdl(reader.name(), "part")) {
          wsdl::part p;
          p.name = opt_attr(reader, "name");
          std::string elem_val = opt_attr(reader, "element");
          std::string type_val = opt_attr(reader, "type");
          if (!elem_val.empty()) {
            p.ref = wsdl::part_by_element{resolve_qname(reader, elem_val)};
          } else {
            p.ref = wsdl::part_by_type{resolve_qname(reader, type_val)};
          }
          msg.parts.push_back(std::move(p));
          skip_element(reader);
        } else if (reader.node_type() == xml_node_type::start_element) {
          skip_element(reader);
        }
      }
      return msg;
    }

    // -- <wsdl:operation> inside <wsdl:portType> ------------------------------

    wsdl::operation
    parse_pt_op(xml_reader& reader) {
      wsdl::operation op;
      op.name = opt_attr(reader, "name");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return op;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl(reader.name(), "input")) {
            std::string msg = opt_attr(reader, "message");
            op.input_message = resolve_qname(reader, msg);
            skip_element(reader);
          } else if (is_wsdl(reader.name(), "output")) {
            std::string msg = opt_attr(reader, "message");
            op.output_message = resolve_qname(reader, msg);
            skip_element(reader);
          } else if (is_wsdl(reader.name(), "fault")) {
            wsdl::fault_ref fr;
            fr.name = opt_attr(reader, "name");
            std::string msg = opt_attr(reader, "message");
            fr.message = resolve_qname(reader, msg);
            op.faults.push_back(std::move(fr));
            skip_element(reader);
          } else {
            skip_element(reader);
          }
        }
      }
      return op;
    }

    // -- <wsdl:portType> ------------------------------------------------------

    wsdl::port_type
    parse_port_type(xml_reader& reader) {
      wsdl::port_type pt;
      pt.name = opt_attr(reader, "name");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return pt;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl(reader.name(), "operation")) {
            pt.operations.push_back(parse_pt_op(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return pt;
    }

    // -- <wsdl:operation> inside <wsdl:binding> -------------------------------

    wsdl::binding_operation
    parse_binding_op(xml_reader& reader) {
      wsdl::binding_operation bo;
      bo.name = opt_attr(reader, "name");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return bo;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_soap_binding(reader.name(), "operation")) {
            bo.soap_op.soap_action = opt_attr(reader, "soapAction");
            std::string style_str = opt_attr(reader, "style");
            if (!style_str.empty())
              bo.soap_op.style_override = parse_style(style_str);
            skip_element(reader);
          } else if (is_wsdl(reader.name(), "input")) {
            // consume children looking for soap:body
            auto in_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == in_depth)
                break;
              if (reader.node_type() == xml_node_type::start_element &&
                  is_soap_binding(reader.name(), "body")) {
                bo.input_body.use = parse_use(opt_attr(reader, "use"));
                bo.input_body.namespace_uri = opt_attr(reader, "namespace");
                skip_element(reader);
              } else if (reader.node_type() == xml_node_type::start_element) {
                skip_element(reader);
              }
            }
          } else if (is_wsdl(reader.name(), "output")) {
            auto out_depth = reader.depth();
            while (read_skip_ws(reader)) {
              if (reader.node_type() == xml_node_type::end_element &&
                  reader.depth() == out_depth)
                break;
              if (reader.node_type() == xml_node_type::start_element &&
                  is_soap_binding(reader.name(), "body")) {
                bo.output_body.use = parse_use(opt_attr(reader, "use"));
                bo.output_body.namespace_uri = opt_attr(reader, "namespace");
                skip_element(reader);
              } else if (reader.node_type() == xml_node_type::start_element) {
                skip_element(reader);
              }
            }
          } else {
            skip_element(reader);
          }
        }
      }
      return bo;
    }

    // -- <wsdl:binding> -------------------------------------------------------

    wsdl::binding
    parse_binding(xml_reader& reader) {
      wsdl::binding b;
      b.name = opt_attr(reader, "name");
      std::string type_str = opt_attr(reader, "type");
      b.port_type = resolve_qname(reader, type_str);

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return b;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_soap_binding(reader.name(), "binding")) {
            std::string style_str = opt_attr(reader, "style");
            b.soap.style = parse_style(style_str);
            b.soap.transport = opt_attr(reader, "transport");
            b.soap.soap_ver = (reader.name().namespace_uri() == soap12b_ns)
                                  ? soap::soap_version::v1_2
                                  : soap::soap_version::v1_1;
            skip_element(reader);
          } else if (is_wsdl(reader.name(), "operation")) {
            b.operations.push_back(parse_binding_op(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return b;
    }

    // -- <wsdl:service> -------------------------------------------------------

    wsdl::service
    parse_service(xml_reader& reader) {
      wsdl::service svc;
      svc.name = opt_attr(reader, "name");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return svc;
        if (reader.node_type() == xml_node_type::start_element &&
            is_wsdl(reader.name(), "port")) {
          wsdl::port p;
          p.name = opt_attr(reader, "name");
          std::string binding_str = opt_attr(reader, "binding");
          p.binding = resolve_qname(reader, binding_str);

          // consume children looking for soap:address
          auto port_depth = reader.depth();
          while (read_skip_ws(reader)) {
            if (reader.node_type() == xml_node_type::end_element &&
                reader.depth() == port_depth)
              break;
            if (reader.node_type() == xml_node_type::start_element &&
                is_soap_binding(reader.name(), "address")) {
              p.address = opt_attr(reader, "location");
              skip_element(reader);
            } else if (reader.node_type() == xml_node_type::start_element) {
              skip_element(reader);
            }
          }

          svc.ports.push_back(std::move(p));
        } else if (reader.node_type() == xml_node_type::start_element) {
          skip_element(reader);
        }
      }
      return svc;
    }

    // -- <wsdl:definitions> ---------------------------------------------------

    wsdl::document
    parse_definitions(xml_reader& reader) {
      wsdl::document doc;
      doc.name = opt_attr(reader, "name");
      doc.target_namespace = opt_attr(reader, "targetNamespace");

      auto depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return doc;
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_wsdl(reader.name(), "import")) {
            wsdl::import imp;
            imp.namespace_uri = opt_attr(reader, "namespace");
            imp.location = opt_attr(reader, "location");
            doc.imports.push_back(std::move(imp));
            skip_element(reader);
          } else if (is_wsdl(reader.name(), "types")) {
            parse_types(reader, doc);
          } else if (is_wsdl(reader.name(), "message")) {
            doc.messages.push_back(parse_message(reader));
          } else if (is_wsdl(reader.name(), "portType")) {
            doc.port_types.push_back(parse_port_type(reader));
          } else if (is_wsdl(reader.name(), "binding")) {
            doc.bindings.push_back(parse_binding(reader));
          } else if (is_wsdl(reader.name(), "service")) {
            doc.services.push_back(parse_service(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return doc;
    }

  } // namespace

  // -- wsdl_parser::parse ---------------------------------------------------

  wsdl::document
  wsdl_parser::parse(xml_reader& reader) {
    // Advance to the root element
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) break;
    }

    if (!is_wsdl(reader.name(), "definitions")) {
      throw std::runtime_error(
          "wsdl_parser: expected <wsdl:definitions> root element");
    }

    return parse_definitions(reader);
  }

} // namespace xb
