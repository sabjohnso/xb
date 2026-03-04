#include <xb/expat_reader.hpp>
#include <xb/wsdl2_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace xb;
namespace w2 = xb::wsdl2;

static w2::description
parse_wsdl2(const std::string& xml) {
  expat_reader reader(xml);
  wsdl2_parser parser;
  return parser.parse(reader);
}

// 1. Minimal description with targetNamespace

TEST_CASE("wsdl2 parser: minimal description", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      targetNamespace="urn:example:stockquote">
    </wsdl:description>
  )");
  CHECK(desc.target_namespace == "urn:example:stockquote");
  CHECK(desc.imports.empty());
  CHECK(desc.includes.empty());
  CHECK(desc.interfaces.empty());
  CHECK(desc.bindings.empty());
  CHECK(desc.services.empty());
}

// 2. Import and include elements

TEST_CASE("wsdl2 parser: import and include", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      targetNamespace="urn:example">
      <wsdl:import namespace="urn:other" location="other.wsdl"/>
      <wsdl:include location="common.wsdl"/>
    </wsdl:description>
  )");
  REQUIRE(desc.imports.size() == 1);
  CHECK(desc.imports[0].namespace_uri == "urn:other");
  CHECK(desc.imports[0].location == "other.wsdl");
  REQUIRE(desc.includes.size() == 1);
  CHECK(desc.includes[0].location == "common.wsdl");
}

// 3. Interface with single in-out operation

TEST_CASE("wsdl2 parser: interface with in-out operation", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="StockQuoteInterface">
        <wsdl:operation name="GetPrice"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:GetPriceRequest"/>
          <wsdl:output element="tns:GetPriceResponse"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  REQUIRE(desc.interfaces.size() == 1);
  const auto& iface = desc.interfaces[0];
  CHECK(iface.name == "StockQuoteInterface");
  REQUIRE(iface.operations.size() == 1);

  const auto& op = iface.operations[0];
  CHECK(op.name == "GetPrice");
  CHECK(op.pattern == w2::mep::in_out);
  REQUIRE(op.input_element.has_value());
  CHECK(*op.input_element == qname("urn:example", "GetPriceRequest"));
  REQUIRE(op.output_element.has_value());
  CHECK(*op.output_element == qname("urn:example", "GetPriceResponse"));
}

// 4. Interface with in-only operation (no output)

TEST_CASE("wsdl2 parser: in-only operation", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="NotifyInterface">
        <wsdl:operation name="Notify"
                        pattern="http://www.w3.org/ns/wsdl/in-only">
          <wsdl:input element="tns:NotifyEvent"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& op = desc.interfaces[0].operations[0];
  CHECK(op.name == "Notify");
  CHECK(op.pattern == w2::mep::in_only);
  CHECK(op.input_element.has_value());
  CHECK_FALSE(op.output_element.has_value());
}

// 5. Interface with fault declarations + infault/outfault refs

TEST_CASE("wsdl2 parser: faults and fault refs", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="Iface">
        <wsdl:fault name="InvalidTicker" element="tns:InvalidTickerDetail"/>
        <wsdl:operation name="GetPrice"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:GetPriceRequest"/>
          <wsdl:output element="tns:GetPriceResponse"/>
          <wsdl:infault ref="tns:InvalidTicker" messageLabel="In"/>
          <wsdl:outfault ref="tns:InvalidTicker" messageLabel="Out"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& iface = desc.interfaces[0];
  REQUIRE(iface.faults.size() == 1);
  CHECK(iface.faults[0].name == "InvalidTicker");
  CHECK(iface.faults[0].element == qname("urn:example", "InvalidTickerDetail"));

  const auto& op = iface.operations[0];
  REQUIRE(op.infaults.size() == 1);
  CHECK(op.infaults[0].ref == qname("urn:example", "InvalidTicker"));
  CHECK(op.infaults[0].message_label == "In");
  REQUIRE(op.outfaults.size() == 1);
  CHECK(op.outfaults[0].ref == qname("urn:example", "InvalidTicker"));
  CHECK(op.outfaults[0].message_label == "Out");
}

