#include <xb/wsdl2_resolver.hpp>

#include <xb/soap_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace svc = xb::service;
namespace w2 = xb::wsdl2;

// -- Helpers ------------------------------------------------------------------

static w2::description
make_simple_inout_desc() {
  w2::description desc;
  desc.target_namespace = "urn:example:stockquote";

  w2::operation op;
  op.name = "GetPrice";
  op.pattern = w2::mep::in_out;
  op.input_element = xb::qname("urn:example:stockquote", "GetPriceRequest");
  op.output_element = xb::qname("urn:example:stockquote", "GetPriceResponse");

  w2::interface iface;
  iface.name = "StockQuoteInterface";
  iface.operations.push_back(op);
  desc.interfaces.push_back(iface);

  w2::soap_binding_info soap;
  soap.protocol = "http://www.w3.org/2003/05/soap/bindings/HTTP/";

  w2::binding_operation bop;
  bop.ref = xb::qname("urn:example:stockquote", "GetPrice");
  bop.soap.soap_action = "http://example.org/GetPrice";

  w2::binding b;
  b.name = "StockQuoteBinding";
  b.interface_ref = xb::qname("urn:example:stockquote", "StockQuoteInterface");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  b.soap = soap;
  b.operations.push_back(bop);
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "StockQuoteEndpoint";
  ep.binding_ref = xb::qname("urn:example:stockquote", "StockQuoteBinding");
  ep.address = "http://example.org/stockquote";

  w2::service svc;
  svc.name = "StockQuoteService";
  svc.interface_ref =
      xb::qname("urn:example:stockquote", "StockQuoteInterface");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  return desc;
}

// -- Tests --------------------------------------------------------------------

// 1. Empty description

TEST_CASE("wsdl2 resolver: empty description", "[wsdl2_resolver]") {
  w2::description desc;
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  CHECK(result.services.empty());
}

// 2. Simple in-out operation resolves correctly

TEST_CASE("wsdl2 resolver: simple in-out operation", "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  REQUIRE(result.services.size() == 1);

  const auto& svc = result.services[0];
  CHECK(svc.name == "StockQuoteService");
  CHECK(svc.target_namespace == "urn:example:stockquote");
  REQUIRE(svc.ports.size() == 1);

  const auto& port = svc.ports[0];
  CHECK(port.name == "StockQuoteEndpoint");
  CHECK(port.address == "http://example.org/stockquote");
  REQUIRE(port.operations.size() == 1);

  const auto& op = port.operations[0];
  CHECK(op.name == "GetPrice");
  CHECK(op.soap_action == "http://example.org/GetPrice");
  CHECK(op.style == xb::wsdl::binding_style::document);
  CHECK(op.use == xb::wsdl::body_use::literal);
  CHECK(op.one_way == false);
  REQUIRE(op.input.size() == 1);
  REQUIRE(op.output.size() == 1);
  CHECK(op.input[0].is_element == true);
  CHECK(op.output[0].is_element == true);
}

// 3. In-only operation

TEST_CASE("wsdl2 resolver: in-only operation", "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();
  desc.interfaces[0].operations[0].pattern = w2::mep::in_only;
  desc.interfaces[0].operations[0].output_element.reset();

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& op = result.services[0].ports[0].operations[0];
  CHECK(op.one_way == true);
  CHECK(op.output.empty());
}

// 4. Operation with faults

TEST_CASE("wsdl2 resolver: operation with faults", "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();

  w2::interface_fault ifault;
  ifault.name = "InvalidTicker";
  ifault.element = xb::qname("urn:example:stockquote", "InvalidTickerDetail");
  desc.interfaces[0].faults.push_back(ifault);

  w2::infault_ref inf;
  inf.ref = xb::qname("urn:example:stockquote", "InvalidTicker");
  desc.interfaces[0].operations[0].infaults.push_back(inf);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& op = result.services[0].ports[0].operations[0];
  REQUIRE(op.faults.size() == 1);
  CHECK(op.faults[0].name == "InvalidTicker");
  CHECK(op.faults[0].detail.is_element == true);
}

// 5. Interface inheritance: child + parent operations

