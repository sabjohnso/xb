#include <xb/wsdl2_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace w2 = xb::wsdl2;

// -- description --------------------------------------------------------------

TEST_CASE("wsdl2 model: description default construction", "[wsdl2_model]") {
  w2::description d;
  CHECK(d.target_namespace.empty());
  CHECK(d.imports.empty());
  CHECK(d.includes.empty());
  CHECK(d.interfaces.empty());
  CHECK(d.bindings.empty());
  CHECK(d.services.empty());
}

// -- import / include ---------------------------------------------------------

TEST_CASE("wsdl2 model: import with namespace and location", "[wsdl2_model]") {
  w2::import imp;
  imp.namespace_uri = "urn:example";
  imp.location = "example.wsdl";

  CHECK(imp.namespace_uri == "urn:example");
  CHECK(imp.location == "example.wsdl");
}

TEST_CASE("wsdl2 model: include with location", "[wsdl2_model]") {
  w2::include inc;
  inc.location = "types.wsdl";

  CHECK(inc.location == "types.wsdl");
}

// -- interface ----------------------------------------------------------------

TEST_CASE("wsdl2 model: interface with operations and faults",
          "[wsdl2_model]") {
  w2::interface_fault fault;
  fault.name = "InvalidTickerFault";
  fault.element = xb::qname("urn:ex", "InvalidTicker");

  w2::infault_ref infault;
  infault.ref = xb::qname("urn:ex", "InvalidTickerFault");
  infault.message_label = "In";

  w2::operation op;
  op.name = "GetPrice";
  op.pattern = w2::mep::in_out;
  op.input_element = xb::qname("urn:ex", "GetPriceRequest");
  op.output_element = xb::qname("urn:ex", "GetPriceResponse");
  op.infaults.push_back(infault);

  w2::interface iface;
  iface.name = "StockQuoteInterface";
  iface.faults.push_back(fault);
  iface.operations.push_back(op);

  CHECK(iface.name == "StockQuoteInterface");
  REQUIRE(iface.faults.size() == 1);
  CHECK(iface.faults[0].name == "InvalidTickerFault");
  REQUIRE(iface.operations.size() == 1);
  CHECK(iface.operations[0].name == "GetPrice");
  CHECK(iface.operations[0].pattern == w2::mep::in_out);
  CHECK(iface.operations[0].input_element.has_value());
  CHECK(iface.operations[0].output_element.has_value());
  CHECK(iface.operations[0].infaults.size() == 1);
}

TEST_CASE("wsdl2 model: interface inheritance via extends", "[wsdl2_model]") {
  w2::interface child;
  child.name = "ChildInterface";
  child.extends.push_back(xb::qname("urn:ex", "ParentInterface"));

  REQUIRE(child.extends.size() == 1);
  CHECK(child.extends[0] == xb::qname("urn:ex", "ParentInterface"));
}

TEST_CASE("wsdl2 model: in_only operation has no output", "[wsdl2_model]") {
  w2::operation op;
  op.name = "Notify";
  op.pattern = w2::mep::in_only;
  op.input_element = xb::qname("urn:ex", "NotifyEvent");

  CHECK(op.pattern == w2::mep::in_only);
  CHECK(op.input_element.has_value());
  CHECK_FALSE(op.output_element.has_value());
}

// -- binding ------------------------------------------------------------------

TEST_CASE("wsdl2 model: binding with SOAP extensions", "[wsdl2_model]") {
  w2::soap_binding_info soap;
  soap.protocol = "http://www.w3.org/2003/05/soap/bindings/HTTP/";
  soap.mep_default = "http://www.w3.org/2003/05/soap/mep/request-response";

  w2::binding_operation bop;
  bop.ref = xb::qname("urn:ex", "GetPrice");
  bop.soap.soap_action = "http://example.org/GetPrice";

  w2::binding b;
  b.name = "StockQuoteBinding";
  b.interface_ref = xb::qname("urn:ex", "StockQuoteInterface");
  b.type = "http://www.w3.org/ns/wsdl/soap";
  b.soap = soap;
  b.operations.push_back(bop);

  CHECK(b.name == "StockQuoteBinding");
  CHECK(b.interface_ref == xb::qname("urn:ex", "StockQuoteInterface"));
  CHECK(b.soap.protocol == "http://www.w3.org/2003/05/soap/bindings/HTTP/");
  REQUIRE(b.operations.size() == 1);
  CHECK(b.operations[0].soap.soap_action == "http://example.org/GetPrice");
}

