#include <xb/xinclude_reader.hpp>

#include <xb/expat_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

  const std::string xi_ns = "http://www.w3.org/2001/XInclude";

  // Helper: collect all events from a reader into a simple string log.
  std::string
  collect_events(xb::xml_reader& reader) {
    std::string log;
    while (reader.read()) {
      switch (reader.node_type()) {
        case xb::xml_node_type::start_element:
          log += "<" + reader.name().local_name() + ">";
          break;
        case xb::xml_node_type::end_element:
          log += "</" + reader.name().local_name() + ">";
          break;
        case xb::xml_node_type::characters:
          log += std::string(reader.text());
          break;
      }
    }
    return log;
  }

  // Helper: resolver backed by a map of href -> content.
  xb::xinclude_resolver
  map_resolver(const std::unordered_map<std::string, std::string>& files) {
    return [&](const std::string& href) -> std::string {
      auto it = files.find(href);
      if (it == files.end()) throw std::runtime_error("not found: " + href);
      return it->second;
    };
  }

} // namespace

// ---------------------------------------------------------------------------
// Passthrough: no xi:include elements
// ---------------------------------------------------------------------------

TEST_CASE("Document without xi:include passes through unchanged",
          "[xinclude_reader]") {
  std::string xml = "<root><a>hello</a><b>world</b></root>";
  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, [](const std::string&) -> std::string {
    throw std::runtime_error("should not be called");
  });

  auto log = collect_events(reader);
  CHECK(log == "<root><a>hello</a><b>world</b></root>");
}

// ---------------------------------------------------------------------------
// Simple include: inline XML elements
// ---------------------------------------------------------------------------

TEST_CASE("xi:include inlines elements from resolved document",
          "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<before/>"
                    "<xi:include href=\"part.xml\"/>"
                    "<after/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {
      {"part.xml", "<section><p>included</p></section>"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  auto log = collect_events(reader);
  CHECK(log == "<root><before></before>"
               "<section><p>included</p></section>"
               "<after></after></root>");
}

// ---------------------------------------------------------------------------
// Text include: parse="text"
// ---------------------------------------------------------------------------

TEST_CASE("xi:include with parse='text' inlines as character data",
          "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"data.txt\" parse=\"text\"/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {
      {"data.txt", "plain text content"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  auto log = collect_events(reader);
  CHECK(log == "<root>plain text content</root>");
}

// ---------------------------------------------------------------------------
// Nested include: included doc itself contains xi:include
// ---------------------------------------------------------------------------

TEST_CASE("Nested xi:include is resolved recursively", "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"outer.xml\"/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {
      {"outer.xml", "<outer xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"inner.xml\"/>"
                    "</outer>"},
      {"inner.xml", "<inner>deep</inner>"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  auto log = collect_events(reader);
  CHECK(log == "<root><outer><inner>deep</inner></outer></root>");
}

// ---------------------------------------------------------------------------
// Circular include detection
// ---------------------------------------------------------------------------

TEST_CASE("Circular xi:include is detected and throws", "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"a.xml\"/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {
      {"a.xml", "<a xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                "<xi:include href=\"b.xml\"/>"
                "</a>"},
      {"b.xml", "<b xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                "<xi:include href=\"a.xml\"/>"
                "</b>"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  CHECK_THROWS_AS(collect_events(reader), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Fallback: resolver fails, fallback content used
// ---------------------------------------------------------------------------

TEST_CASE("xi:fallback content used when include fails", "[xinclude_reader]") {
  std::string xml =
      "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
      "<xi:include href=\"missing.xml\">"
      "<xi:fallback><default>fallback content</default></xi:fallback>"
      "</xi:include>"
      "</root>";

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, [](const std::string&) -> std::string {
    throw std::runtime_error("not found");
  });

  auto log = collect_events(reader);
  CHECK(log == "<root><default>fallback content</default></root>");
}

// ---------------------------------------------------------------------------
// No fallback and resolver fails: error propagates
// ---------------------------------------------------------------------------

TEST_CASE("xi:include without fallback throws when resolver fails",
          "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"missing.xml\"/>"
                    "</root>";

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, [](const std::string&) -> std::string {
    throw std::runtime_error("not found");
  });

  CHECK_THROWS_AS(collect_events(reader), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Multiple includes in same document
// ---------------------------------------------------------------------------

TEST_CASE("Multiple xi:include elements in same document",
          "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"a.xml\"/>"
                    "<middle/>"
                    "<xi:include href=\"b.xml\"/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {{"a.xml", "<first/>"},
                                                        {"b.xml", "<second/>"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  auto log = collect_events(reader);
  CHECK(log == "<root><first></first>"
               "<middle></middle>"
               "<second></second></root>");
}

// ---------------------------------------------------------------------------
// Depth is correct during included content
// ---------------------------------------------------------------------------

TEST_CASE("Depth is consistent during included content", "[xinclude_reader]") {
  std::string xml = "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
                    "<xi:include href=\"part.xml\"/>"
                    "</root>";

  std::unordered_map<std::string, std::string> files = {
      {"part.xml", "<a><b>text</b></a>"}};

  xb::expat_reader inner(xml);
  xb::xinclude_reader reader(inner, map_resolver(files));

  std::vector<std::pair<std::string, std::size_t>> events;
  while (reader.read()) {
    if (reader.node_type() == xb::xml_node_type::start_element) {
      events.push_back({reader.name().local_name(), reader.depth()});
    }
  }

  // root=1, a=2, b=3 (a replaces xi:include at depth 2)
  REQUIRE(events.size() == 3);
  CHECK(events[0] == std::make_pair(std::string("root"), std::size_t(1)));
  CHECK(events[1] == std::make_pair(std::string("a"), std::size_t(2)));
  CHECK(events[2] == std::make_pair(std::string("b"), std::size_t(3)));
}
