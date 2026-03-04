#include <xb/wsdl2_resolver.hpp>
#include <xb/wsdl_codegen.hpp>

#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/service_model.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl2_model.hpp>
#include <xb/wsdl_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace svc = xb::service;
namespace w2 = xb::wsdl2;

// -- Helpers ------------------------------------------------------------------

static bool
contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

static std::string
render(const std::vector<xb::cpp_file>& files) {
  xb::cpp_writer writer;
  std::string result;
  for (const auto& f : files) {
    result += writer.write(f);
  }
  return result;
}

static w2::description
make_wsdl2_desc() {
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

// 1. Full WSDL 2.0 pipeline: resolve → generate_client → verify SOAP 1.2

TEST_CASE("wsdl2 codegen: full pipeline produces SOAP 1.2 client",
          "[wsdl2_codegen]") {
  auto desc = make_wsdl2_desc();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;
  auto service_desc = resolver.resolve(desc, types);

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(service_desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class stock_quote_endpoint_client"));
  CHECK(contains(output, "get_price"));
  CHECK(contains(output, "soap_version::v1_2"));
  CHECK_FALSE(contains(output, "soap_version::v1_1"));
}

// 2. WSDL 2.0 server codegen: generate_server → verify interface + dispatcher

TEST_CASE("wsdl2 codegen: server interface and dispatcher", "[wsdl2_codegen]") {
  auto desc = make_wsdl2_desc();
  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;
  auto service_desc = resolver.resolve(desc, types);

  xb::wsdl_codegen gen;
  auto files = gen.generate_server(service_desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class stock_quote_endpoint_interface"));
  CHECK(contains(output, "class stock_quote_endpoint_dispatcher"));
  CHECK(contains(output, "get_price"));
}

// 3. Interface inheritance codegen: A extends B → client has both operations

TEST_CASE("wsdl2 codegen: inherited operations in client", "[wsdl2_codegen]") {
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
  ep.name = "ChildEndpoint";
  ep.binding_ref = xb::qname("urn:example", "ChildBinding");
  ep.address = "http://example.org/child";

  w2::service svc;
  svc.name = "ChildService";
  svc.interface_ref = xb::qname("urn:example", "ChildInterface");
  svc.endpoints.push_back(ep);
  desc.services.push_back(svc);

  xb::type_map types = xb::type_map::defaults();
  xb::wsdl2_resolver resolver;
  auto service_desc = resolver.resolve(desc, types);

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(service_desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "parent_op"));
  CHECK(contains(output, "child_op"));
}

// 4. WSDL 1.1 with SOAP 1.2: verify Group 1 fix works end-to-end

TEST_CASE("wsdl2 codegen: WSDL 1.1 SOAP 1.2 end-to-end", "[wsdl2_codegen]") {
  // Build a WSDL 1.1 service_description with SOAP 1.2
  svc::resolved_part in_part;
  in_part.name = "parameters";
  in_part.xsd_name = xb::qname("urn:ex", "GetPrice");
  in_part.is_element = true;
  in_part.cpp_type = "ex::get_price";
  in_part.read_function = "ex::read_get_price";
  in_part.write_function = "ex::write_get_price";

  svc::resolved_part out_part;
  out_part.name = "parameters";
  out_part.xsd_name = xb::qname("urn:ex", "GetPriceResponse");
  out_part.is_element = true;
  out_part.cpp_type = "ex::get_price_response";
  out_part.read_function = "ex::read_get_price_response";
  out_part.write_function = "ex::write_get_price_response";

  svc::resolved_operation op;
  op.name = "GetPrice";
  op.soap_action = "http://example.org/GetPrice";
  op.style = xb::wsdl::binding_style::document;
  op.use = xb::wsdl::body_use::literal;
  op.input.push_back(in_part);
  op.output.push_back(out_part);

  svc::resolved_port port;
  port.name = "StockQuotePort";
  port.address = "http://example.org/stockquote";
  port.soap_ver = xb::soap::soap_version::v1_2; // SOAP 1.2!
  port.operations.push_back(op);

  svc::resolved_service service;
  service.name = "StockQuoteService";
  service.target_namespace = "urn:ex";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "soap_version::v1_2"));
  CHECK_FALSE(contains(output, "soap_version::v1_1"));
}

// 5. WSDL 1.1 regression: existing codegen patterns unchanged

TEST_CASE("wsdl2 codegen: WSDL 1.1 default still uses SOAP 1.1",
          "[wsdl2_codegen]") {
  // Build a standard WSDL 1.1 service_description (default soap_ver = v1_1)
  svc::resolved_part in_part;
  in_part.name = "parameters";
  in_part.xsd_name = xb::qname("urn:ex", "Req");
  in_part.is_element = true;
  in_part.cpp_type = "ex::req";
  in_part.read_function = "ex::read_req";
  in_part.write_function = "ex::write_req";

  svc::resolved_operation op;
  op.name = "DoWork";
  op.soap_action = "urn:DoWork";
  op.style = xb::wsdl::binding_style::document;
  op.use = xb::wsdl::body_use::literal;
  op.input.push_back(in_part);
  op.one_way = true;

  svc::resolved_port port;
  port.name = "WorkPort";
  port.address = "http://example.org/work";
  // soap_ver defaults to v1_1
  port.operations.push_back(op);

  svc::resolved_service service;
  service.name = "WorkService";
  service.target_namespace = "urn:ex";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "soap_version::v1_1"));
}
