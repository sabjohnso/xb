/// @file
/// Unit tests for xb::expat_reader, with particular focus on the security
/// hardening contract: external entity references and runaway entity
/// expansion are rejected by default, while internal general entities
/// continue to work for documents that legitimately use them.

#include <xb/expat_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

namespace {

  std::string
  collect_text(xb::xml_reader& reader) {
    std::string text;
    while (reader.read()) {
      if (reader.node_type() == xb::xml_node_type::characters) {
        text += std::string(reader.text());
      }
    }
    return text;
  }

} // namespace

// ---------------------------------------------------------------------------
// Baseline behaviour
// ---------------------------------------------------------------------------

TEST_CASE("expat_reader parses a minimal document", "[expat_reader]") {
  xb::expat_reader r("<r>hello</r>");
  CHECK(collect_text(r) == "hello");
}

TEST_CASE("expat_reader expands internal general entities", "[expat_reader]") {
  // Internal general entities are a legitimate XML 1.0 feature used by
  // standards documents (UBL xmldsig, W3C XMLSchema). The hardening must
  // not break them.
  std::string xml = "<?xml version=\"1.0\"?>"
                    "<!DOCTYPE r [<!ENTITY foo \"bar\">]>"
                    "<r>&foo;</r>";
  xb::expat_reader r(xml);
  CHECK(collect_text(r) == "bar");
}

// ---------------------------------------------------------------------------
// Security: external entity references (XXE)
// ---------------------------------------------------------------------------

TEST_CASE("expat_reader rejects external entity references by default",
          "[expat_reader][security][XXE]") {
  // The referenced URI does not need to exist — the rejection happens
  // before any I/O is attempted, so this test does not depend on the
  // filesystem.
  std::string xml =
      "<?xml version=\"1.0\"?>"
      "<!DOCTYPE r [<!ENTITY x SYSTEM \"file:///nonexistent/path\">]>"
      "<r>&x;</r>";
  REQUIRE_THROWS_AS(xb::expat_reader(xml), std::runtime_error);
}

TEST_CASE(
    "expat_reader accepts external entity references when explicitly opted in",
    "[expat_reader][security][XXE]") {
  std::string xml =
      "<?xml version=\"1.0\"?>"
      "<!DOCTYPE r [<!ENTITY x SYSTEM \"file:///nonexistent/path\">]>"
      "<r>&x;</r>";
  xb::expat_reader_options opts;
  opts.allow_external_entities = true;
  // With opt-in, no rejection handler is installed; expat's default
  // behaviour is to skip the reference (treat it as unexpanded). The
  // parse must not throw.
  REQUIRE_NOTHROW(xb::expat_reader(xml, opts));
}

// ---------------------------------------------------------------------------
// Security: billion-laughs / entity-expansion amplification
// ---------------------------------------------------------------------------

TEST_CASE("expat_reader rejects billion-laughs amplification by default",
          "[expat_reader][security][DoS]") {
  // 11 levels of 4-fan-out gives ~3 MiB of expanded text from ~600 bytes
  // of input, an amplification factor of ~5000x — well over the
  // configured maximum. The activation threshold (set to 1 MiB by the
  // hardening) is also exceeded, so expat aborts.
  std::string xml = "<?xml version=\"1.0\"?>"
                    "<!DOCTYPE lolz ["
                    "<!ENTITY l0 \"lol\">"
                    "<!ENTITY l1 \"&l0;&l0;&l0;&l0;\">"
                    "<!ENTITY l2 \"&l1;&l1;&l1;&l1;\">"
                    "<!ENTITY l3 \"&l2;&l2;&l2;&l2;\">"
                    "<!ENTITY l4 \"&l3;&l3;&l3;&l3;\">"
                    "<!ENTITY l5 \"&l4;&l4;&l4;&l4;\">"
                    "<!ENTITY l6 \"&l5;&l5;&l5;&l5;\">"
                    "<!ENTITY l7 \"&l6;&l6;&l6;&l6;\">"
                    "<!ENTITY l8 \"&l7;&l7;&l7;&l7;\">"
                    "<!ENTITY l9 \"&l8;&l8;&l8;&l8;\">"
                    "<!ENTITY l10 \"&l9;&l9;&l9;&l9;\">"
                    "]>"
                    "<lolz>&l10;</lolz>";
  REQUIRE_THROWS_AS(xb::expat_reader(xml), std::runtime_error);
}

TEST_CASE("expat_reader allows modest entity expansion under the limit",
          "[expat_reader][security]") {
  // Tiny amplification well under the 100x cap and well under the 1 MiB
  // activation threshold — must succeed.
  std::string xml = "<?xml version=\"1.0\"?>"
                    "<!DOCTYPE r ["
                    "<!ENTITY a \"AAAA\">"
                    "<!ENTITY b \"&a;&a;&a;&a;\">"
                    "]>"
                    "<r>&b;</r>";
  xb::expat_reader r(xml);
  CHECK(collect_text(r) == "AAAAAAAAAAAAAAAA");
}

// ---------------------------------------------------------------------------
// Security: parameter entity parsing is disabled by default
// ---------------------------------------------------------------------------

TEST_CASE("expat_reader does not parse parameter entities by default",
          "[expat_reader][security]") {
  // A parameter entity referencing an external URL would, if parsed,
  // attempt to fetch the URL. With XML_PARAM_ENTITY_PARSING_NEVER the
  // declaration is skipped entirely and parsing succeeds without any
  // I/O attempt.
  std::string xml = "<?xml version=\"1.0\"?>"
                    "<!DOCTYPE r ["
                    "<!ENTITY % unused SYSTEM \"file:///nonexistent\">"
                    "]>"
                    "<r>ok</r>";
  xb::expat_reader r(xml);
  CHECK(collect_text(r) == "ok");
}
