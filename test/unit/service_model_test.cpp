#include <xb/service_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace svc = xb::service;

// -- resolved_part ------------------------------------------------------------

TEST_CASE("service model: resolved_part default construction",
          "[service_model]") {
  svc::resolved_part p;
  CHECK(p.name.empty());
  CHECK(p.xsd_name == xb::qname());
  CHECK(p.is_element == false);
  CHECK(p.cpp_type.empty());
  CHECK(p.read_function.empty());
  CHECK(p.write_function.empty());
}

TEST_CASE("service model: resolved_part with values", "[service_model]") {
  svc::resolved_part p;
  p.name = "parameters";
  p.xsd_name = xb::qname("urn:ex", "GetPrice");
  p.is_element = true;
  p.cpp_type = "ex::get_price";
  p.read_function = "ex::read_get_price";
  p.write_function = "ex::write_get_price";

  CHECK(p.name == "parameters");
  CHECK(p.xsd_name == xb::qname("urn:ex", "GetPrice"));
  CHECK(p.is_element == true);
  CHECK(p.cpp_type == "ex::get_price");
  CHECK(p.read_function == "ex::read_get_price");
  CHECK(p.write_function == "ex::write_get_price");
}

TEST_CASE("service model: resolved_part equality", "[service_model]") {
  svc::resolved_part a;
  a.name = "p";
  a.xsd_name = xb::qname("urn:ex", "T");
  a.is_element = true;
  a.cpp_type = "ex::t";
  a.read_function = "ex::read_t";
  a.write_function = "ex::write_t";

  svc::resolved_part b = a;
  CHECK(a == b);

  b.name = "other";
  CHECK_FALSE(a == b);
}

// -- resolved_fault -----------------------------------------------------------

TEST_CASE("service model: resolved_fault default construction",
          "[service_model]") {
  svc::resolved_fault f;
  CHECK(f.name.empty());
}

TEST_CASE("service model: resolved_fault with values", "[service_model]") {
  svc::resolved_fault f;
  f.name = "ServerFault";
  f.detail.name = "detail";
  f.detail.xsd_name = xb::qname("urn:ex", "FaultDetail");
  f.detail.is_element = true;
  f.detail.cpp_type = "ex::fault_detail";
  f.detail.read_function = "ex::read_fault_detail";
  f.detail.write_function = "ex::write_fault_detail";

  CHECK(f.name == "ServerFault");
  CHECK(f.detail.cpp_type == "ex::fault_detail");
}

TEST_CASE("service model: resolved_fault equality", "[service_model]") {
  svc::resolved_fault a;
  a.name = "F1";
  a.detail.name = "d";

  svc::resolved_fault b = a;
  CHECK(a == b);

  b.name = "F2";
  CHECK_FALSE(a == b);
}

// -- resolved_operation -------------------------------------------------------

TEST_CASE("service model: resolved_operation default construction",
          "[service_model]") {
  svc::resolved_operation op;
  CHECK(op.name.empty());
  CHECK(op.soap_action.empty());
  CHECK(op.style == xb::wsdl::binding_style::document);
  CHECK(op.use == xb::wsdl::body_use::literal);
  CHECK(op.rpc_namespace.empty());
  CHECK(op.input.empty());
  CHECK(op.output.empty());
  CHECK(op.one_way == false);
  CHECK(op.faults.empty());
}

TEST_CASE("service model: resolved_operation request-response",
          "[service_model]") {
  svc::resolved_part in_part;
  in_part.name = "parameters";
  in_part.xsd_name = xb::qname("urn:ex", "Req");
  in_part.is_element = true;
  in_part.cpp_type = "ex::req";
  in_part.read_function = "ex::read_req";
  in_part.write_function = "ex::write_req";

  svc::resolved_part out_part;
  out_part.name = "parameters";
  out_part.xsd_name = xb::qname("urn:ex", "Resp");
  out_part.is_element = true;
  out_part.cpp_type = "ex::resp";
  out_part.read_function = "ex::read_resp";
  out_part.write_function = "ex::write_resp";

  svc::resolved_operation op;
  op.name = "GetPrice";
  op.soap_action = "http://example.org/GetPrice";
  op.style = xb::wsdl::binding_style::document;
  op.use = xb::wsdl::body_use::literal;
  op.input.push_back(in_part);
  op.output.push_back(out_part);
  op.one_way = false;

  CHECK(op.name == "GetPrice");
  CHECK(op.soap_action == "http://example.org/GetPrice");
  CHECK(op.input.size() == 1);
  CHECK(op.output.size() == 1);
  CHECK(op.one_way == false);
}

