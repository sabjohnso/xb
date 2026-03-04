#pragma once

#include <xb/qname.hpp>
#include <xb/wsdl_model.hpp>

#include <string>
#include <vector>

namespace xb::service {

  struct resolved_part {
    std::string name;
    qname xsd_name;
    bool is_element = false;
    std::string cpp_type;
    std::string read_function;
    std::string write_function;

    bool
    operator==(const resolved_part&) const = default;
  };

  struct resolved_fault {
    std::string name;
    resolved_part detail;

    bool
    operator==(const resolved_fault&) const = default;
  };

  struct resolved_operation {
    std::string name;
    std::string soap_action;
    wsdl::binding_style style = wsdl::binding_style::document;
    wsdl::body_use use = wsdl::body_use::literal;
    std::string rpc_namespace;
    std::vector<resolved_part> input;
    std::vector<resolved_part> output;
    bool one_way = false;
    std::vector<resolved_fault> faults;

    bool
    operator==(const resolved_operation&) const = default;
  };

  struct resolved_port {
    std::string name;
    std::string address;
    soap::soap_version soap_ver = soap::soap_version::v1_1;
    std::vector<resolved_operation> operations;

    bool
    operator==(const resolved_port&) const = default;
  };

  struct resolved_service {
    std::string name;
    std::string target_namespace;
    std::vector<resolved_port> ports;

    bool
    operator==(const resolved_service&) const = default;
  };

  struct service_description {
    std::vector<resolved_service> services;

    bool
    operator==(const service_description&) const = default;
  };

} // namespace xb::service
