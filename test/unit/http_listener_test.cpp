#include <xb/http_listener.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace svc = xb::service;
namespace soap = xb::soap;

// -- http_listener_options ----------------------------------------------------

TEST_CASE("http_listener_options: default values", "[http_listener]") {
  svc::http_listener_options opts;
  CHECK(opts.bind_address == "0.0.0.0");
  CHECK(opts.port == 8080);
  CHECK(opts.path == "/");
  CHECK(opts.backlog == 16);
}

TEST_CASE("http_listener_options: equality", "[http_listener]") {
  svc::http_listener_options a;
  svc::http_listener_options b;
  CHECK(a == b);

  b.port = 9090;
  CHECK_FALSE(a == b);
}

// -- http_listener (requires POSIX sockets) -----------------------------------

#ifdef XB_HAS_POSIX_SOCKETS

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <thread>

namespace {

  // Minimal HTTP client using raw sockets for test independence.
  std::string
  http_post(std::uint16_t port, const std::string& path,
            const std::string& content_type, const std::string& body,
            const std::string& soap_action = "") {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      ::close(fd);
      throw std::runtime_error("connect() failed");
    }

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: localhost:" << port << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n";
    if (!soap_action.empty())
      req << "SOAPAction: \"" << soap_action << "\"\r\n";
    req << "Connection: close\r\n"
        << "\r\n"
        << body;

    auto request = req.str();
    ::send(fd, request.data(), request.size(), 0);

    std::string response;
    char buf[4096];
    for (;;) {
      auto n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) break;
      response.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fd);
    return response;
  }

  std::string
  make_soap_xml(const std::string& body_content) {
    soap::envelope env;
    env.version = soap::soap_version::v1_2;
    env.body.push_back(
        xb::any_element(xb::qname{"urn:test", "Echo"}, {},
                        {xb::any_element::child{std::string(body_content)}}));
    std::ostringstream os;
    xb::ostream_writer writer(os);
    soap::write_envelope(writer, env);
    return os.str();
  }

} // namespace

TEST_CASE("http_listener: bind to port 0 assigns a port", "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  CHECK(listener.listening_port() != 0);
}

TEST_CASE("http_listener: move construction", "[http_listener]") {
  svc::http_listener a({.bind_address = "127.0.0.1", .port = 0});
  auto port = a.listening_port();
  svc::http_listener b = std::move(a);
  CHECK(b.listening_port() == port);
}

TEST_CASE("http_listener: stop causes serve to return", "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  bool serve_returned = false;

  std::thread t([&] {
    listener.serve(
        [](const std::string&, const soap::envelope& req) { return req; });
    serve_returned = true;
  });

  // Give serve() time to enter its loop
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  listener.stop();
  t.join();

  CHECK(serve_returned);
}

TEST_CASE("http_listener: SOAP round-trip over HTTP", "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  // Handler echoes the request body back
  std::thread t([&] {
    listener.serve([](const std::string& action, const soap::envelope& req) {
      soap::envelope resp;
      resp.version = req.version;
      // Echo body + add action as text in a wrapper element
      resp.body.push_back(
          xb::any_element(xb::qname{"urn:test", "EchoResponse"}, {},
                          {xb::any_element::child{std::string(action)}}));
      return resp;
    });
  });

  std::string request_xml = make_soap_xml("hello");
  auto response =
      http_post(port, "/",
                "application/soap+xml; charset=utf-8; action=\"urn:test/Echo\"",
                request_xml);

  listener.stop();
  t.join();

  // Response should contain HTTP 200 and the echo
  CHECK(response.find("HTTP/1.1 200") != std::string::npos);
  CHECK(response.find("EchoResponse") != std::string::npos);
  CHECK(response.find("urn:test/Echo") != std::string::npos);
}

TEST_CASE("http_listener: SOAPAction from header (SOAP 1.1)",
          "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  std::string captured_action;
  std::thread t([&] {
    listener.serve([&captured_action](const std::string& action,
                                      const soap::envelope& req) {
      captured_action = action;
      soap::envelope resp;
      resp.version = req.version;
      return resp;
    });
  });

  // SOAP 1.1: text/xml + separate SOAPAction header
  soap::envelope env;
  env.version = soap::soap_version::v1_1;
  std::ostringstream os;
  xb::ostream_writer writer(os);
  soap::write_envelope(writer, env);

  http_post(port, "/", "text/xml; charset=utf-8", os.str(),
            "urn:test/MyAction");

  listener.stop();
  t.join();

  CHECK(captured_action == "urn:test/MyAction");
}

TEST_CASE("http_listener: handler exception produces SOAP fault",
          "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  std::thread t([&] {
    listener.serve(
        [](const std::string&, const soap::envelope&) -> soap::envelope {
          throw std::runtime_error("something broke");
        });
  });

  std::string request_xml = make_soap_xml("trigger-error");
  auto response =
      http_post(port, "/", "application/soap+xml; charset=utf-8", request_xml);

  listener.stop();
  t.join();

  CHECK(response.find("HTTP/1.1 500") != std::string::npos);
  CHECK(response.find("Fault") != std::string::npos);
  CHECK(response.find("something broke") != std::string::npos);
}

TEST_CASE("http_listener: non-POST request returns 405", "[http_listener]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  std::thread t([&] {
    listener.serve(
        [](const std::string&, const soap::envelope& req) { return req; });
  });

  // Send a GET request via raw socket
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

  std::string get_req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  ::send(fd, get_req.data(), get_req.size(), 0);

  std::string response;
  char buf[4096];
  for (;;) {
    auto n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    response.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fd);

  listener.stop();
  t.join();

  CHECK(response.find("HTTP/1.1 405") != std::string::npos);
}

#endif // XB_HAS_POSIX_SOCKETS
