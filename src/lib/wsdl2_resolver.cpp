#include <xb/wsdl2_resolver.hpp>

#include <set>
#include <stdexcept>
#include <string>

namespace xb {

  namespace {

    // -- Lookup helpers -------------------------------------------------------

    const wsdl2::interface*
    find_interface(const wsdl2::description& desc, const std::string& name) {
      for (const auto& iface : desc.interfaces) {
        if (iface.name == name) return &iface;
      }
      return nullptr;
    }

    const wsdl2::binding*
    find_binding(const wsdl2::description& desc, const std::string& name) {
      for (const auto& b : desc.bindings) {
        if (b.name == name) return &b;
      }
      return nullptr;
    }

    const wsdl2::binding_operation*
    find_binding_op(const wsdl2::binding& b, const std::string& op_name) {
      for (const auto& bop : b.operations) {
        if (bop.ref.local_name() == op_name) return &bop;
      }
      return nullptr;
    }

    // -- Interface inheritance flattening
    // --------------------------------------

    // Collect all operations from an interface, including inherited ones.
    // Child operations override parent operations with the same name.
    void
    collect_operations(const wsdl2::description& desc,
                       const wsdl2::interface& iface,
                       std::vector<wsdl2::operation>& ops,
                       std::set<std::string>& visited) {
      if (visited.count(iface.name)) {
        throw std::runtime_error(
            "WSDL 2.0 resolver: circular interface inheritance detected: " +
            iface.name);
      }
      visited.insert(iface.name);

      // Collect parent operations first (depth-first)
      for (const auto& parent_ref : iface.extends) {
        const auto* parent = find_interface(desc, parent_ref.local_name());
        if (!parent) {
          throw std::runtime_error(
              "WSDL 2.0 resolver: parent interface not found: " +
              parent_ref.local_name());
        }
        collect_operations(desc, *parent, ops, visited);
      }

      // Add own operations, overriding parent's same-name operations
      for (const auto& op : iface.operations) {
        // Remove parent's operation with same name
        ops.erase(std::remove_if(ops.begin(), ops.end(),
                                 [&](const wsdl2::operation& existing) {
                                   return existing.name == op.name;
                                 }),
                  ops.end());
        ops.push_back(op);
      }
    }

    // Collect all faults (own + inherited)
    void
    collect_faults(const wsdl2::description& desc,
                   const wsdl2::interface& iface,
                   std::vector<wsdl2::interface_fault>& faults,
                   std::set<std::string>& visited) {
      if (visited.count(iface.name)) return; // Already handled in ops
      visited.insert(iface.name);

      for (const auto& parent_ref : iface.extends) {
        const auto* parent = find_interface(desc, parent_ref.local_name());
        if (parent) { collect_faults(desc, *parent, faults, visited); }
      }

      for (const auto& f : iface.faults) {
        bool found = false;
        for (const auto& existing : faults) {
          if (existing.name == f.name) {
            found = true;
            break;
          }
        }
        if (!found) faults.push_back(f);
      }
    }

    // -- Part resolution ------------------------------------------------------

    service::resolved_part
    resolve_element_part(const qname& element, const codegen_options& opts) {
      service::resolved_part rp;
      rp.name = element.local_name();
      rp.xsd_name = element;
      rp.is_element = true;

      auto cpp_ns = cpp_namespace_for(element.namespace_uri(), opts);
      auto id = to_snake_case(to_cpp_identifier(element.local_name()));
      if (cpp_ns.empty()) {
        rp.cpp_type = id;
        rp.read_function = "read_" + id;
        rp.write_function = "write_" + id;
      } else {
        rp.cpp_type = cpp_ns + "::" + id;
        rp.read_function = cpp_ns + "::read_" + id;
        rp.write_function = cpp_ns + "::write_" + id;
      }

      return rp;
    }

    // -- Operation resolution -------------------------------------------------

