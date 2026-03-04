#include <xb/wsdl_codegen.hpp>

#include <xb/naming.hpp>
#include <xb/soap_model.hpp>

#include <sstream>
#include <string>

namespace xb {

  namespace {

    std::string
    client_class_name(const service::resolved_port& port) {
      return to_snake_case(to_cpp_identifier(port.name)) + "_client";
    }

    std::string
    method_name(const service::resolved_operation& op) {
      return to_snake_case(to_cpp_identifier(op.name));
    }

    std::string
    soap_version_literal(soap::soap_version ver) {
      return ver == soap::soap_version::v1_2 ? "xb::soap::soap_version::v1_2"
                                             : "xb::soap::soap_version::v1_1";
    }

    std::string
    generate_doc_lit_method(const service::resolved_operation& op,
                            soap::soap_version soap_ver) {
      std::ostringstream os;
      auto mname = method_name(op);

      if (op.one_way) {
        // One-way: void return, single input part
        os << "  void " << mname << "(";
        if (!op.input.empty()) {
          os << "const " << op.input[0].cpp_type << "& req";
        }
        os << ") {\n";
        os << "    xb::soap::envelope env;\n";
        os << "    env.version = " << soap_version_literal(soap_ver) << ";\n";
        if (!op.input.empty()) {
          os << "    env.body.push_back(\n";
          os << "        xb::service::make_body_element(\n";
          os << "            xb::qname(\""
             << op.input[0].xsd_name.namespace_uri() << "\", \""
             << op.input[0].xsd_name.local_name() << "\"),\n";
          os << "            req, &" << op.input[0].write_function << "));\n";
        }
        os << "    transport_.call(endpoint_, \"" << op.soap_action
           << "\", env);\n";
        os << "  }\n";
      } else {
        // Request-response
        std::string ret_type =
            op.output.empty() ? "void" : op.output[0].cpp_type;
        os << "  " << ret_type << " " << mname << "(";
        if (!op.input.empty()) {
          os << "const " << op.input[0].cpp_type << "& req";
        }
        os << ") {\n";
        os << "    xb::soap::envelope env;\n";
        os << "    env.version = " << soap_version_literal(soap_ver) << ";\n";
        if (!op.input.empty()) {
          os << "    env.body.push_back(\n";
          os << "        xb::service::make_body_element(\n";
          os << "            xb::qname(\""
             << op.input[0].xsd_name.namespace_uri() << "\", \""
             << op.input[0].xsd_name.local_name() << "\"),\n";
          os << "            req, &" << op.input[0].write_function << "));\n";
        }
        os << "    auto resp = transport_.call(endpoint_, \"" << op.soap_action
           << "\", env);\n";
        os << "    xb::service::check_fault(resp);\n";
        if (!op.output.empty()) {
          os << "    return xb::service::parse_body_element<" << ret_type
             << ">(\n";
          os << "        resp.body.at(0), &" << op.output[0].read_function
             << ");\n";
        }
        os << "  }\n";
      }
      return os.str();
    }

    std::string
    generate_rpc_lit_method(const service::resolved_operation& op,
                            soap::soap_version soap_ver) {
      std::ostringstream os;
      auto mname = method_name(op);

      std::string ret_type =
          (op.one_way || op.output.empty()) ? "void" : op.output[0].cpp_type;
      os << "  " << ret_type << " " << mname << "(";
      // RPC parameters become method arguments
      for (size_t i = 0; i < op.input.size(); ++i) {
        if (i > 0) os << ", ";
        os << "const " << op.input[i].cpp_type << "& " << op.input[i].name;
      }
      os << ") {\n";
      os << "    xb::soap::envelope env;\n";
      os << "    env.version = " << soap_version_literal(soap_ver) << ";\n";
      os << "    env.body.push_back(\n";
      os << "        xb::service::make_rpc_request(\n";
      os << "            xb::qname(\"" << op.rpc_namespace << "\", \""
         << op.name << "\"),\n";
      os << "            {";
      for (size_t i = 0; i < op.input.size(); ++i) {
        if (i > 0) os << ", ";
        os << "{\"" << op.input[i].name << "\", xb::format(" << op.input[i].name
           << ")}";
      }
      os << "}));\n";
      os << "    auto resp = transport_.call(endpoint_, \"" << op.soap_action
         << "\", env);\n";
      if (!op.one_way) {
        os << "    xb::service::check_fault(resp);\n";
        if (!op.output.empty()) {
          os << "    return xb::service::parse_body_element<" << ret_type
             << ">(\n";
          os << "        resp.body.at(0), &" << op.output[0].read_function
             << ");\n";
        }
      }
      os << "  }\n";
      return os.str();
    }

