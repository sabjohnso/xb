#include <xb/wsdl_resolver.hpp>

#include <catch2/catch_test_macros.hpp>

namespace svc = xb::service;
namespace wsdl = xb::wsdl;

// -- Helper: build a minimal WSDL document for testing ------------------------

static wsdl::document
make_doc_lit_wsdl() {
  wsdl::document doc;
  doc.name = "StockQuote";
  doc.target_namespace = "urn:example:stockquote";

  // Messages
  wsdl::message in_msg;
  in_msg.name = "GetPriceRequest";
  wsdl::part in_part;
  in_part.name = "parameters";
  in_part.ref =
      wsdl::part_by_element{xb::qname("urn:example:stockquote", "GetPrice")};
  in_msg.parts.push_back(in_part);
  doc.messages.push_back(in_msg);

  wsdl::message out_msg;
  out_msg.name = "GetPriceResponse";
  wsdl::part out_part;
  out_part.name = "parameters";
  out_part.ref = wsdl::part_by_element{
      xb::qname("urn:example:stockquote", "GetPriceResult")};
  out_msg.parts.push_back(out_part);
  doc.messages.push_back(out_msg);

  // PortType
  wsdl::port_type pt;
  pt.name = "StockQuotePortType";
  wsdl::operation op;
  op.name = "GetPrice";
  op.input_message = xb::qname("urn:example:stockquote", "GetPriceRequest");
  op.output_message = xb::qname("urn:example:stockquote", "GetPriceResponse");
  pt.operations.push_back(op);
  doc.port_types.push_back(pt);

  // Binding (document/literal)
  wsdl::binding binding;
  binding.name = "StockQuoteBinding";
  binding.port_type = xb::qname("urn:example:stockquote", "StockQuotePortType");
  binding.soap.style = wsdl::binding_style::document;
  binding.soap.transport = "http://schemas.xmlsoap.org/soap/http";

  wsdl::binding_operation bop;
  bop.name = "GetPrice";
  bop.soap_op.soap_action = "http://example.org/GetPrice";
  bop.input_body.use = wsdl::body_use::literal;
  bop.output_body.use = wsdl::body_use::literal;
  binding.operations.push_back(bop);
  doc.bindings.push_back(binding);

  // Service
  wsdl::service svc;
  svc.name = "StockQuoteService";
  wsdl::port port;
  port.name = "StockQuotePort";
  port.binding = xb::qname("urn:example:stockquote", "StockQuoteBinding");
  port.address = "http://example.org/stockquote";
  svc.ports.push_back(port);
  doc.services.push_back(svc);

  return doc;
}

static wsdl::document
make_rpc_lit_wsdl() {
  wsdl::document doc;
  doc.name = "DataService";
  doc.target_namespace = "urn:example:data";

  // Message with type= parts (RPC style)
  wsdl::message in_msg;
  in_msg.name = "GetDataInput";
  wsdl::part p;
  p.name = "ticker";
  p.ref = wsdl::part_by_type{
      xb::qname("http://www.w3.org/2001/XMLSchema", "string")};
  in_msg.parts.push_back(p);
  doc.messages.push_back(in_msg);

  wsdl::message out_msg;
  out_msg.name = "GetDataOutput";
  wsdl::part op;
  op.name = "result";
  op.ref = wsdl::part_by_type{
      xb::qname("http://www.w3.org/2001/XMLSchema", "string")};
  out_msg.parts.push_back(op);
  doc.messages.push_back(out_msg);

  // PortType
  wsdl::port_type pt;
  pt.name = "DataPortType";
  wsdl::operation wop;
  wop.name = "GetData";
  wop.input_message = xb::qname("urn:example:data", "GetDataInput");
  wop.output_message = xb::qname("urn:example:data", "GetDataOutput");
  pt.operations.push_back(wop);
  doc.port_types.push_back(pt);

  // Binding (rpc/literal)
  wsdl::binding binding;
  binding.name = "DataBinding";
  binding.port_type = xb::qname("urn:example:data", "DataPortType");
  binding.soap.style = wsdl::binding_style::rpc;
  binding.soap.transport = "http://schemas.xmlsoap.org/soap/http";

  wsdl::binding_operation bop;
  bop.name = "GetData";
  bop.soap_op.soap_action = "urn:GetData";
  bop.input_body.use = wsdl::body_use::literal;
  bop.input_body.namespace_uri = "urn:example:data";
  bop.output_body.use = wsdl::body_use::literal;
  bop.output_body.namespace_uri = "urn:example:data";
  binding.operations.push_back(bop);
  doc.bindings.push_back(binding);

  // Service
  wsdl::service svc;
  svc.name = "DataService";
  wsdl::port port;
  port.name = "DataPort";
  port.binding = xb::qname("urn:example:data", "DataBinding");
  port.address = "http://example.org/data";
  svc.ports.push_back(port);
  doc.services.push_back(svc);

  return doc;
}