    service::resolved_operation
    resolve_operation(const wsdl2::operation& op, const wsdl2::binding* binding,
                      const std::vector<wsdl2::interface_fault>& all_faults,
                      const codegen_options& opts) {
      service::resolved_operation rop;
      rop.name = op.name;
      rop.style = wsdl::binding_style::document;
      rop.use = wsdl::body_use::literal;

      // SOAPAction from binding operation
      if (binding) {
        const auto* bop = find_binding_op(*binding, op.name);
        if (bop) { rop.soap_action = bop->soap.soap_action; }
      }

      // Input
      if (op.input_element) {
        rop.input.push_back(resolve_element_part(*op.input_element, opts));
      }

      // Output
      if (op.output_element) {
        rop.output.push_back(resolve_element_part(*op.output_element, opts));
        rop.one_way = false;
      } else {
        rop.one_way = true;
      }

      // MEP-based one_way
      if (op.pattern == wsdl2::mep::in_only ||
          op.pattern == wsdl2::mep::robust_in_only) {
        rop.one_way = true;
        rop.output.clear();
      }

      // Faults from infault refs
      for (const auto& infault : op.infaults) {
        service::resolved_fault rf;
        rf.name = infault.ref.local_name();

        // Find the interface fault to get the element
        for (const auto& ifault : all_faults) {
          if (ifault.name == infault.ref.local_name() && ifault.element) {
            rf.detail = resolve_element_part(*ifault.element, opts);
            break;
          }
        }

        rop.faults.push_back(std::move(rf));
      }

      return rop;
    }

    // -- SOAP version detection -----------------------------------------------

    soap::soap_version
    detect_soap_version(const wsdl2::binding& binding) {
      // Explicit wsoap:version takes precedence
      if (binding.soap.version == "1.1") return soap::soap_version::v1_1;
      if (binding.soap.version == "1.2") return soap::soap_version::v1_2;
      // Fall back to protocol URI heuristic; WSDL 2.0 defaults to SOAP 1.2
      if (binding.soap.protocol.find("1.1") != std::string::npos) {
        return soap::soap_version::v1_1;
      }
      return soap::soap_version::v1_2;
    }

  } // namespace

  service::service_description
  wsdl2_resolver::resolve(const wsdl2::description& desc,
                          const type_map& /*types*/,
                          const codegen_options& options) const {
    service::service_description result;

    for (const auto& svc : desc.services) {
      service::resolved_service rsvc;
      rsvc.name = svc.name;
      rsvc.target_namespace = desc.target_namespace;

      for (const auto& ep : svc.endpoints) {
        const auto* binding = find_binding(desc, ep.binding_ref.local_name());
        if (!binding) {
          throw std::runtime_error("WSDL 2.0 resolver: binding not found: " +
                                   ep.binding_ref.local_name());
        }

        service::resolved_port rport;
        rport.name = ep.name;
        rport.address = ep.address;
        rport.soap_ver = detect_soap_version(*binding);

        // Resolve operations if binding references an interface
        if (!binding->interface_ref.local_name().empty()) {
          const auto* iface =
              find_interface(desc, binding->interface_ref.local_name());
          if (!iface) {
            throw std::runtime_error(
                "WSDL 2.0 resolver: interface not found: " +
                binding->interface_ref.local_name());
          }

          // Flatten inherited operations
          std::vector<wsdl2::operation> all_ops;
          std::set<std::string> visited_ops;
          collect_operations(desc, *iface, all_ops, visited_ops);

          // Flatten inherited faults
          std::vector<wsdl2::interface_fault> all_faults;
          std::set<std::string> visited_faults;
          collect_faults(desc, *iface, all_faults, visited_faults);

          for (const auto& op : all_ops) {
            rport.operations.push_back(
                resolve_operation(op, binding, all_faults, options));
          }
        }

        rsvc.ports.push_back(std::move(rport));
      }

      result.services.push_back(std::move(rsvc));
    }

    return result;
  }

} // namespace xb