TEST_CASE("wsdl2 resolver: interface inheritance", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  w2::operation parent_op;
  parent_op.name = "ParentOp";
  parent_op.pattern = w2::mep::in_out;
  parent_op.input_element = xb::qname("urn:example", "ParentReq");
  parent_op.output_element = xb::qname("urn:example", "ParentResp");

  w2::interface parent;
  parent.name = "ParentInterface";
  parent.operations.push_back(parent_op);
  desc.interfaces.push_back(parent);

  w2::operation child_op;
  child_op.name = "ChildOp";
  child_op.pattern = w2::mep::in_out;
  child_op.input_element = xb::qname("urn:example", "ChildReq");
  child_op.output_element = xb::qname("urn:example", "ChildResp");

  w2::interface child;
  child.name = "ChildInterface";
  child.extends.push_back(xb::qname("urn:example", "ParentInterface"));
  child.operations.push_back(child_op);
  desc.interfaces.push_back(child);

  w2::binding b;
  b.name = "ChildBinding";
  b.interface_ref = xb::qname("urn:example", "ChildInterface");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "Endpoint";
  ep.binding_ref = xb::qname("urn:example", "ChildBinding");
  ep.address = "http://example.org/child";

  w2::service svc;
  svc.name = "Svc";
  svc.interface_ref = xb::qname("urn:example", "ChildInterface");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& port = result.services[0].ports[0];
  // Should have both parent and child operations
  REQUIRE(port.operations.size() == 2);

  // Parent op first, then child op (parent's operations are prepended)
  bool has_parent = false, has_child = false;
  for (const auto& op : port.operations) {
    if (op.name == "ParentOp") has_parent = true;
    if (op.name == "ChildOp") has_child = true;
  }
  CHECK(has_parent);
  CHECK(has_child);
}

// 6. Inheritance override: child replaces parent's same-name operation

TEST_CASE("wsdl2 resolver: inheritance override", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  w2::operation parent_op;
  parent_op.name = "SharedOp";
  parent_op.pattern = w2::mep::in_out;
  parent_op.input_element = xb::qname("urn:example", "ParentReq");
  parent_op.output_element = xb::qname("urn:example", "ParentResp");

  w2::interface parent;
  parent.name = "Parent";
  parent.operations.push_back(parent_op);
  desc.interfaces.push_back(parent);

  w2::operation child_op;
  child_op.name = "SharedOp"; // Same name — overrides parent
  child_op.pattern = w2::mep::in_out;
  child_op.input_element = xb::qname("urn:example", "ChildReq");
  child_op.output_element = xb::qname("urn:example", "ChildResp");

  w2::interface child;
  child.name = "Child";
  child.extends.push_back(xb::qname("urn:example", "Parent"));
  child.operations.push_back(child_op);
  desc.interfaces.push_back(child);

  w2::binding b;
  b.name = "B";
  b.interface_ref = xb::qname("urn:example", "Child");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "E";
  ep.binding_ref = xb::qname("urn:example", "B");
  ep.address = "http://example.org";

  w2::service svc;
  svc.name = "S";
  svc.interface_ref = xb::qname("urn:example", "Child");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& port = result.services[0].ports[0];
  // Only one operation (child overrides parent)
  REQUIRE(port.operations.size() == 1);
  CHECK(port.operations[0].input[0].xsd_name ==
        xb::qname("urn:example", "ChildReq"));
}

// 7. Chain inheritance (A extends B extends C)

