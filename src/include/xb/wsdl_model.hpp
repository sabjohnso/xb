#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/soap_model.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace xb::wsdl {

  // -- Part reference alternatives -------------------------------------------

  struct part_by_element {
    qname element;
    bool
    operator==(const part_by_element&) const = default;
  };

  struct part_by_type {
    qname type;
    bool
    operator==(const part_by_type&) const = default;
  };

  // -- Message parts ---------------------------------------------------------

  struct part {
    std::string name;
    std::variant<part_by_element, part_by_type> ref;
    bool
    operator==(const part&) const = default;
  };

  struct message {
    std::string name;
    std::vector<part> parts;
    bool
    operator==(const message&) const = default;
  };

  // -- Port type operations --------------------------------------------------

  struct fault_ref {
    std::string name;
    qname message;
    bool
    operator==(const fault_ref&) const = default;
  };

  struct operation {
    std::string name;
    qname input_message;
    std::optional<qname> output_message; // absent = one-way
    std::vector<fault_ref> faults;
    bool
    operator==(const operation&) const = default;
  };

  struct port_type {
    std::string name;
    std::vector<operation> operations;
    bool
    operator==(const port_type&) const = default;
  };

  // -- Binding extensions ----------------------------------------------------

  enum class binding_style { document, rpc };
  enum class body_use { literal, encoded };

  struct soap_binding_ext {
    binding_style style = binding_style::document;
    std::string transport;
    soap::soap_version soap_ver = soap::soap_version::v1_1;
    bool
    operator==(const soap_binding_ext&) const = default;
  };

  struct soap_body_ext {
    body_use use = body_use::literal;
    std::string namespace_uri;
    bool
    operator==(const soap_body_ext&) const = default;
  };

  struct soap_operation_ext {
    std::string soap_action;
    std::optional<binding_style> style_override;
    bool
    operator==(const soap_operation_ext&) const = default;
  };

  struct binding_operation {
    std::string name;
    soap_operation_ext soap_op;
    soap_body_ext input_body;
    soap_body_ext output_body;
    bool
    operator==(const binding_operation&) const = default;
  };

  struct binding {
    std::string name;
    qname port_type;
    soap_binding_ext soap;
    std::vector<binding_operation> operations;
    bool
    operator==(const binding&) const = default;
  };

  // -- Service ---------------------------------------------------------------

  struct port {
    std::string name;
    qname binding;
    std::string address; // soap:address location=
    bool
    operator==(const port&) const = default;
  };

  struct service {
    std::string name;
    std::vector<port> ports;
    bool
    operator==(const service&) const = default;
  };

  // -- Import ----------------------------------------------------------------

  struct import {
    std::string namespace_uri;
    std::string location;
    bool
    operator==(const import&) const = default;
  };

  // -- Document (top-level) --------------------------------------------------

  struct document {
    std::string name;
    std::string target_namespace;
    std::vector<import> imports;
    std::vector<message> messages;
    std::vector<port_type> port_types;
    std::vector<binding> bindings;
    std::vector<service> services;
    schema_set types; // extracted from <wsdl:types>
    // No operator== — schema_set is not equality-comparable.
  };

} // namespace xb::wsdl
