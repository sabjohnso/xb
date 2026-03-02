#include <xb/wsdl_model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace wsdl = xb::wsdl;

// -- part_by_element ----------------------------------------------------------

TEST_CASE("wsdl model: part_by_element construction and equality",
          "[wsdl_model]") {
  wsdl::part_by_element a;
  a.element = xb::qname("urn:ex", "GetPrice");

  wsdl::part_by_element b;
  b.element = xb::qname("urn:ex", "GetPrice");

  CHECK(a == b);

  b.element = xb::qname("urn:ex", "SetPrice");
  CHECK_FALSE(a == b);
}

// -- part_by_type -------------------------------------------------------------

TEST_CASE("wsdl model: part_by_type construction and equality",
          "[wsdl_model]") {
  wsdl::part_by_type a;
  a.type = xb::qname("http://www.w3.org/2001/XMLSchema", "string");

  wsdl::part_by_type b;
  b.type = xb::qname("http://www.w3.org/2001/XMLSchema", "string");

  CHECK(a == b);

  b.type = xb::qname("http://www.w3.org/2001/XMLSchema", "int");
  CHECK_FALSE(a == b);
}

// -- part ---------------------------------------------------------------------

TEST_CASE("wsdl model: part with element ref", "[wsdl_model]") {
  wsdl::part p;
  p.name = "parameters";
  p.ref = wsdl::part_by_element{xb::qname("urn:ex", "GetPriceRequest")};

  CHECK(p.name == "parameters");
  CHECK(std::holds_alternative<wsdl::part_by_element>(p.ref));
  CHECK(std::get<wsdl::part_by_element>(p.ref).element ==
        xb::qname("urn:ex", "GetPriceRequest"));
}

TEST_CASE("wsdl model: part with type ref", "[wsdl_model]") {
  wsdl::part p;
  p.name = "body";
  p.ref = wsdl::part_by_type{
      xb::qname("http://www.w3.org/2001/XMLSchema", "string")};

  CHECK(std::holds_alternative<wsdl::part_by_type>(p.ref));
}

TEST_CASE("wsdl model: part equality", "[wsdl_model]") {
  wsdl::part a;
  a.name = "parameters";
  a.ref = wsdl::part_by_element{xb::qname("urn:ex", "Req")};

  wsdl::part b = a;
  CHECK(a == b);

  b.name = "other";
  CHECK_FALSE(a == b);
}

// -- message ------------------------------------------------------------------

TEST_CASE("wsdl model: message construction", "[wsdl_model]") {
  wsdl::message m;
  m.name = "GetPriceRequest";

  wsdl::part p1;
  p1.name = "parameters";
  p1.ref = wsdl::part_by_element{xb::qname("urn:ex", "GetPrice")};

  wsdl::part p2;
  p2.name = "extra";
  p2.ref =
      wsdl::part_by_type{xb::qname("http://www.w3.org/2001/XMLSchema", "int")};

  m.parts.push_back(p1);
  m.parts.push_back(p2);

  CHECK(m.name == "GetPriceRequest");
  CHECK(m.parts.size() == 2);
  CHECK(m.parts[0].name == "parameters");
  CHECK(m.parts[1].name == "extra");
}

