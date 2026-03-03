#include <xb/wsdl_codegen.hpp>

#include <xb/cpp_code.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/service_model.hpp>
#include <xb/wsdl_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace svc = xb::service;
namespace wsdl = xb::wsdl;

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

// -- Server codegen tests -----------------------------------------------------

TEST_CASE("wsdl server codegen: interface has pure virtual methods",
          "[wsdl_server_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_server(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class stock_quote_port_interface"));
  CHECK(contains(output, "virtual ~stock_quote_port_interface()"));
  CHECK(contains(output, "virtual ex::get_price_response get_price("));
  CHECK(contains(output, ") = 0;"));
}

TEST_CASE(
    "wsdl server codegen: dispatcher generated with element QName dispatch",
    "[wsdl_server_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_server(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "class stock_quote_port_dispatcher"));
  CHECK(contains(output, "stock_quote_port_interface& impl_"));
  CHECK(contains(output, "dispatch(const xb::soap::envelope& request)"));
  // Element QName comparison for doc/lit dispatch
  CHECK(contains(output, "xb::qname(\"urn:ex\", \"GetPrice\")"));
  CHECK(contains(output, "parse_body_element"));
  CHECK(contains(output, "make_body_element"));
}

TEST_CASE("wsdl server codegen: SOAPAction dispatch present",
          "[wsdl_server_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_server(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "dispatch(const std::string& soap_action"));
  CHECK(contains(output, "\"http://example.org/GetPrice\""));
}

TEST_CASE("wsdl server codegen: one-way operations produce void dispatch",
          "[wsdl_server_codegen]") {
  auto desc = make_oneway_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_server(desc);

  REQUIRE_FALSE(files.empty());
  auto output = render(files);

  CHECK(contains(output, "virtual void notify("));
  CHECK(contains(output, "class notify_port_dispatcher"));
}

TEST_CASE("wsdl server codegen: correct includes", "[wsdl_server_codegen]") {
  auto desc = make_doc_lit_description();
  xb::wsdl_codegen gen;
  auto files = gen.generate_server(desc);

  REQUIRE_FALSE(files.empty());

  bool has_transport = false;
  bool has_support = false;
  bool has_soap = false;
  for (const auto& f : files) {
    for (const auto& inc : f.includes) {
      if (contains(inc.path, "wsdl_transport")) has_transport = true;
      if (contains(inc.path, "wsdl_support")) has_support = true;
      if (contains(inc.path, "soap_model")) has_soap = true;
    }
  }
  CHECK(has_transport);
  CHECK(has_support);
  CHECK(has_soap);
}