static wsdl::document
make_oneway_wsdl() {
  wsdl::document doc;
  doc.name = "NotifyService";
  doc.target_namespace = "urn:example:notify";

  wsdl::message in_msg;
  in_msg.name = "NotifyInput";
  wsdl::part p;
  p.name = "parameters";
  p.ref = wsdl::part_by_element{xb::qname("urn:example:notify", "Notify")};
  in_msg.parts.push_back(p);
  doc.messages.push_back(in_msg);

  wsdl::port_type pt;
  pt.name = "NotifyPortType";
  wsdl::operation op;
  op.name = "Notify";
  op.input_message = xb::qname("urn:example:notify", "NotifyInput");
  // no output_message = one-way
  pt.operations.push_back(op);
  doc.port_types.push_back(pt);

  wsdl::binding binding;
  binding.name = "NotifyBinding";
  binding.port_type = xb::qname("urn:example:notify", "NotifyPortType");
  binding.soap.style = wsdl::binding_style::document;

  wsdl::binding_operation bop;
  bop.name = "Notify";
  bop.soap_op.soap_action = "urn:Notify";
  bop.input_body.use = wsdl::body_use::literal;
  binding.operations.push_back(bop);
  doc.bindings.push_back(binding);

  wsdl::service svc;
  svc.name = "NotifyService";
  wsdl::port port;
  port.name = "NotifyPort";
  port.binding = xb::qname("urn:example:notify", "NotifyBinding");
  port.address = "http://example.org/notify";
  svc.ports.push_back(port);
  doc.services.push_back(svc);

  return doc;
}

static wsdl::document
make_fault_wsdl() {
  wsdl::document doc;
  doc.name = "FaultService";
  doc.target_namespace = "urn:example:fault";

  wsdl::message in_msg;
  in_msg.name = "DoWorkInput";
  wsdl::part ip;
  ip.name = "parameters";
  ip.ref = wsdl::part_by_element{xb::qname("urn:example:fault", "DoWork")};
  in_msg.parts.push_back(ip);
  doc.messages.push_back(in_msg);

  wsdl::message out_msg;
  out_msg.name = "DoWorkOutput";
  wsdl::part op;
  op.name = "parameters";
  op.ref =
      wsdl::part_by_element{xb::qname("urn:example:fault", "DoWorkResult")};
  out_msg.parts.push_back(op);
  doc.messages.push_back(out_msg);

  wsdl::message fault_msg;
  fault_msg.name = "DoWorkFault";
  wsdl::part fp;
  fp.name = "detail";
  fp.ref = wsdl::part_by_element{xb::qname("urn:example:fault", "FaultDetail")};
  fault_msg.parts.push_back(fp);
  doc.messages.push_back(fault_msg);

  wsdl::port_type pt;
  pt.name = "FaultPortType";
  wsdl::operation wop;
  wop.name = "DoWork";
  wop.input_message = xb::qname("urn:example:fault", "DoWorkInput");
  wop.output_message = xb::qname("urn:example:fault", "DoWorkOutput");
  wsdl::fault_ref fr;
  fr.name = "ServerFault";
  fr.message = xb::qname("urn:example:fault", "DoWorkFault");
  wop.faults.push_back(fr);
  pt.operations.push_back(wop);
  doc.port_types.push_back(pt);

  wsdl::binding binding;
  binding.name = "FaultBinding";
  binding.port_type = xb::qname("urn:example:fault", "FaultPortType");
  binding.soap.style = wsdl::binding_style::document;

  wsdl::binding_operation bop;
  bop.name = "DoWork";
  bop.soap_op.soap_action = "urn:DoWork";
  bop.input_body.use = wsdl::body_use::literal;
  bop.output_body.use = wsdl::body_use::literal;
  binding.operations.push_back(bop);
  doc.bindings.push_back(binding);

  wsdl::service svc;
  svc.name = "FaultService";
  wsdl::port port;
  port.name = "FaultPort";
  port.binding = xb::qname("urn:example:fault", "FaultBinding");
  port.address = "http://example.org/fault";
  svc.ports.push_back(port);
  doc.services.push_back(svc);

  return doc;
}

// -- Tests --------------------------------------------------------------------

TEST_CASE("wsdl resolver: empty document produces empty description",
          "[wsdl_resolver]") {
  wsdl::document doc;
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  CHECK(desc.services.empty());
}

TEST_CASE("wsdl resolver: doc/lit operation resolves correctly",
          "[wsdl_resolver]") {
  auto doc = make_doc_lit_wsdl();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  REQUIRE(desc.services.size() == 1);

  const auto& svc = desc.services[0];
  CHECK(svc.name == "StockQuoteService");
  CHECK(svc.target_namespace == "urn:example:stockquote");
  REQUIRE(svc.ports.size() == 1);

  const auto& port = svc.ports[0];
  CHECK(port.name == "StockQuotePort");
  CHECK(port.address == "http://example.org/stockquote");
  REQUIRE(port.operations.size() == 1);

  const auto& op = port.operations[0];
  CHECK(op.name == "GetPrice");
  CHECK(op.soap_action == "http://example.org/GetPrice");
  CHECK(op.style == wsdl::binding_style::document);
  CHECK(op.use == wsdl::body_use::literal);
  CHECK(op.one_way == false);
  REQUIRE(op.input.size() == 1);
  REQUIRE(op.output.size() == 1);

  CHECK(op.input[0].name == "parameters");
  CHECK(op.input[0].xsd_name ==
        xb::qname("urn:example:stockquote", "GetPrice"));
  CHECK(op.input[0].is_element == true);

  CHECK(op.output[0].name == "parameters");
  CHECK(op.output[0].xsd_name ==
        xb::qname("urn:example:stockquote", "GetPriceResult"));
  CHECK(op.output[0].is_element == true);
}

