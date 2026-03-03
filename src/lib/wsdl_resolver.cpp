#include <xb/wsdl_resolver.hpp>

#include <stdexcept>
#include <string>

namespace xb {

  namespace {

    const wsdl::binding*
    find_binding(const wsdl::document& doc, const qname& name) {
      for (const auto& b : doc.bindings) {
        if (b.name == name.local_name()) return &b;
      }
      return nullptr;
    }

    const wsdl::port_type*
    find_port_type(const wsdl::document& doc, const qname& name) {
      for (const auto& pt : doc.port_types) {
        if (pt.name == name.local_name()) return &pt;
      }
      return nullptr;
    }

    const wsdl::message*
    find_message(const wsdl::document& doc, const qname& name) {
      for (const auto& m : doc.messages) {
        if (m.name == name.local_name()) return &m;
      }
      return nullptr;
    }

    const wsdl::operation*
    find_port_type_operation(const wsdl::port_type& pt,
                             const std::string& name) {
      for (const auto& op : pt.operations) {
        if (op.name == name) return &op;
      }
      return nullptr;
    }

    service::resolved_part
    resolve_part(const wsdl::part& part, const type_map& types,
                 const codegen_options& opts) {
      service::resolved_part rp;
      rp.name = part.name;

      std::visit(
          [&](const auto& ref) {
            using T = std::decay_t<decltype(ref)>;
            if constexpr (std::is_same_v<T, wsdl::part_by_element>) {
              rp.xsd_name = ref.element;
              rp.is_element = true;

              auto cpp_ns =
                  cpp_namespace_for(ref.element.namespace_uri(), opts);
              auto id =
                  to_snake_case(to_cpp_identifier(ref.element.local_name()));
              if (cpp_ns.empty()) {
                rp.cpp_type = id;
                rp.read_function = "read_" + id;
                rp.write_function = "write_" + id;
              } else {
                rp.cpp_type = cpp_ns + "::" + id;
                rp.read_function = cpp_ns + "::read_" + id;
                rp.write_function = cpp_ns + "::write_" + id;
              }
            } else {
              rp.xsd_name = ref.type;
              rp.is_element = false;

              const auto* mapping = types.find(ref.type.local_name());
              if (mapping) {
                rp.cpp_type = mapping->cpp_type;
                rp.read_function =
                    "xb::read_" +
                    to_snake_case(to_cpp_identifier(ref.type.local_name()));
                rp.write_function =
                    "xb::write_" +
                    to_snake_case(to_cpp_identifier(ref.type.local_name()));
              } else {
                auto cpp_ns = cpp_namespace_for(ref.type.namespace_uri(), opts);
                auto id =
                    to_snake_case(to_cpp_identifier(ref.type.local_name()));
                if (cpp_ns.empty()) {
                  rp.cpp_type = id;
                  rp.read_function = "read_" + id;
                  rp.write_function = "write_" + id;
                } else {
                  rp.cpp_type = cpp_ns + "::" + id;
                  rp.read_function = cpp_ns + "::read_" + id;
                  rp.write_function = cpp_ns + "::write_" + id;
                }
              }
            }
          },
          part.ref);

      return rp;
    }

    service::resolved_fault
    resolve_fault(const wsdl::fault_ref& fref, const wsdl::document& doc,
                  const type_map& types, const codegen_options& opts) {
      service::resolved_fault rf;
      rf.name = fref.name;

      const auto* msg = find_message(doc, fref.message);
      if (!msg) {
        throw std::runtime_error("WSDL resolver: fault message not found: " +
                                 fref.message.local_name());
      }

      if (!msg->parts.empty()) {
        rf.detail = resolve_part(msg->parts[0], types, opts);
      }

      return rf;
    }

    service::resolved_operation
    resolve_operation(const wsdl::binding_operation& bop,
                      const wsdl::operation& pt_op,
                      const wsdl::binding& binding, const wsdl::document& doc,
                      const type_map& types, const codegen_options& opts) {
      service::resolved_operation rop;
      rop.name = bop.name;
      rop.soap_action = bop.soap_op.soap_action;

      // Effective style: operation override > binding default
      rop.style = bop.soap_op.style_override.value_or(binding.soap.style);
      rop.use = bop.input_body.use;

      if (rop.style == wsdl::binding_style::rpc) {
        rop.rpc_namespace = bop.input_body.namespace_uri;
      }

      // Resolve input message
      const auto* in_msg = find_message(doc, pt_op.input_message);
      if (!in_msg) {
        throw std::runtime_error("WSDL resolver: input message not found: " +
                                 pt_op.input_message.local_name());
      }
      for (const auto& part : in_msg->parts) {
        rop.input.push_back(resolve_part(part, types, opts));
      }

      // Resolve output message (if present)
      if (pt_op.output_message) {
        const auto* out_msg = find_message(doc, *pt_op.output_message);
        if (!out_msg) {
          throw std::runtime_error("WSDL resolver: output message not found: " +
                                   pt_op.output_message->local_name());
        }
        for (const auto& part : out_msg->parts) {
          rop.output.push_back(resolve_part(part, types, opts));
        }
        rop.one_way = false;
      } else {
        rop.one_way = true;
      }

      // Resolve faults
      for (const auto& fref : pt_op.faults) {
        rop.faults.push_back(resolve_fault(fref, doc, types, opts));
      }

      return rop;
    }

  } // namespace

  service::service_description
  wsdl_resolver::resolve(const wsdl::document& doc, const type_map& types,
                         const codegen_options& options) const {
    service::service_description desc;

    for (const auto& svc : doc.services) {
      service::resolved_service rsvc;
      rsvc.name = svc.name;
      rsvc.target_namespace = doc.target_namespace;

      for (const auto& port : svc.ports) {
        const auto* binding = find_binding(doc, port.binding);
        if (!binding) {
          throw std::runtime_error("WSDL resolver: binding not found: " +
                                   port.binding.local_name());
        }

        const auto* pt = find_port_type(doc, binding->port_type);
        if (!pt) {
          throw std::runtime_error("WSDL resolver: port type not found: " +
                                   binding->port_type.local_name());
        }

        service::resolved_port rport;
        rport.name = port.name;
        rport.address = port.address;

        for (const auto& bop : binding->operations) {
          const auto* pt_op = find_port_type_operation(*pt, bop.name);
          if (!pt_op) {
            throw std::runtime_error(
                "WSDL resolver: port type operation not found: " + bop.name);
          }
          rport.operations.push_back(
              resolve_operation(bop, *pt_op, *binding, doc, types, options));
        }

        rsvc.ports.push_back(std::move(rport));
      }

      desc.services.push_back(std::move(rsvc));
    }

    return desc;
  }

} // namespace xb