// 6. Interface inheritance (extends attribute)

TEST_CASE("wsdl2 parser: interface inheritance", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="Parent">
        <wsdl:operation name="Op1"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:Req1"/>
          <wsdl:output element="tns:Resp1"/>
        </wsdl:operation>
      </wsdl:interface>
      <wsdl:interface name="Child" extends="tns:Parent">
        <wsdl:operation name="Op2"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:Req2"/>
          <wsdl:output element="tns:Resp2"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  REQUIRE(desc.interfaces.size() == 2);
  CHECK(desc.interfaces[1].name == "Child");
  REQUIRE(desc.interfaces[1].extends.size() == 1);
  CHECK(desc.interfaces[1].extends[0] == qname("urn:example", "Parent"));
}

// 7. Binding with SOAP extensions

TEST_CASE("wsdl2 parser: binding with SOAP extensions", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:wsoap="http://www.w3.org/ns/wsdl/soap"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:binding name="StockQuoteBinding"
                    interface="tns:StockQuoteInterface"
                    type="http://www.w3.org/ns/wsdl/soap"
                    wsoap:protocol="http://www.w3.org/2003/05/soap/bindings/HTTP/"
                    wsoap:mepDefault="http://www.w3.org/2003/05/soap/mep/request-response">
      </wsdl:binding>
    </wsdl:description>
  )");
  REQUIRE(desc.bindings.size() == 1);
  const auto& b = desc.bindings[0];
  CHECK(b.name == "StockQuoteBinding");
  CHECK(b.interface_ref == qname("urn:example", "StockQuoteInterface"));
  CHECK(b.type == "http://www.w3.org/ns/wsdl/soap");
  CHECK(b.soap.protocol == "http://www.w3.org/2003/05/soap/bindings/HTTP/");
  CHECK(b.soap.mep_default ==
        "http://www.w3.org/2003/05/soap/mep/request-response");
}

// 8. Binding operation with wsoap:action

TEST_CASE("wsdl2 parser: binding operation with soap action",
          "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:wsoap="http://www.w3.org/ns/wsdl/soap"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:binding name="B" interface="tns:I"
                    type="http://www.w3.org/ns/wsdl/soap">
        <wsdl:operation ref="tns:GetPrice"
                        wsoap:action="http://example.org/GetPrice"/>
      </wsdl:binding>
    </wsdl:description>
  )");
  REQUIRE(desc.bindings[0].operations.size() == 1);
  const auto& bop = desc.bindings[0].operations[0];
  CHECK(bop.ref == qname("urn:example", "GetPrice"));
  CHECK(bop.soap.soap_action == "http://example.org/GetPrice");
}

// 9. Service with endpoint

TEST_CASE("wsdl2 parser: service with endpoint", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:service name="StockQuoteService"
                    interface="tns:StockQuoteInterface">
        <wsdl:endpoint name="StockQuoteEndpoint"
                       binding="tns:StockQuoteBinding"
                       address="http://example.org/stockquote"/>
      </wsdl:service>
    </wsdl:description>
  )");
  REQUIRE(desc.services.size() == 1);
  const auto& svc = desc.services[0];
  CHECK(svc.name == "StockQuoteService");
  CHECK(svc.interface_ref == qname("urn:example", "StockQuoteInterface"));
  REQUIRE(svc.endpoints.size() == 1);
  CHECK(svc.endpoints[0].name == "StockQuoteEndpoint");
  CHECK(svc.endpoints[0].binding_ref ==
        qname("urn:example", "StockQuoteBinding"));
  CHECK(svc.endpoints[0].address == "http://example.org/stockquote");
}

// 10. Embedded xs:schema in <wsdl:types>

