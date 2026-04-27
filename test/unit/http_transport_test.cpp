#include <xb/http_transport.hpp>

#include <catch2/catch_test_macros.hpp>

namespace svc = xb::service;

// -- http_options -------------------------------------------------------------

TEST_CASE("http_options: default values", "[http_transport]") {
  svc::http_options opts;
  CHECK(opts.connect_timeout == std::chrono::milliseconds{30000});
  CHECK(opts.request_timeout == std::chrono::milliseconds{60000});
  CHECK(opts.follow_redirects == true);
  CHECK(opts.verify_peer == true);
  CHECK(opts.ca_bundle.empty());
  CHECK(opts.client_cert.empty());
  CHECK(opts.client_key.empty());
  // Hardening defaults
  CHECK(opts.max_redirects == 5);
  CHECK(opts.max_response_bytes == 64ULL * 1024ULL * 1024ULL);
}

TEST_CASE("http_options: equality", "[http_transport]") {
  svc::http_options a;
  svc::http_options b;
  CHECK(a == b);

  b.connect_timeout = std::chrono::milliseconds{5000};
  CHECK_FALSE(a == b);
}

TEST_CASE("http_options: custom values", "[http_transport]") {
  svc::http_options opts;
  opts.connect_timeout = std::chrono::milliseconds{10000};
  opts.request_timeout = std::chrono::milliseconds{30000};
  opts.follow_redirects = false;
  opts.verify_peer = false;
  opts.ca_bundle = "/etc/ssl/certs/ca-certificates.crt";
  opts.client_cert = "/path/to/cert.pem";
  opts.client_key = "/path/to/key.pem";

  CHECK(opts.connect_timeout == std::chrono::milliseconds{10000});
  CHECK(opts.follow_redirects == false);
  CHECK(opts.ca_bundle == "/etc/ssl/certs/ca-certificates.crt");
}

// -- http_response ------------------------------------------------------------

TEST_CASE("http_response: default values", "[http_transport]") {
  svc::http_response resp;
  CHECK(resp.status_code == 0);
  CHECK(resp.content_type.empty());
  CHECK(resp.body.empty());
}

TEST_CASE("http_response: equality", "[http_transport]") {
  svc::http_response a;
  a.status_code = 200;
  a.content_type = "text/xml";
  a.body = "<response/>";

  svc::http_response b = a;
  CHECK(a == b);

  b.status_code = 500;
  CHECK_FALSE(a == b);
}

// -- http_options move semantics
// -----------------------------------------------

TEST_CASE("http_options: move construction", "[http_transport]") {
  svc::http_options a;
  a.ca_bundle = "/path/to/ca.pem";
  a.connect_timeout = std::chrono::milliseconds{5000};

  svc::http_options b = std::move(a);
  CHECK(b.ca_bundle == "/path/to/ca.pem");
  CHECK(b.connect_timeout == std::chrono::milliseconds{5000});
}

TEST_CASE("http_response: move construction", "[http_transport]") {
  svc::http_response a;
  a.status_code = 200;
  a.body = "<response/>";

  svc::http_response b = std::move(a);
  CHECK(b.status_code == 200);
  CHECK(b.body == "<response/>");
}

// -- http_transport (pimpl basics, requires curl) -----------------------------

#ifdef XB_HAS_CURL
TEST_CASE("http_transport: constructor succeeds with default options",
          "[http_transport]") {
  CHECK_NOTHROW(svc::http_transport{});
}

TEST_CASE("http_transport: constructor succeeds with custom options",
          "[http_transport]") {
  svc::http_options opts;
  opts.connect_timeout = std::chrono::milliseconds{5000};
  CHECK_NOTHROW(svc::http_transport{opts});
}

TEST_CASE("http_transport: move construction", "[http_transport]") {
  svc::http_transport a;
  svc::http_transport b = std::move(a);
  (void)b;
}

TEST_CASE("http_transport: unreachable endpoint throws transport_error",
          "[http_transport]") {
  svc::http_options opts;
  opts.connect_timeout = std::chrono::milliseconds{500};
  opts.request_timeout = std::chrono::milliseconds{1000};
  svc::http_transport t(opts);
  xb::soap::envelope env;
  env.version = xb::soap::soap_version::v1_1;

  CHECK_THROWS_AS(t.call("http://192.0.2.1:1/nonexistent", "urn:test", env),
                  svc::transport_error);
}

#ifdef XB_HAS_POSIX_SOCKETS

#include <xb/http_listener.hpp>

#include <chrono>
#include <thread>

TEST_CASE("http_transport: refuses to connect to loopback by default",
          "[http_transport][security][SSRF]") {
  // Stand up a real listener on 127.0.0.1 so the destination accepts
  // connections; the transport must still refuse via the prereq
  // callback before any HTTP I/O happens.
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  std::thread t([&] {
    listener.serve(
        [](const std::string&, const xb::soap::envelope& req) { return req; });
  });

  svc::http_options opts;
  opts.connect_timeout = std::chrono::milliseconds{500};
  opts.request_timeout = std::chrono::milliseconds{500};
  svc::http_transport client(opts);

  xb::soap::envelope env;
  env.version = xb::soap::soap_version::v1_1;
  auto endpoint = "http://127.0.0.1:" + std::to_string(port) + "/";

  bool threw = false;
  try {
    client.call(endpoint, "urn:test", env);
  } catch (const svc::transport_error&) { threw = true; }

  listener.stop();
  t.join();
  CHECK(threw);
}

TEST_CASE("http_transport: opt-in permits loopback connection",
          "[http_transport][security][SSRF]") {
  svc::http_listener listener({.bind_address = "127.0.0.1", .port = 0});
  auto port = listener.listening_port();

  std::thread t([&] {
    listener.serve([](const std::string&, const xb::soap::envelope& req) {
      xb::soap::envelope resp;
      resp.version = req.version;
      return resp;
    });
  });

  svc::http_options opts;
  opts.block_private_destinations = false;
  opts.connect_timeout = std::chrono::milliseconds{1000};
  opts.request_timeout = std::chrono::milliseconds{1000};
  svc::http_transport client(opts);

  xb::soap::envelope env;
  env.version = xb::soap::soap_version::v1_1;
  auto endpoint = "http://127.0.0.1:" + std::to_string(port) + "/";

  CHECK_NOTHROW(client.call(endpoint, "urn:test", env));

  listener.stop();
  t.join();
}

#endif // XB_HAS_POSIX_SOCKETS

TEST_CASE("http_transport: missing CA bundle file is rejected",
          "[http_transport][security][TLS]") {
  // Configuring a non-existent CA bundle would, with some libcurl
  // builds, silently fall back to the system store. The hardening must
  // refuse the request explicitly so that a typo does not turn into a
  // security-relevant fallback.
  svc::http_options opts;
  opts.connect_timeout = std::chrono::milliseconds{500};
  opts.request_timeout = std::chrono::milliseconds{1000};
  opts.ca_bundle = "/tmp/xb-no-such-ca-bundle-9d4e3f.pem";
  svc::http_transport t(opts);
  xb::soap::envelope env;
  env.version = xb::soap::soap_version::v1_1;

  CHECK_THROWS_AS(t.call("https://example.com/x", "urn:test", env),
                  svc::transport_error);
}
#endif
