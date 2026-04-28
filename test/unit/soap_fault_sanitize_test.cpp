/// @file
/// Tests for xb::soap::sanitize_fault — redacts common server-side
/// leakage patterns (filesystem paths, hex addresses, SQL fragments,
/// stack frames) from a fault's text fields before it ships out as a
/// SOAP response.

#include <xb/soap_fault_sanitize.hpp>
#include <xb/soap_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

namespace soap = xb::soap;

namespace {

  /// Convenience: build a SOAP 1.1 fault with the given fault_string.
  soap::fault
  make_1_1(const std::string& s) {
    soap::fault_1_1 f;
    f.fault_code = "soap:Server";
    f.fault_string = s;
    return f;
  }

  std::string
  fault_string_of(const soap::fault& f) {
    return std::get<soap::fault_1_1>(f).fault_string;
  }

} // namespace

// -- Pattern redaction (default mode) -----------------------------------------

TEST_CASE("sanitize_fault redacts absolute Unix paths",
          "[soap_fault_sanitize][security]") {
  auto f = make_1_1("Cannot open /etc/passwd for reading");
  soap::sanitize_fault(f);
  auto out = fault_string_of(f);
  CHECK(out.find("/etc/passwd") == std::string::npos);
  CHECK(out.find("<path>") != std::string::npos);
}

TEST_CASE("sanitize_fault redacts Windows-style absolute paths",
          "[soap_fault_sanitize][security]") {
  auto f = make_1_1(R"(Failed to read C:\Users\admin\secrets.txt)");
  soap::sanitize_fault(f);
  auto out = fault_string_of(f);
  CHECK(out.find("admin") == std::string::npos);
  CHECK(out.find("<path>") != std::string::npos);
}

TEST_CASE("sanitize_fault redacts hex addresses",
          "[soap_fault_sanitize][security]") {
  auto f = make_1_1("Crash at 0x7fff12345678 in handler");
  soap::sanitize_fault(f);
  auto out = fault_string_of(f);
  CHECK(out.find("0x7fff12345678") == std::string::npos);
  CHECK(out.find("<addr>") != std::string::npos);
}

TEST_CASE("sanitize_fault redacts SQL-error fragments",
          "[soap_fault_sanitize][security]") {
  auto f = make_1_1("Database error: UNIQUE constraint failed on users.email");
  soap::sanitize_fault(f);
  auto out = fault_string_of(f);
  CHECK(out.find("UNIQUE constraint failed") == std::string::npos);
  CHECK(out.find("users.email") == std::string::npos);
  CHECK(out.find("<sql-error>") != std::string::npos);
}

TEST_CASE("sanitize_fault leaves benign text unchanged",
          "[soap_fault_sanitize]") {
  auto f = make_1_1("Operation failed");
  soap::sanitize_fault(f);
  CHECK(fault_string_of(f) == "Operation failed");
}

TEST_CASE("sanitize_fault redacts the SOAP 1.2 reason texts",
          "[soap_fault_sanitize][security]") {
  soap::fault_1_2 f;
  f.code.value = "soap:Receiver";
  soap::fault_reason_text rt;
  rt.lang = "en";
  rt.text = "Failed to open /var/log/app.log at 0xdeadbeef0001";
  f.reason.push_back(std::move(rt));

  soap::fault generic = f;
  soap::sanitize_fault(generic);

  const auto& sanitized = std::get<soap::fault_1_2>(generic);
  REQUIRE(sanitized.reason.size() == 1);
  const auto& text = sanitized.reason[0].text;
  CHECK(text.find("/var/log/app.log") == std::string::npos);
  CHECK(text.find("0xdeadbeef0001") == std::string::npos);
}

TEST_CASE("sanitize_fault clears the SOAP 1.1 detail element",
          "[soap_fault_sanitize][security]") {
  // The detail element commonly carries application-internal stack
  // traces and structured exception data.  The default policy is to
  // drop it entirely — callers that have an audit-safe detail block
  // should attach it AFTER sanitisation.
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "ok";
  f.detail = xb::any_element(xb::qname{"urn:internal", "StackTrace"}, {},
                             {std::string("a lot of internals")});

  soap::fault generic = f;
  soap::sanitize_fault(generic);

  CHECK_FALSE(std::get<soap::fault_1_1>(generic).detail.has_value());
}

// -- Generic-message mode -----------------------------------------------------

TEST_CASE("sanitize_fault with replace_with_generic uses the configured "
          "generic message",
          "[soap_fault_sanitize]") {
  auto f = make_1_1("any leaky message at all");
  soap::fault_sanitize_options opts;
  opts.replace_with_generic = true;
  opts.generic_message = "Internal server error";
  soap::sanitize_fault(f, opts);
  CHECK(fault_string_of(f) == "Internal server error");
}

TEST_CASE("sanitize_fault with replace_with_generic clears fault_actor",
          "[soap_fault_sanitize]") {
  soap::fault_1_1 f;
  f.fault_code = "soap:Server";
  f.fault_string = "internal error";
  f.fault_actor = "http://example.com/internal-actor";
  soap::fault generic = f;
  soap::fault_sanitize_options opts;
  opts.replace_with_generic = true;
  soap::sanitize_fault(generic, opts);
  CHECK(std::get<soap::fault_1_1>(generic).fault_actor.empty());
}