TEST_CASE("wsdl2 parser: embedded xs:schema in types", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:xs="http://www.w3.org/2001/XMLSchema"
      targetNamespace="urn:example">
      <wsdl:types>
        <xs:schema targetNamespace="urn:example">
          <xs:element name="GetPrice" type="xs:string"/>
        </xs:schema>
      </wsdl:types>
    </wsdl:description>
  )");
  CHECK(desc.types.schemas().size() == 1);
  CHECK(desc.types.schemas()[0].target_namespace() == "urn:example");
}

// 11. Complete WSDL 2.0 document

TEST_CASE("wsdl2 parser: complete document", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:wsoap="http://www.w3.org/ns/wsdl/soap"
      xmlns:tns="urn:example"
      xmlns:xs="http://www.w3.org/2001/XMLSchema"
      targetNamespace="urn:example">

      <wsdl:types>
        <xs:schema targetNamespace="urn:example">
          <xs:element name="GetPriceRequest" type="xs:string"/>
          <xs:element name="GetPriceResponse" type="xs:decimal"/>
        </xs:schema>
      </wsdl:types>

      <wsdl:interface name="StockQuoteInterface">
        <wsdl:fault name="InvalidTicker" element="tns:InvalidTickerDetail"/>
        <wsdl:operation name="GetPrice"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:GetPriceRequest"/>
          <wsdl:output element="tns:GetPriceResponse"/>
        </wsdl:operation>
      </wsdl:interface>

      <wsdl:binding name="StockQuoteBinding"
                    interface="tns:StockQuoteInterface"
                    type="http://www.w3.org/ns/wsdl/soap"
                    wsoap:protocol="http://www.w3.org/2003/05/soap/bindings/HTTP/">
        <wsdl:operation ref="tns:GetPrice"
                        wsoap:action="http://example.org/GetPrice"/>
      </wsdl:binding>

      <wsdl:service name="StockQuoteService"
                    interface="tns:StockQuoteInterface">
        <wsdl:endpoint name="StockQuoteEndpoint"
                       binding="tns:StockQuoteBinding"
                       address="http://example.org/stockquote"/>
      </wsdl:service>
    </wsdl:description>
  )");
  CHECK(desc.target_namespace == "urn:example");
  CHECK(desc.types.schemas().size() == 1);
  CHECK(desc.interfaces.size() == 1);
  CHECK(desc.bindings.size() == 1);
  CHECK(desc.services.size() == 1);
  CHECK(desc.interfaces[0].operations.size() == 1);
  CHECK(desc.interfaces[0].faults.size() == 1);
  CHECK(desc.bindings[0].operations.size() == 1);
  CHECK(desc.services[0].endpoints.size() == 1);
}

// MEP inferred from children when no pattern attribute

TEST_CASE("wsdl2 parser: MEP inferred from children", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:operation name="InferredInOut">
          <wsdl:input element="tns:Req"/>
          <wsdl:output element="tns:Resp"/>
        </wsdl:operation>
        <wsdl:operation name="InferredInOnly">
          <wsdl:input element="tns:Req"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& ops = desc.interfaces[0].operations;
  REQUIRE(ops.size() == 2);
  CHECK(ops[0].pattern == w2::mep::in_out);
  CHECK(ops[1].pattern == w2::mep::in_only);
}

// 13. In-opt-out MEP URI (spec uses "in-opt-out", not "in-optional-out")

TEST_CASE("wsdl2 parser: in-opt-out MEP URI per spec", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:operation name="MaybeReply"
                        pattern="http://www.w3.org/ns/wsdl/in-opt-out">
          <wsdl:input element="tns:Req"/>
          <wsdl:output element="tns:Resp"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& op = desc.interfaces[0].operations[0];
  CHECK(op.pattern == w2::mep::in_optional_out);
}

// 14. messageLabel attribute on input and output

TEST_CASE("wsdl2 parser: messageLabel on input and output", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:operation name="Op"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="tns:Req" messageLabel="In"/>
          <wsdl:output element="tns:Resp" messageLabel="Out"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& op = desc.interfaces[0].operations[0];
  CHECK(op.input_message_label == "In");
  CHECK(op.output_message_label == "Out");
}