TEST_CASE("wsdl resolver: rpc/lit operation resolves correctly",
          "[wsdl_resolver]") {
  auto doc = make_rpc_lit_wsdl();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  REQUIRE(desc.services.size() == 1);
  REQUIRE(desc.services[0].ports.size() == 1);
  REQUIRE(desc.services[0].ports[0].operations.size() == 1);

  const auto& op = desc.services[0].ports[0].operations[0];
  CHECK(op.name == "GetData");
  CHECK(op.style == wsdl::binding_style::rpc);
  CHECK(op.use == wsdl::body_use::literal);
  CHECK(op.rpc_namespace == "urn:example:data");
  CHECK(op.one_way == false);

  REQUIRE(op.input.size() == 1);
  CHECK(op.input[0].name == "ticker");
  CHECK(op.input[0].is_element == false);
  CHECK(op.input[0].cpp_type == "std::string");

  REQUIRE(op.output.size() == 1);
  CHECK(op.output[0].name == "result");
  CHECK(op.output[0].is_element == false);
  CHECK(op.output[0].cpp_type == "std::string");
}

TEST_CASE("wsdl resolver: one-way operation", "[wsdl_resolver]") {
  auto doc = make_oneway_wsdl();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  REQUIRE(desc.services.size() == 1);
  REQUIRE(desc.services[0].ports.size() == 1);
  REQUIRE(desc.services[0].ports[0].operations.size() == 1);

  const auto& op = desc.services[0].ports[0].operations[0];
  CHECK(op.name == "Notify");
  CHECK(op.one_way == true);
  CHECK(op.output.empty());
  CHECK(op.input.size() == 1);
}

TEST_CASE("wsdl resolver: operation with faults", "[wsdl_resolver]") {
  auto doc = make_fault_wsdl();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  REQUIRE(desc.services.size() == 1);
  REQUIRE(desc.services[0].ports.size() == 1);
  REQUIRE(desc.services[0].ports[0].operations.size() == 1);

  const auto& op = desc.services[0].ports[0].operations[0];
  REQUIRE(op.faults.size() == 1);
  CHECK(op.faults[0].name == "ServerFault");
  CHECK(op.faults[0].detail.name == "detail");
  CHECK(op.faults[0].detail.xsd_name ==
        xb::qname("urn:example:fault", "FaultDetail"));
  CHECK(op.faults[0].detail.is_element == true);
}

TEST_CASE("wsdl resolver: style override on binding_operation",
          "[wsdl_resolver]") {
  auto doc = make_doc_lit_wsdl();
  // Override the operation style to rpc
  doc.bindings[0].operations[0].soap_op.style_override =
      wsdl::binding_style::rpc;
  doc.bindings[0].operations[0].input_body.namespace_uri =
      "urn:example:stockquote";

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  REQUIRE(desc.services.size() == 1);
  REQUIRE(desc.services[0].ports[0].operations.size() == 1);

  const auto& op = desc.services[0].ports[0].operations[0];
  CHECK(op.style == wsdl::binding_style::rpc);
}

TEST_CASE("wsdl resolver: missing message reference throws",
          "[wsdl_resolver]") {
  auto doc = make_doc_lit_wsdl();
  // Break the input message reference
  doc.port_types[0].operations[0].input_message =
      xb::qname("urn:example:stockquote", "NonExistent");

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  CHECK_THROWS_AS(resolver.resolve(doc, types), std::runtime_error);
}

TEST_CASE("wsdl resolver: missing binding reference throws",
          "[wsdl_resolver]") {
  auto doc = make_doc_lit_wsdl();
  // Break the binding reference
  doc.services[0].ports[0].binding =
      xb::qname("urn:example:stockquote", "NonExistentBinding");

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  CHECK_THROWS_AS(resolver.resolve(doc, types), std::runtime_error);
}

TEST_CASE("wsdl resolver: XSD built-in type part resolves via type_map",
          "[wsdl_resolver]") {
  auto doc = make_rpc_lit_wsdl();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl_resolver resolver;

  auto desc = resolver.resolve(doc, types);
  const auto& op = desc.services[0].ports[0].operations[0];

  // "string" from XSD schema maps to "std::string" via default type_map
  CHECK(op.input[0].cpp_type == "std::string");
  CHECK(op.output[0].cpp_type == "std::string");
}