TEST_CASE("service model: resolved_operation one-way", "[service_model]") {
  svc::resolved_operation op;
  op.name = "Notify";
  op.one_way = true;

  CHECK(op.output.empty());
  CHECK(op.one_way == true);
}

TEST_CASE("service model: resolved_operation with faults", "[service_model]") {
  svc::resolved_fault fault;
  fault.name = "ServerFault";
  fault.detail.name = "detail";
  fault.detail.cpp_type = "ex::fault_detail";

  svc::resolved_operation op;
  op.name = "GetPrice";
  op.faults.push_back(fault);

  CHECK(op.faults.size() == 1);
  CHECK(op.faults[0].name == "ServerFault");
}

TEST_CASE("service model: resolved_operation equality", "[service_model]") {
  svc::resolved_operation a;
  a.name = "Op";
  a.soap_action = "urn:action";

  svc::resolved_operation b = a;
  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- resolved_port ------------------------------------------------------------

TEST_CASE("service model: resolved_port default construction",
          "[service_model]") {
  svc::resolved_port p;
  CHECK(p.name.empty());
  CHECK(p.address.empty());
  CHECK(p.operations.empty());
}

TEST_CASE("service model: resolved_port with values", "[service_model]") {
  svc::resolved_operation op;
  op.name = "GetPrice";
  op.soap_action = "urn:GetPrice";

  svc::resolved_port port;
  port.name = "StockQuotePort";
  port.address = "http://example.org/stockquote";
  port.operations.push_back(op);

  CHECK(port.name == "StockQuotePort");
  CHECK(port.address == "http://example.org/stockquote");
  CHECK(port.operations.size() == 1);
}

TEST_CASE("service model: resolved_port equality", "[service_model]") {
  svc::resolved_port a;
  a.name = "Port";
  a.address = "http://example.org";

  svc::resolved_port b = a;
  CHECK(a == b);

  b.address = "http://other.org";
  CHECK_FALSE(a == b);
}

// -- resolved_service ---------------------------------------------------------

TEST_CASE("service model: resolved_service default construction",
          "[service_model]") {
  svc::resolved_service s;
  CHECK(s.name.empty());
  CHECK(s.target_namespace.empty());
  CHECK(s.ports.empty());
}

TEST_CASE("service model: resolved_service with values", "[service_model]") {
  svc::resolved_port port;
  port.name = "StockQuotePort";
  port.address = "http://example.org/stockquote";

  svc::resolved_service svc_val;
  svc_val.name = "StockQuoteService";
  svc_val.target_namespace = "urn:example:stockquote";
  svc_val.ports.push_back(port);

  CHECK(svc_val.name == "StockQuoteService");
  CHECK(svc_val.target_namespace == "urn:example:stockquote");
  CHECK(svc_val.ports.size() == 1);
}

TEST_CASE("service model: resolved_service equality", "[service_model]") {
  svc::resolved_service a;
  a.name = "Svc";
  a.target_namespace = "urn:ex";

  svc::resolved_service b = a;
  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- service_description ------------------------------------------------------

TEST_CASE("service model: service_description default construction",
          "[service_model]") {
  svc::service_description desc;
  CHECK(desc.services.empty());
}

TEST_CASE("service model: service_description with services",
          "[service_model]") {
  svc::resolved_service s;
  s.name = "Svc";
  s.target_namespace = "urn:ex";

  svc::service_description desc;
  desc.services.push_back(s);

  CHECK(desc.services.size() == 1);
  CHECK(desc.services[0].name == "Svc");
}

TEST_CASE("service model: service_description equality", "[service_model]") {
  svc::service_description a;
  svc::resolved_service s;
  s.name = "Svc";
  a.services.push_back(s);

  svc::service_description b = a;
  CHECK(a == b);

  b.services[0].name = "Other";
  CHECK_FALSE(a == b);
}
