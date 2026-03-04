#include <xb/wsdl_codegen.hpp>

#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/service_model.hpp>
#include <xb/soap_model.hpp>
#include <xb/wsdl_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace svc = xb::service;
namespace wsdl = xb::wsdl;

// -- Helpers ------------------------------------------------------------------

static svc::service_description
make_doc_lit_description() {
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
  op.style = wsdl::binding_style::document;
  op.use = wsdl::body_use::literal;
  op.input.push_back(in_part);
  op.output.push_back(out_part);

  svc::resolved_port port;
  port.name = "StockQuotePort";
  port.address = "http://example.org/stockquote";
  port.operations.push_back(op);

  svc::resolved_service service;
  service.name = "StockQuoteService";
  service.target_namespace = "urn:ex";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);
  return desc;
}

static svc::service_description
make_rpc_lit_description() {
  svc::resolved_part in_part;
  in_part.name = "ticker";
  in_part.xsd_name = xb::qname("http://www.w3.org/2001/XMLSchema", "string");
  in_part.is_element = false;
  in_part.cpp_type = "std::string";
  in_part.read_function = "xb::read_string";
  in_part.write_function = "xb::write_string";

  svc::resolved_part out_part;
  out_part.name = "result";
  out_part.xsd_name = xb::qname("http://www.w3.org/2001/XMLSchema", "string");
  out_part.is_element = false;
  out_part.cpp_type = "std::string";
  out_part.read_function = "xb::read_string";
  out_part.write_function = "xb::write_string";

  svc::resolved_operation op;
  op.name = "GetData";
  op.soap_action = "urn:GetData";
  op.style = wsdl::binding_style::rpc;
  op.use = wsdl::body_use::literal;
  op.rpc_namespace = "urn:rpc:data";
  op.input.push_back(in_part);
  op.output.push_back(out_part);

  svc::resolved_port port;
  port.name = "DataPort";
  port.address = "http://example.org/data";
  port.operations.push_back(op);

  svc::resolved_service service;
  service.name = "DataService";
  service.target_namespace = "urn:rpc:data";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);
  return desc;
}

static svc::service_description
make_oneway_description() {
  svc::resolved_part in_part;
  in_part.name = "parameters";
  in_part.xsd_name = xb::qname("urn:ex", "Notify");
  in_part.is_element = true;
  in_part.cpp_type = "ex::notify";
  in_part.read_function = "ex::read_notify";
  in_part.write_function = "ex::write_notify";

  svc::resolved_operation op;
  op.name = "Notify";
  op.soap_action = "urn:Notify";
  op.style = wsdl::binding_style::document;
  op.use = wsdl::body_use::literal;
  op.one_way = true;
  op.input.push_back(in_part);

  svc::resolved_port port;
  port.name = "NotifyPort";
  port.address = "http://example.org/notify";
  port.operations.push_back(op);

  svc::resolved_service service;
  service.name = "NotifyService";
  service.target_namespace = "urn:ex";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);
  return desc;
}

