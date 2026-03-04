#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>

#include <optional>
#include <string>
#include <vector>

namespace xb::wsdl2 {

  // -- MEP (Message Exchange Pattern) -----------------------------------------

  enum class mep { in_only, robust_in_only, in_out, in_optional_out };

  // Special element declarations per WSDL 2.0 spec (section 2.4.1, 2.3.1)
  enum class element_token { any, none, other };

  // -- Import / Include -------------------------------------------------------

  struct import {
    std::string namespace_uri;
    std::string location;
    bool
    operator==(const import&) const = default;
  };

  struct include {
    std::string location;
    bool
    operator==(const include&) const = default;
  };

  // -- Interface faults -------------------------------------------------------

  struct interface_fault {
    std::string name;
    std::optional<qname> element;
    std::optional<element_token> token;
    bool
    operator==(const interface_fault&) const = default;
  };

  // -- Fault references on operations -----------------------------------------

  struct infault_ref {
    qname ref;
    std::string message_label;
    bool
    operator==(const infault_ref&) const = default;
  };

  struct outfault_ref {
    qname ref;
    std::string message_label;
    bool
    operator==(const outfault_ref&) const = default;
  };

  // -- Operation --------------------------------------------------------------

  struct operation {
    std::string name;
    mep pattern = mep::in_out;
    std::optional<qname> input_element;
    std::optional<qname> output_element;
    std::optional<element_token> input_token;
    std::optional<element_token> output_token;
    std::string input_message_label;
    std::string output_message_label;
    std::vector<infault_ref> infaults;
    std::vector<outfault_ref> outfaults;
    bool
    operator==(const operation&) const = default;
  };

  // -- Interface --------------------------------------------------------------

  struct interface {
    std::string name;
    std::vector<qname> extends;
    std::vector<interface_fault> faults;
    std::vector<operation> operations;
    bool
    operator==(const interface&) const = default;
  };

  // -- Binding SOAP extensions ------------------------------------------------

  struct soap_binding_info {
    std::string protocol;
    std::string mep_default;
    std::string version;
    bool
    operator==(const soap_binding_info&) const = default;
  };

  struct soap_operation_info {
    std::string soap_action;
    std::string soap_mep;
    bool
    operator==(const soap_operation_info&) const = default;
  };

  // -- Binding ----------------------------------------------------------------

  struct binding_fault {
    qname ref;
    std::string soap_code;
    bool
    operator==(const binding_fault&) const = default;
  };

  struct binding_message_ref {
    std::string message_label;
    std::string soap_header_ns;
    std::string soap_header_element;
    bool
    operator==(const binding_message_ref&) const = default;
  };

  struct binding_operation {
    qname ref;
    soap_operation_info soap;
    std::vector<binding_message_ref> inputs;
    std::vector<binding_message_ref> outputs;
    std::vector<binding_message_ref> infaults;
    std::vector<binding_message_ref> outfaults;
    bool
    operator==(const binding_operation&) const = default;
  };

  struct binding {
    std::string name;
    qname interface_ref;
    std::string type;
    soap_binding_info soap;
    std::vector<binding_operation> operations;
    std::vector<binding_fault> faults;
    bool
    operator==(const binding&) const = default;
  };

  // -- Service ----------------------------------------------------------------

  struct endpoint {
    std::string name;
    qname binding_ref;
    std::string address;
    bool
    operator==(const endpoint&) const = default;
  };

  struct service {
    std::string name;
    qname interface_ref;
    std::vector<endpoint> endpoints;
    bool
    operator==(const service&) const = default;
  };

  // -- Description (top-level) ------------------------------------------------

  struct description {
    std::string target_namespace;
    std::vector<import> imports;
    std::vector<include> includes;
    std::vector<interface> interfaces;
    std::vector<binding> bindings;
    std::vector<service> services;
    schema_set types;

    // No operator== — schema_set is not equality-comparable.
    // Provide equality that compares everything except types.
    bool
    operator==(const description& other) const {
      return target_namespace == other.target_namespace &&
             imports == other.imports && includes == other.includes &&
             interfaces == other.interfaces && bindings == other.bindings &&
             services == other.services;
    }
  };

} // namespace xb::wsdl2