TEST_CASE("wsdl2 model: binding fault with soap code", "[wsdl2_model]") {
  w2::binding_fault bf;
  bf.ref = xb::qname("urn:ex", "InvalidTickerFault");
  bf.soap_code = "soap:Sender";

  CHECK(bf.ref == xb::qname("urn:ex", "InvalidTickerFault"));
  CHECK(bf.soap_code == "soap:Sender");
}

// -- service ------------------------------------------------------------------

TEST_CASE("wsdl2 model: service with endpoint", "[wsdl2_model]") {
  w2::endpoint ep;
  ep.name = "StockQuoteEndpoint";
  ep.binding_ref = xb::qname("urn:ex", "StockQuoteBinding");
  ep.address = "http://example.org/stockquote";

  w2::service svc;
  svc.name = "StockQuoteService";
  svc.interface_ref = xb::qname("urn:ex", "StockQuoteInterface");
  svc.endpoints.push_back(ep);

  CHECK(svc.name == "StockQuoteService");
  CHECK(svc.interface_ref == xb::qname("urn:ex", "StockQuoteInterface"));
  REQUIRE(svc.endpoints.size() == 1);
  CHECK(svc.endpoints[0].name == "StockQuoteEndpoint");
  CHECK(svc.endpoints[0].address == "http://example.org/stockquote");
}

// -- interface_fault optional element
// ------------------------------------------

TEST_CASE("wsdl2 model: interface_fault element is optional", "[wsdl2_model]") {
  w2::interface_fault f;
  f.name = "MyFault";
  // No element set — defaults to #any per spec
  CHECK_FALSE(f.element.has_value());
}

// -- mep enum -----------------------------------------------------------------

TEST_CASE("wsdl2 model: mep enum values", "[wsdl2_model]") {
  CHECK(w2::mep::in_only != w2::mep::in_out);
  CHECK(w2::mep::robust_in_only != w2::mep::in_optional_out);
}

// -- description equality -----------------------------------------------------

TEST_CASE("wsdl2 model: description equality", "[wsdl2_model]") {
  w2::description a;
  a.target_namespace = "urn:ex";

  w2::description b;
  b.target_namespace = "urn:ex";
  CHECK(a == b);

  b.target_namespace = "urn:other";
  CHECK_FALSE(a == b);
}

// -- complete description -----------------------------------------------------

TEST_CASE("wsdl2 model: complete description", "[wsdl2_model]") {
  w2::description desc;
  desc.target_namespace = "urn:example:stockquote";

  w2::import imp;
  imp.namespace_uri = "urn:types";
  imp.location = "types.wsdl";
  desc.imports.push_back(imp);

  w2::interface_fault fault;
  fault.name = "InvalidTicker";
  fault.element = xb::qname("urn:example:stockquote", "InvalidTickerDetail");

  w2::operation op;
  op.name = "GetPrice";
  op.pattern = w2::mep::in_out;
  op.input_element = xb::qname("urn:example:stockquote", "GetPriceRequest");
  op.output_element = xb::qname("urn:example:stockquote", "GetPriceResponse");

  w2::interface iface;
  iface.name = "StockQuoteInterface";
  iface.faults.push_back(fault);
  iface.operations.push_back(op);
  desc.interfaces.push_back(iface);

  w2::soap_binding_info soap;
  soap.protocol = "http://www.w3.org/2003/05/soap/bindings/HTTP/";

  w2::binding_operation bop;
  bop.ref = xb::qname("urn:example:stockquote", "GetPrice");
  bop.soap.soap_action = "http://example.org/GetPrice";

  w2::binding bind;
  bind.name = "StockQuoteBinding";
  bind.interface_ref =
      xb::qname("urn:example:stockquote", "StockQuoteInterface");
  bind.type = "http://www.w3.org/ns/wsdl/soap";
  bind.soap = soap;
  bind.operations.push_back(bop);
  desc.bindings.push_back(bind);

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

  CHECK(desc.target_namespace == "urn:example:stockquote");
  CHECK(desc.imports.size() == 1);
  CHECK(desc.interfaces.size() == 1);
  CHECK(desc.bindings.size() == 1);
  CHECK(desc.services.size() == 1);
}