// 15. Interface fault without element attribute

TEST_CASE("wsdl2 parser: fault without element attribute", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:fault name="GeneralFault"/>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& fault = desc.interfaces[0].faults[0];
  CHECK(fault.name == "GeneralFault");
  CHECK_FALSE(fault.element.has_value());
}

// 16. Special element tokens (#any, #none, #other)

TEST_CASE("wsdl2 parser: element token #any on fault", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:fault name="AnyFault" element="#any"/>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& fault = desc.interfaces[0].faults[0];
  CHECK_FALSE(fault.element.has_value());
  REQUIRE(fault.token.has_value());
  CHECK(*fault.token == w2::element_token::any);
}

TEST_CASE("wsdl2 parser: element token #none on input", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:operation name="Op"
                        pattern="http://www.w3.org/ns/wsdl/in-out">
          <wsdl:input element="#none"/>
          <wsdl:output element="tns:Resp"/>
        </wsdl:operation>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& op = desc.interfaces[0].operations[0];
  CHECK_FALSE(op.input_element.has_value());
  REQUIRE(op.input_token.has_value());
  CHECK(*op.input_token == w2::element_token::none);
  CHECK(op.output_element.has_value());
  CHECK_FALSE(op.output_token.has_value());
}

TEST_CASE("wsdl2 parser: element token #other on fault", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      targetNamespace="urn:example">
      <wsdl:interface name="I">
        <wsdl:fault name="OtherFault" element="#other"/>
      </wsdl:interface>
    </wsdl:description>
  )");
  const auto& fault = desc.interfaces[0].faults[0];
  CHECK_FALSE(fault.element.has_value());
  REQUIRE(fault.token.has_value());
  CHECK(*fault.token == w2::element_token::other);
}

// 19. wsoap:version attribute on binding

TEST_CASE("wsdl2 parser: wsoap:version attribute on binding",
          "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:wsoap="http://www.w3.org/ns/wsdl/soap"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:binding name="B" interface="tns:I"
                    type="http://www.w3.org/ns/wsdl/soap"
                    wsoap:protocol="http://www.w3.org/2003/05/soap/bindings/HTTP/"
                    wsoap:version="1.1">
      </wsdl:binding>
    </wsdl:description>
  )");
  const auto& b = desc.bindings[0];
  CHECK(b.soap.version == "1.1");
}

// 20. Binding operation child elements (input, output, infault, outfault)

TEST_CASE("wsdl2 parser: binding operation child elements", "[wsdl2_parser]") {
  auto desc = parse_wsdl2(R"(
    <wsdl:description
      xmlns:wsdl="http://www.w3.org/ns/wsdl"
      xmlns:wsoap="http://www.w3.org/ns/wsdl/soap"
      xmlns:tns="urn:example"
      targetNamespace="urn:example">
      <wsdl:binding name="B" interface="tns:I"
                    type="http://www.w3.org/ns/wsdl/soap">
        <wsdl:operation ref="tns:GetPrice"
                        wsoap:action="http://example.org/GetPrice">
          <wsdl:input messageLabel="In"/>
          <wsdl:output messageLabel="Out"/>
          <wsdl:infault ref="tns:SomeFault" messageLabel="In"/>
          <wsdl:outfault ref="tns:OtherFault" messageLabel="Out"/>
        </wsdl:operation>
      </wsdl:binding>
    </wsdl:description>
  )");
  const auto& bop = desc.bindings[0].operations[0];
  REQUIRE(bop.inputs.size() == 1);
  CHECK(bop.inputs[0].message_label == "In");
  REQUIRE(bop.outputs.size() == 1);
  CHECK(bop.outputs[0].message_label == "Out");
  REQUIRE(bop.infaults.size() == 1);
  CHECK(bop.infaults[0].message_label == "In");
  REQUIRE(bop.outfaults.size() == 1);
  CHECK(bop.outfaults[0].message_label == "Out");
}