static svc::service_description
make_multi_op_description() {
  svc::resolved_part in1;
  in1.name = "parameters";
  in1.xsd_name = xb::qname("urn:ex", "GetPrice");
  in1.is_element = true;
  in1.cpp_type = "ex::get_price";
  in1.read_function = "ex::read_get_price";
  in1.write_function = "ex::write_get_price";

  svc::resolved_part out1;
  out1.name = "parameters";
  out1.xsd_name = xb::qname("urn:ex", "GetPriceResponse");
  out1.is_element = true;
  out1.cpp_type = "ex::get_price_response";
  out1.read_function = "ex::read_get_price_response";
  out1.write_function = "ex::write_get_price_response";

  svc::resolved_operation op1;
  op1.name = "GetPrice";
  op1.soap_action = "urn:GetPrice";
  op1.style = wsdl::binding_style::document;
  op1.use = wsdl::body_use::literal;
  op1.input.push_back(in1);
  op1.output.push_back(out1);

  svc::resolved_part in2;
  in2.name = "parameters";
  in2.xsd_name = xb::qname("urn:ex", "SetPrice");
  in2.is_element = true;
  in2.cpp_type = "ex::set_price";
  in2.read_function = "ex::read_set_price";
  in2.write_function = "ex::write_set_price";

  svc::resolved_part out2;
  out2.name = "parameters";
  out2.xsd_name = xb::qname("urn:ex", "SetPriceResponse");
  out2.is_element = true;
  out2.cpp_type = "ex::set_price_response";
  out2.read_function = "ex::read_set_price_response";
  out2.write_function = "ex::write_set_price_response";

  svc::resolved_operation op2;
  op2.name = "SetPrice";
  op2.soap_action = "urn:SetPrice";
  op2.style = wsdl::binding_style::document;
  op2.use = wsdl::body_use::literal;
  op2.input.push_back(in2);
  op2.output.push_back(out2);

  svc::resolved_port port;
  port.name = "StockPort";
  port.address = "http://example.org/stock";
  port.operations.push_back(op1);
  port.operations.push_back(op2);

  svc::resolved_service service;
  service.name = "StockService";
  service.target_namespace = "urn:ex";
  service.ports.push_back(port);

  svc::service_description desc;
  desc.services.push_back(service);
  return desc;
}

// Helper to find text in generated output
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

// -- Client codegen tests -----------------------------------------------------

TEST_CASE("wsdl codegen: doc/lit client has class with method",
          "[wsdl_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class stock_quote_port_client"));
  CHECK(contains(output, "xb::service::transport&"));
  CHECK(contains(output, "get_price"));
  CHECK(contains(output, "ex::get_price_response"));
  CHECK(contains(output, "\"http://example.org/GetPrice\""));
}

TEST_CASE("wsdl codegen: rpc/lit client has RPC wrapper logic",
          "[wsdl_codegen]") {
  auto desc = make_rpc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class data_port_client"));
  CHECK(contains(output, "get_data"));
  CHECK(contains(output, "make_rpc_request"));
  CHECK(contains(output, "\"urn:GetData\""));
}

TEST_CASE("wsdl codegen: one-way client has void return", "[wsdl_codegen]") {
  auto desc = make_oneway_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "void notify"));
  CHECK(contains(output, "class notify_port_client"));
}

TEST_CASE("wsdl codegen: multiple operations all present", "[wsdl_codegen]") {
  auto desc = make_multi_op_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "get_price"));
  CHECK(contains(output, "set_price"));
  CHECK(contains(output, "class stock_port_client"));
}

TEST_CASE("wsdl codegen: SOAP 1.2 client uses soap_version::v1_2",
          "[wsdl_codegen]") {
  auto desc = make_doc_lit_description();
  // Set SOAP 1.2 on the port
  desc.services[0].ports[0].soap_ver = xb::soap::soap_version::v1_2;

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "soap_version::v1_2"));
  CHECK_FALSE(contains(output, "soap_version::v1_1"));
}

TEST_CASE("wsdl codegen: default SOAP 1.1 client uses soap_version::v1_1",
          "[wsdl_codegen]") {
  auto desc = make_doc_lit_description();
  // soap_ver defaults to v1_1

  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "soap_version::v1_1"));
}

TEST_CASE("wsdl codegen: client includes contain transport and support",
          "[wsdl_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_client(desc);

  REQUIRE_FALSE(files.empty());

  bool has_transport = false;
  bool has_support = false;
  bool has_http_transport = false;
  for (const auto& f : files) {
    for (const auto& inc : f.includes) {
      if (contains(inc.path, "wsdl_transport")) has_transport = true;
      if (contains(inc.path, "wsdl_support")) has_support = true;
      if (contains(inc.path, "http_transport")) has_http_transport = true;
    }
  }
  CHECK(has_transport);
  CHECK(has_support);
  CHECK(has_http_transport);
}