TEST_CASE("wsdl model: message equality", "[wsdl_model]") {
  wsdl::message a;
  a.name = "Msg";

  wsdl::message b;
  b.name = "Msg";

  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- fault_ref ----------------------------------------------------------------

TEST_CASE("wsdl model: fault_ref construction and equality", "[wsdl_model]") {
  wsdl::fault_ref fr;
  fr.name = "ServerFault";
  fr.message = xb::qname("urn:ex", "FaultMessage");

  wsdl::fault_ref fr2 = fr;
  CHECK(fr == fr2);

  fr2.name = "OtherFault";
  CHECK_FALSE(fr == fr2);
}

// -- operation ----------------------------------------------------------------

TEST_CASE("wsdl model: operation request-response", "[wsdl_model]") {
  wsdl::operation op;
  op.name = "GetPrice";
  op.input_message = xb::qname("urn:ex", "GetPriceRequest");
  op.output_message = xb::qname("urn:ex", "GetPriceResponse");

  CHECK(op.name == "GetPrice");
  CHECK(op.input_message == xb::qname("urn:ex", "GetPriceRequest"));
  CHECK(op.output_message.has_value());
  CHECK(op.output_message.value() == xb::qname("urn:ex", "GetPriceResponse"));
  CHECK(op.faults.empty());
}

TEST_CASE("wsdl model: operation one-way (no output)", "[wsdl_model]") {
  wsdl::operation op;
  op.name = "Notify";
  op.input_message = xb::qname("urn:ex", "NotifyRequest");

  CHECK_FALSE(op.output_message.has_value());
}

TEST_CASE("wsdl model: operation with faults", "[wsdl_model]") {
  wsdl::operation op;
  op.name = "GetPrice";
  op.input_message = xb::qname("urn:ex", "GetPriceRequest");
  op.output_message = xb::qname("urn:ex", "GetPriceResponse");

  wsdl::fault_ref fr;
  fr.name = "ServerFault";
  fr.message = xb::qname("urn:ex", "FaultMessage");
  op.faults.push_back(fr);

  CHECK(op.faults.size() == 1);
  CHECK(op.faults[0].name == "ServerFault");
}

TEST_CASE("wsdl model: operation equality", "[wsdl_model]") {
  wsdl::operation a;
  a.name = "Op";
  a.input_message = xb::qname("urn:ex", "In");

  wsdl::operation b = a;
  CHECK(a == b);

  b.name = "OtherOp";
  CHECK_FALSE(a == b);
}

// -- port_type ----------------------------------------------------------------

TEST_CASE("wsdl model: port_type construction", "[wsdl_model]") {
  wsdl::port_type pt;
  pt.name = "StockQuotePortType";

  wsdl::operation op;
  op.name = "GetPrice";
  op.input_message = xb::qname("urn:ex", "GetPriceRequest");
  op.output_message = xb::qname("urn:ex", "GetPriceResponse");
  pt.operations.push_back(op);

  CHECK(pt.name == "StockQuotePortType");
  CHECK(pt.operations.size() == 1);
}

TEST_CASE("wsdl model: port_type equality", "[wsdl_model]") {
  wsdl::port_type a;
  a.name = "PT";

  wsdl::port_type b = a;
  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- binding_style / body_use enums -------------------------------------------

TEST_CASE("wsdl model: binding_style enum values", "[wsdl_model]") {
  CHECK(wsdl::binding_style::document != wsdl::binding_style::rpc);
}

TEST_CASE("wsdl model: body_use enum values", "[wsdl_model]") {
  CHECK(wsdl::body_use::literal != wsdl::body_use::encoded);
}

// -- soap_binding_ext ---------------------------------------------------------

TEST_CASE("wsdl model: soap_binding_ext defaults", "[wsdl_model]") {
  wsdl::soap_binding_ext sbe;
  CHECK(sbe.style == wsdl::binding_style::document);
  CHECK(sbe.transport.empty());
}

TEST_CASE("wsdl model: soap_binding_ext equality", "[wsdl_model]") {
  wsdl::soap_binding_ext a;
  a.style = wsdl::binding_style::rpc;
  a.transport = "http://schemas.xmlsoap.org/soap/http";

  wsdl::soap_binding_ext b = a;
  CHECK(a == b);

  b.style = wsdl::binding_style::document;
  CHECK_FALSE(a == b);
}

// -- soap_body_ext ------------------------------------------------------------

TEST_CASE("wsdl model: soap_body_ext defaults", "[wsdl_model]") {
  wsdl::soap_body_ext sbe;
  CHECK(sbe.use == wsdl::body_use::literal);
  CHECK(sbe.namespace_uri.empty());
}

TEST_CASE("wsdl model: soap_body_ext equality", "[wsdl_model]") {
  wsdl::soap_body_ext a;
  a.use = wsdl::body_use::encoded;
  a.namespace_uri = "urn:ex";

  wsdl::soap_body_ext b = a;
  CHECK(a == b);

  b.use = wsdl::body_use::literal;
  CHECK_FALSE(a == b);
}

// -- soap_operation_ext -------------------------------------------------------

TEST_CASE("wsdl model: soap_operation_ext defaults", "[wsdl_model]") {
  wsdl::soap_operation_ext soe;
  CHECK(soe.soap_action.empty());
  CHECK_FALSE(soe.style_override.has_value());
}

TEST_CASE("wsdl model: soap_operation_ext with style override",
          "[wsdl_model]") {
  wsdl::soap_operation_ext soe;
  soe.soap_action = "http://example.org/GetPrice";
  soe.style_override = wsdl::binding_style::rpc;

  CHECK(soe.soap_action == "http://example.org/GetPrice");
  CHECK(soe.style_override.has_value());
  CHECK(soe.style_override.value() == wsdl::binding_style::rpc);
}

TEST_CASE("wsdl model: soap_operation_ext equality", "[wsdl_model]") {
  wsdl::soap_operation_ext a;
  a.soap_action = "urn:action";

  wsdl::soap_operation_ext b = a;
  CHECK(a == b);

  b.soap_action = "urn:other";
  CHECK_FALSE(a == b);
}

// -- binding_operation --------------------------------------------------------

TEST_CASE("wsdl model: binding_operation construction", "[wsdl_model]") {
  wsdl::binding_operation bo;
  bo.name = "GetPrice";
  bo.soap_op.soap_action = "http://example.org/GetPrice";
  bo.input_body.use = wsdl::body_use::literal;
  bo.output_body.use = wsdl::body_use::literal;

  CHECK(bo.name == "GetPrice");
  CHECK(bo.soap_op.soap_action == "http://example.org/GetPrice");
}

TEST_CASE("wsdl model: binding_operation equality", "[wsdl_model]") {
  wsdl::binding_operation a;
  a.name = "Op";

  wsdl::binding_operation b = a;
  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- binding ------------------------------------------------------------------

TEST_CASE("wsdl model: binding construction", "[wsdl_model]") {
  wsdl::binding b;
  b.name = "StockQuoteBinding";
  b.port_type = xb::qname("urn:ex", "StockQuotePortType");
  b.soap.style = wsdl::binding_style::document;
  b.soap.transport = "http://schemas.xmlsoap.org/soap/http";

  wsdl::binding_operation bo;
  bo.name = "GetPrice";
  b.operations.push_back(bo);

  CHECK(b.name == "StockQuoteBinding");
  CHECK(b.port_type == xb::qname("urn:ex", "StockQuotePortType"));
  CHECK(b.operations.size() == 1);
}

TEST_CASE("wsdl model: binding equality", "[wsdl_model]") {
  wsdl::binding a;
  a.name = "B";

  wsdl::binding b = a;
  CHECK(a == b);

  b.name = "C";
  CHECK_FALSE(a == b);
}

// -- port ---------------------------------------------------------------------

TEST_CASE("wsdl model: port construction and equality", "[wsdl_model]") {
  wsdl::port p;
  p.name = "StockQuotePort";
  p.binding = xb::qname("urn:ex", "StockQuoteBinding");
  p.address = "http://example.org/stockquote";

  wsdl::port p2 = p;
  CHECK(p == p2);

  p2.address = "http://example.org/other";
  CHECK_FALSE(p == p2);
}

// -- service ------------------------------------------------------------------

TEST_CASE("wsdl model: service construction", "[wsdl_model]") {
  wsdl::service s;
  s.name = "StockQuoteService";

  wsdl::port p;
  p.name = "StockQuotePort";
  p.binding = xb::qname("urn:ex", "StockQuoteBinding");
  p.address = "http://example.org/stockquote";
  s.ports.push_back(p);

  CHECK(s.name == "StockQuoteService");
  CHECK(s.ports.size() == 1);
  CHECK(s.ports[0].name == "StockQuotePort");
}

TEST_CASE("wsdl model: service equality", "[wsdl_model]") {
  wsdl::service a;
  a.name = "Svc";

  wsdl::service b = a;
  CHECK(a == b);

  b.name = "Other";
  CHECK_FALSE(a == b);
}

// -- import -------------------------------------------------------------------

TEST_CASE("wsdl model: import construction and equality", "[wsdl_model]") {
  wsdl::import i;
  i.namespace_uri = "urn:ex";
  i.location = "http://example.org/ex.wsdl";

  wsdl::import i2 = i;
  CHECK(i == i2);

  i2.location = "http://other.org/ex.wsdl";
  CHECK_FALSE(i == i2);
}

// -- document (no operator==) -------------------------------------------------

TEST_CASE("wsdl model: document construction", "[wsdl_model]") {
  wsdl::document doc;
  doc.name = "StockQuote";
  doc.target_namespace = "urn:example:stockquote";

  CHECK(doc.name == "StockQuote");
  CHECK(doc.target_namespace == "urn:example:stockquote");
  CHECK(doc.imports.empty());
  CHECK(doc.messages.empty());
  CHECK(doc.port_types.empty());
  CHECK(doc.bindings.empty());
  CHECK(doc.services.empty());
}
