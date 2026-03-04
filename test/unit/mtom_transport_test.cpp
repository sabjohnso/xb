#include <xb/mtom_transport.hpp>

#include <catch2/catch_test_macros.hpp>

namespace svc = xb::service;

// -- mtom_options -------------------------------------------------------------

TEST_CASE("mtom_options: default values", "[mtom_transport]") {
  svc::mtom_options opts;
  CHECK(opts.optimization_threshold == 1024);
}

TEST_CASE("mtom_options: equality", "[mtom_transport]") {
  svc::mtom_options a;
  svc::mtom_options b;
  CHECK(a == b);

  b.optimization_threshold = 512;
  CHECK_FALSE(a == b);
}

TEST_CASE("mtom_options: custom threshold", "[mtom_transport]") {
  svc::mtom_options opts;
  opts.optimization_threshold = 4096;
  CHECK(opts.optimization_threshold == 4096);
}

// -- mtom_transport (pimpl basics, requires curl) -----------------------------

#ifdef XB_HAS_CURL
TEST_CASE("mtom_transport: constructor succeeds with default options",
          "[mtom_transport]") {
  CHECK_NOTHROW(svc::mtom_transport{});
}

TEST_CASE("mtom_transport: constructor succeeds with custom options",
          "[mtom_transport]") {
  svc::http_options http_opts;
  http_opts.connect_timeout = std::chrono::milliseconds{5000};
  svc::mtom_options mtom_opts;
  mtom_opts.optimization_threshold = 512;
  CHECK_NOTHROW(svc::mtom_transport(http_opts, mtom_opts));
}

TEST_CASE("mtom_transport: move construction", "[mtom_transport]") {
  svc::mtom_transport a;
  svc::mtom_transport b = std::move(a);
  (void)b;
}
#endif