    std::string
    generate_client_method(const service::resolved_operation& op,
                           soap::soap_version soap_ver) {
      if (op.style == wsdl::binding_style::rpc) {
        return generate_rpc_lit_method(op, soap_ver);
      }
      return generate_doc_lit_method(op, soap_ver);
    }

    cpp_raw_text
    generate_client_class(const service::resolved_port& port) {
      std::ostringstream os;
      auto cls_name = client_class_name(port);

      os << "class " << cls_name << " {\n";
      os << "  xb::service::transport& transport_;\n";
      os << "  std::string endpoint_;\n";
      os << "public:\n";
      os << "  " << cls_name
         << "(xb::service::transport& t, std::string endpoint)\n";
      os << "      : transport_(t), endpoint_(std::move(endpoint)) {}\n";
      os << "\n";

      for (const auto& op : port.operations) {
        os << generate_client_method(op, port.soap_ver);
        os << "\n";
      }

      os << "};\n";
      return cpp_raw_text{os.str()};
    }

    std::string
    server_interface_name(const service::resolved_port& port) {
      return to_snake_case(to_cpp_identifier(port.name)) + "_interface";
    }

    std::string
    server_dispatcher_name(const service::resolved_port& port) {
      return to_snake_case(to_cpp_identifier(port.name)) + "_dispatcher";
    }

    cpp_raw_text
    generate_server_interface(const service::resolved_port& port) {
      std::ostringstream os;
      auto iface_name = server_interface_name(port);

      os << "class " << iface_name << " {\n";
      os << "public:\n";
      os << "  virtual ~" << iface_name << "() = default;\n";
      os << "\n";

      for (const auto& op : port.operations) {
        auto mname = method_name(op);
        if (op.one_way || op.output.empty()) {
          os << "  virtual void " << mname << "(";
        } else {
          os << "  virtual " << op.output[0].cpp_type << " " << mname << "(";
        }
        if (!op.input.empty()) {
          if (op.style == wsdl::binding_style::rpc) {
            for (size_t i = 0; i < op.input.size(); ++i) {
              if (i > 0) os << ", ";
              os << "const " << op.input[i].cpp_type << "& "
                 << op.input[i].name;
            }
          } else {
            os << "const " << op.input[0].cpp_type << "& req";
          }
        }
        os << ") = 0;\n";
      }

      os << "};\n";
      return cpp_raw_text{os.str()};
    }

