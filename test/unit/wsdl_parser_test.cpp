#include <xb/expat_reader.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace wsdl = xb::wsdl;

static wsdl::document
parse_wsdl(const std::string& xml) {
  expat_reader reader(xml);
  wsdl_parser parser;
  return parser.parse(reader);
}

// -- Minimal definitions ------------------------------------------------------

TEST_CASE("wsdl parser: minimal definitions", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      name="StockQuote"
      targetNamespace="urn:example:stockquote">
    </wsdl:definitions>
  )");
  CHECK(doc.name == "StockQuote");
  CHECK(doc.target_namespace == "urn:example:stockquote");
  CHECK(doc.imports.empty());
  CHECK(doc.messages.empty());
  CHECK(doc.port_types.empty());
  CHECK(doc.bindings.empty());
  CHECK(doc.services.empty());
}

// -- Import -------------------------------------------------------------------

TEST_CASE("wsdl parser: import recorded", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      name="Test"
      targetNamespace="urn:test">
      <wsdl:import namespace="urn:other" location="other.wsdl"/>
    </wsdl:definitions>
  )");
  REQUIRE(doc.imports.size() == 1);
  CHECK(doc.imports[0].namespace_uri == "urn:other");
  CHECK(doc.imports[0].location == "other.wsdl");
}

// -- Message (document/element style) -----------------------------------------

TEST_CASE("wsdl parser: message with element-ref part", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:message name="GetPriceRequest">
        <wsdl:part name="parameters" element="tns:GetPrice"/>
      </wsdl:message>
    </wsdl:definitions>
  )");
  REQUIRE(doc.messages.size() == 1);
  CHECK(doc.messages[0].name == "GetPriceRequest");
  REQUIRE(doc.messages[0].parts.size() == 1);
  CHECK(doc.messages[0].parts[0].name == "parameters");
  REQUIRE(std::holds_alternative<wsdl::part_by_element>(
      doc.messages[0].parts[0].ref));
  CHECK(std::get<wsdl::part_by_element>(doc.messages[0].parts[0].ref).element ==
        qname("urn:example", "GetPrice"));
}

// -- Message (rpc/type style) -------------------------------------------------

TEST_CASE("wsdl parser: message with type-ref part", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:xs="http://www.w3.org/2001/XMLSchema"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:message name="GetPriceRpc">
        <wsdl:part name="ticker" type="xs:string"/>
      </wsdl:message>
    </wsdl:definitions>
  )");
  REQUIRE(doc.messages.size() == 1);
  REQUIRE(doc.messages[0].parts.size() == 1);
  CHECK(doc.messages[0].parts[0].name == "ticker");
  REQUIRE(
      std::holds_alternative<wsdl::part_by_type>(doc.messages[0].parts[0].ref));
  CHECK(std::get<wsdl::part_by_type>(doc.messages[0].parts[0].ref).type ==
        qname("http://www.w3.org/2001/XMLSchema", "string"));
}

// -- PortType: request-response -----------------------------------------------

TEST_CASE("wsdl parser: portType request-response operation", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:portType name="StockQuotePortType">
        <wsdl:operation name="GetPrice">
          <wsdl:input message="tns:GetPriceRequest"/>
          <wsdl:output message="tns:GetPriceResponse"/>
        </wsdl:operation>
      </wsdl:portType>
    </wsdl:definitions>
  )");
  REQUIRE(doc.port_types.size() == 1);
  CHECK(doc.port_types[0].name == "StockQuotePortType");
  REQUIRE(doc.port_types[0].operations.size() == 1);

  const auto& op = doc.port_types[0].operations[0];
  CHECK(op.name == "GetPrice");
  CHECK(op.input_message == qname("urn:example", "GetPriceRequest"));
  REQUIRE(op.output_message.has_value());
  CHECK(op.output_message.value() == qname("urn:example", "GetPriceResponse"));
  CHECK(op.faults.empty());
}

// -- PortType: one-way --------------------------------------------------------

TEST_CASE("wsdl parser: portType one-way operation", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:portType name="NotifyPort">
        <wsdl:operation name="Notify">
          <wsdl:input message="tns:NotifyRequest"/>
        </wsdl:operation>
      </wsdl:portType>
    </wsdl:definitions>
  )");
  REQUIRE(doc.port_types[0].operations.size() == 1);
  const auto& op = doc.port_types[0].operations[0];
  CHECK(op.name == "Notify");
  CHECK_FALSE(op.output_message.has_value());
}

