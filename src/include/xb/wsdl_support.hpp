#pragma once

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/qname.hpp>
#include <xb/soap_fault.hpp>
#include <xb/soap_model.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xb::service {

  struct soap_call_fault : std::runtime_error {
    soap::fault fault_value;

    explicit soap_call_fault(soap::fault f)
        : std::runtime_error("SOAP fault received"), fault_value(std::move(f)) {
    }
  };

  // Serialize a typed value to an any_element via XML round-trip.
  template <typename T, typename WriteFn>
  any_element
  make_body_element(const qname& name, const T& value, WriteFn write_fn) {
    std::ostringstream os;
    ostream_writer writer(os);
    write_fn(writer, value);
    std::string xml = os.str();

    expat_reader reader(xml);
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) {
        return any_element(reader);
      }
    }
    throw std::runtime_error("make_body_element: failed to serialize element " +
                             name.local_name());
  }

  // Deserialize an any_element back to a typed value via XML round-trip.
  template <typename T, typename ReadFn>
  T
  parse_body_element(const any_element& elem, ReadFn read_fn) {
    std::ostringstream os;
    ostream_writer writer(os);
    elem.write(writer);
    std::string xml = os.str();

    expat_reader reader(xml);
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) {
        return read_fn(reader);
      }
    }
    throw std::runtime_error("parse_body_element: failed to parse element " +
                             elem.name().local_name());
  }

  // Check if envelope body contains a SOAP fault; if so, throw.
  inline void
  check_fault(const soap::envelope& env) {
    if (env.body.empty()) return;

    const auto& first = env.body[0];
    if (soap::is_fault(first, env.version)) {
      // Re-parse the fault element
      std::ostringstream os;
      ostream_writer writer(os);
      first.write(writer);
      std::string xml = os.str();

      expat_reader reader(xml);
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::start_element) {
          auto f = soap::read_fault(reader, env.version);
          throw soap_call_fault(std::move(f));
        }
      }
    }
  }

  // Build an RPC wrapper element from name/value pairs.
  inline any_element
  make_rpc_request(
      const qname& operation_name,
      const std::vector<std::pair<std::string, std::string>>& parts) {
    std::vector<any_element::child> children;
    for (const auto& [name, value] : parts) {
      children.push_back(
          any_element(qname("", name), {},
                      std::vector<any_element::child>{std::string(value)}));
    }
    return any_element(operation_name, {}, std::move(children));
  }

} // namespace xb::service