TEST_CASE("wsdl2 resolver: chain inheritance", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  auto make_iface = [](const std::string& name, const std::string& op_name,
                       const std::string& ns) {
    w2::operation op;
    op.name = op_name;
    op.pattern = w2::mep::in_out;
    op.input_element = xb::qname(ns, op_name + "Req");
    op.output_element = xb::qname(ns, op_name + "Resp");

    w2::interface iface;
    iface.name = name;
    iface.operations.push_back(op);
    return iface;
  };

  auto grandparent = make_iface("GrandParent", "OpA", "urn:example");
  desc.interfaces.push_back(grandparent);

  auto parent = make_iface("Parent", "OpB", "urn:example");
  parent.extends.push_back(xb::qname("urn:example", "GrandParent"));
  desc.interfaces.push_back(parent);

  auto child = make_iface("Child", "OpC", "urn:example");
  child.extends.push_back(xb::qname("urn:example", "Parent"));
  desc.interfaces.push_back(child);

  w2::binding b;
  b.name = "B";
  b.interface_ref = xb::qname("urn:example", "Child");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "E";
  ep.binding_ref = xb::qname("urn:example", "B");
  ep.address = "http://example.org";

  w2::service svc;
  svc.name = "S";
  svc.interface_ref = xb::qname("urn:example", "Child");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& port = result.services[0].ports[0];
  REQUIRE(port.operations.size() == 3);
}

// 8. Circular inheritance throws

TEST_CASE("wsdl2 resolver: circular inheritance throws", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  w2::interface a;
  a.name = "A";
  a.extends.push_back(xb::qname("urn:example", "B"));
  desc.interfaces.push_back(a);

  w2::interface b;
  b.name = "B";
  b.extends.push_back(xb::qname("urn:example", "A"));
  desc.interfaces.push_back(b);

  w2::binding bind;
  bind.name = "Bind";
  bind.interface_ref = xb::qname("urn:example", "A");
  bind.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(bind);

  w2::endpoint ep;
  ep.name = "E";
  ep.binding_ref = xb::qname("urn:example", "Bind");
  ep.address = "http://example.org";

  w2::service svc;
  svc.name = "S";
  svc.interface_ref = xb::qname("urn:example", "A");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  CHECK_THROWS_AS(resolver.resolve(desc, types), std::runtime_error);
}

// 9. SOAP version from protocol URI

TEST_CASE("wsdl2 resolver: SOAP 1.2 from protocol", "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();
  // Default protocol is HTTP = SOAP 1.2
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  CHECK(result.services[0].ports[0].soap_ver == xb::soap::soap_version::v1_2);
}

// 10. Missing interface reference throws

TEST_CASE("wsdl2 resolver: missing interface throws", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  w2::binding b;
  b.name = "B";
  b.interface_ref = xb::qname("urn:example", "NonExistent");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "E";
  ep.binding_ref = xb::qname("urn:example", "B");
  ep.address = "http://example.org";

  w2::service svc;
  svc.name = "S";
  svc.interface_ref = xb::qname("urn:example", "NonExistent");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  CHECK_THROWS_AS(resolver.resolve(desc, types), std::runtime_error);
}

// 11. Explicit wsoap:version overrides protocol heuristic

TEST_CASE("wsdl2 resolver: explicit soap version from binding",
          "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();
  // Protocol looks like SOAP 1.2 (HTTP), but version explicitly says 1.1
  desc.bindings[0].soap.version = "1.1";

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  CHECK(result.services[0].ports[0].soap_ver == xb::soap::soap_version::v1_1);
}

// 12. Binding without interface attribute produces port with no operations

TEST_CASE("wsdl2 resolver: binding without interface", "[wsdl2_resolver]") {
  w2::description desc;
  desc.target_namespace = "urn:example";

  w2::binding b;
  b.name = "ReusableBinding";
  // No interface_ref set — reusable binding per spec
  b.type = "http://www.w3.org/ns/wsdl/soap";
  desc.bindings.push_back(b);

  w2::endpoint ep;
  ep.name = "E";
  ep.binding_ref = xb::qname("urn:example", "ReusableBinding");
  ep.address = "http://example.org";

  w2::service svc;
  svc.name = "S";
  svc.interface_ref = xb::qname("urn:example", "SomeInterface");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& port = result.services[0].ports[0];
  CHECK(port.name == "E");
  CHECK(port.operations.empty());
}

// 13. Style always document, use always literal

TEST_CASE("wsdl2 resolver: style=document, use=literal", "[wsdl2_resolver]") {
  auto desc = make_simple_inout_desc();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;

  auto result = resolver.resolve(desc, types);
  const auto& op = result.services[0].ports[0].operations[0];
  CHECK(op.style == xb::wsdl::binding_style::document);
  CHECK(op.use == xb::wsdl::body_use::literal);
}