    cpp_raw_text
    generate_server_dispatcher(const service::resolved_port& port) {
      std::ostringstream os;
      auto disp_name = server_dispatcher_name(port);
      auto iface_name = server_interface_name(port);

      os << "class " << disp_name << " {\n";
      os << "  " << iface_name << "& impl_;\n";
      os << "public:\n";
      os << "  explicit " << disp_name << "(" << iface_name << "& impl)\n";
      os << "      : impl_(impl) {}\n";
      os << "\n";

      // Element QName dispatch
      os << "  xb::soap::envelope dispatch("
         << "const xb::soap::envelope& request) {\n";
      os << "    const auto& body = request.body.at(0);\n";

      for (const auto& op : port.operations) {
        auto mname = method_name(op);
        if (op.style == wsdl::binding_style::document && !op.input.empty()) {
          os << "    if (body.name() == xb::qname(\""
             << op.input[0].xsd_name.namespace_uri() << "\", \""
             << op.input[0].xsd_name.local_name() << "\")) {\n";
          os << "      auto input = xb::service::parse_body_element<"
             << op.input[0].cpp_type << ">(\n";
          os << "          body, &" << op.input[0].read_function << ");\n";
          if (op.one_way || op.output.empty()) {
            os << "      impl_." << mname << "(input);\n";
            os << "      return xb::soap::envelope{request.version};\n";
          } else {
            os << "      auto result = impl_." << mname << "(input);\n";
            os << "      xb::soap::envelope resp;\n";
            os << "      resp.version = request.version;\n";
            os << "      resp.body.push_back(xb::service::make_body_element(\n";
            os << "          xb::qname(\""
               << op.output[0].xsd_name.namespace_uri() << "\", \""
               << op.output[0].xsd_name.local_name() << "\"),\n";
            os << "          result, &" << op.output[0].write_function
               << "));\n";
            os << "      return resp;\n";
          }
          os << "    }\n";
        } else if (op.style == wsdl::binding_style::rpc) {
          os << "    if (body.name().local_name() == \"" << op.name
             << "\") {\n";
          os << "      // RPC dispatch: extract parts from wrapper\n";
          if (op.one_way || op.output.empty()) {
            os << "      impl_." << mname
               << "(/* TODO: extract RPC parts */);\n";
            os << "      return xb::soap::envelope{request.version};\n";
          } else {
            os << "      // TODO: full RPC dispatch\n";
            os << "      throw std::runtime_error(\"RPC dispatch not yet "
                  "implemented\");\n";
          }
          os << "    }\n";
        }
      }

      os << "    throw std::runtime_error(\"Unknown operation\");\n";
      os << "  }\n";
      os << "\n";

      // SOAPAction dispatch
      os << "  xb::soap::envelope dispatch("
         << "const std::string& soap_action,\n"
         << "                              "
         << "const xb::soap::envelope& request) {\n";

      for (const auto& op : port.operations) {
        os << "    if (soap_action == \"" << op.soap_action << "\") {\n";
        os << "      return dispatch(request);\n";
        os << "    }\n";
      }

      os << "    throw std::runtime_error("
         << "\"Unknown SOAPAction: \" + soap_action);\n";
      os << "  }\n";
      os << "};\n";

      return cpp_raw_text{os.str()};
    }

  } // namespace

  std::vector<cpp_file>
  wsdl_codegen::generate_client(const service::service_description& desc,
                                const wsdl_codegen_options& /*options*/) const {
    std::vector<cpp_file> files;

    for (const auto& svc : desc.services) {
      for (const auto& port : svc.ports) {
        cpp_file file;
        file.filename =
            to_snake_case(to_cpp_identifier(port.name)) + "_client.hpp";
        file.kind = file_kind::header;
        file.includes.push_back({"<xb/wsdl_transport.hpp>"});
        file.includes.push_back({"<xb/http_transport.hpp>"});
        file.includes.push_back({"<xb/wsdl_support.hpp>"});
        file.includes.push_back({"<xb/soap_model.hpp>"});
        file.includes.push_back({"<xb/qname.hpp>"});
        file.includes.push_back({"<string>"});

        cpp_namespace ns;
        ns.name = "xb::client";
        ns.declarations.push_back(generate_client_class(port));
        file.namespaces.push_back(std::move(ns));

        files.push_back(std::move(file));
      }
    }

    return files;
  }

  std::vector<cpp_file>
  wsdl_codegen::generate_server(const service::service_description& desc,
                                const wsdl_codegen_options& /*options*/) const {
    std::vector<cpp_file> files;

    for (const auto& svc : desc.services) {
      for (const auto& port : svc.ports) {
        cpp_file file;
        file.filename =
            to_snake_case(to_cpp_identifier(port.name)) + "_server.hpp";
        file.kind = file_kind::header;
        file.includes.push_back({"<xb/wsdl_transport.hpp>"});
        file.includes.push_back({"<xb/wsdl_support.hpp>"});
        file.includes.push_back({"<xb/soap_model.hpp>"});
        file.includes.push_back({"<xb/qname.hpp>"});
        file.includes.push_back({"<string>"});
        file.includes.push_back({"<stdexcept>"});

        cpp_namespace ns;
        ns.name = "xb::server";
        ns.declarations.push_back(generate_server_interface(port));
        ns.declarations.push_back(generate_server_dispatcher(port));
        file.namespaces.push_back(std::move(ns));

        files.push_back(std::move(file));
      }
    }

    return files;
  }

} // namespace xb