// -- PortType: operation with fault -------------------------------------------

TEST_CASE("wsdl parser: portType operation with fault", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:portType name="StockQuotePortType">
        <wsdl:operation name="GetPrice">
          <wsdl:input message="tns:GetPriceRequest"/>
          <wsdl:output message="tns:GetPriceResponse"/>
          <wsdl:fault name="ServerFault" message="tns:FaultMessage"/>
        </wsdl:operation>
      </wsdl:portType>
    </wsdl:definitions>
  )");
  const auto& op = doc.port_types[0].operations[0];
  REQUIRE(op.faults.size() == 1);
  CHECK(op.faults[0].name == "ServerFault");
  CHECK(op.faults[0].message == qname("urn:example", "FaultMessage"));
}

// -- Binding: document/literal ------------------------------------------------

TEST_CASE("wsdl parser: binding document/literal", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:binding name="StockQuoteBinding" type="tns:StockQuotePortType">
        <soap:binding style="document"
                      transport="http://schemas.xmlsoap.org/soap/http"/>
        <wsdl:operation name="GetPrice">
          <soap:operation soapAction="http://example.org/GetPrice"/>
          <wsdl:input>
            <soap:body use="literal"/>
          </wsdl:input>
          <wsdl:output>
            <soap:body use="literal"/>
          </wsdl:output>
        </wsdl:operation>
      </wsdl:binding>
    </wsdl:definitions>
  )");
  REQUIRE(doc.bindings.size() == 1);
  const auto& b = doc.bindings[0];
  CHECK(b.name == "StockQuoteBinding");
  CHECK(b.port_type == qname("urn:example", "StockQuotePortType"));
  CHECK(b.soap.style == wsdl::binding_style::document);
  CHECK(b.soap.transport == "http://schemas.xmlsoap.org/soap/http");

  REQUIRE(b.operations.size() == 1);
  const auto& bo = b.operations[0];
  CHECK(bo.name == "GetPrice");
  CHECK(bo.soap_op.soap_action == "http://example.org/GetPrice");
  CHECK(bo.input_body.use == wsdl::body_use::literal);
  CHECK(bo.output_body.use == wsdl::body_use::literal);
}

// -- Binding: rpc/literal -----------------------------------------------------

TEST_CASE("wsdl parser: binding rpc/literal", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:binding name="RpcBinding" type="tns:RpcPortType">
        <soap:binding style="rpc"
                      transport="http://schemas.xmlsoap.org/soap/http"/>
        <wsdl:operation name="GetData">
          <soap:operation soapAction="urn:GetData"/>
          <wsdl:input>
            <soap:body use="literal" namespace="urn:example"/>
          </wsdl:input>
          <wsdl:output>
            <soap:body use="literal" namespace="urn:example"/>
          </wsdl:output>
        </wsdl:operation>
      </wsdl:binding>
    </wsdl:definitions>
  )");
  REQUIRE(doc.bindings.size() == 1);
  CHECK(doc.bindings[0].soap.style == wsdl::binding_style::rpc);
  CHECK(doc.bindings[0].operations[0].input_body.namespace_uri ==
        "urn:example");
}

// -- Service with soap:address ------------------------------------------------

TEST_CASE("wsdl parser: service with soap:address", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:service name="StockQuoteService">
        <wsdl:port name="StockQuotePort" binding="tns:StockQuoteBinding">
          <soap:address location="http://example.org/stockquote"/>
        </wsdl:port>
      </wsdl:service>
    </wsdl:definitions>
  )");
  REQUIRE(doc.services.size() == 1);
  CHECK(doc.services[0].name == "StockQuoteService");
  REQUIRE(doc.services[0].ports.size() == 1);
  const auto& p = doc.services[0].ports[0];
  CHECK(p.name == "StockQuotePort");
  CHECK(p.binding == qname("urn:example", "StockQuoteBinding"));
  CHECK(p.address == "http://example.org/stockquote");
}

// -- Complete WSDL ------------------------------------------------------------

TEST_CASE("wsdl parser: complete wsdl document", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
      xmlns:tns="urn:example"
      name="StockQuote"
      targetNamespace="urn:example">
      <wsdl:message name="GetPriceRequest">
        <wsdl:part name="parameters" element="tns:GetPrice"/>
      </wsdl:message>
      <wsdl:message name="GetPriceResponse">
        <wsdl:part name="parameters" element="tns:GetPriceResult"/>
      </wsdl:message>
      <wsdl:portType name="StockQuotePortType">
        <wsdl:operation name="GetPrice">
          <wsdl:input message="tns:GetPriceRequest"/>
          <wsdl:output message="tns:GetPriceResponse"/>
        </wsdl:operation>
      </wsdl:portType>
      <wsdl:binding name="StockQuoteBinding" type="tns:StockQuotePortType">
        <soap:binding style="document"
                      transport="http://schemas.xmlsoap.org/soap/http"/>
        <wsdl:operation name="GetPrice">
          <soap:operation soapAction="http://example.org/GetPrice"/>
          <wsdl:input><soap:body use="literal"/></wsdl:input>
          <wsdl:output><soap:body use="literal"/></wsdl:output>
        </wsdl:operation>
      </wsdl:binding>
      <wsdl:service name="StockQuoteService">
        <wsdl:port name="StockQuotePort" binding="tns:StockQuoteBinding">
          <soap:address location="http://example.org/stockquote"/>
        </wsdl:port>
      </wsdl:service>
    </wsdl:definitions>
  )");
  CHECK(doc.name == "StockQuote");
  CHECK(doc.target_namespace == "urn:example");
  CHECK(doc.messages.size() == 2);
  CHECK(doc.port_types.size() == 1);
  CHECK(doc.bindings.size() == 1);
  CHECK(doc.services.size() == 1);
}

// -- Embedded xs:schema in <wsdl:types> ---------------------------------------

TEST_CASE("wsdl parser: embedded xs:schema parsed into types",
          "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:xs="http://www.w3.org/2001/XMLSchema"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:types>
        <xs:schema targetNamespace="urn:example"
                   xmlns:xs="http://www.w3.org/2001/XMLSchema">
          <xs:element name="GetPrice" type="xs:string"/>
        </xs:schema>
      </wsdl:types>
    </wsdl:definitions>
  )");
  CHECK(doc.types.schemas().size() == 1);
  CHECK(doc.types.schemas()[0].target_namespace() == "urn:example");
}

// -- SOAP 1.2 binding namespace -----------------------------------------------

TEST_CASE("wsdl parser: soap 1.2 binding namespace accepted", "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap12="http://schemas.xmlsoap.org/wsdl/soap12/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:binding name="Soap12Binding" type="tns:MyPortType">
        <soap12:binding style="document"
                        transport="http://schemas.xmlsoap.org/soap/http"/>
        <wsdl:operation name="DoSomething">
          <soap12:operation soapAction="urn:DoSomething"/>
          <wsdl:input><soap12:body use="literal"/></wsdl:input>
          <wsdl:output><soap12:body use="literal"/></wsdl:output>
        </wsdl:operation>
      </wsdl:binding>
    </wsdl:definitions>
  )");
  REQUIRE(doc.bindings.size() == 1);
  CHECK(doc.bindings[0].name == "Soap12Binding");
  CHECK(doc.bindings[0].soap.style == wsdl::binding_style::document);
  CHECK(doc.bindings[0].soap.soap_ver == xb::soap::soap_version::v1_2);
  CHECK(doc.bindings[0].operations[0].soap_op.soap_action == "urn:DoSomething");
  CHECK(doc.bindings[0].operations[0].input_body.use ==
        wsdl::body_use::literal);
}

TEST_CASE("wsdl parser: soap 1.1 binding sets soap_ver to v1_1",
          "[wsdl_parser]") {
  auto doc = parse_wsdl(R"(
    <wsdl:definitions
      xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
      xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
      xmlns:tns="urn:example"
      name="Test"
      targetNamespace="urn:example">
      <wsdl:binding name="Soap11Binding" type="tns:MyPortType">
        <soap:binding style="document"
                      transport="http://schemas.xmlsoap.org/soap/http"/>
      </wsdl:binding>
    </wsdl:definitions>
  )");
  REQUIRE(doc.bindings.size() == 1);
  CHECK(doc.bindings[0].soap.soap_ver == xb::soap::soap_version::v1_1);
}
