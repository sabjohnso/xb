#include <xb/schema_fetcher.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace xb;

// ---------------------------------------------------------------------------
// Phase A: URL resolution (tests 1-5)
// ---------------------------------------------------------------------------

TEST_CASE("resolve_url: relative path against HTTP base", "[schema_fetcher]") {
  auto result =
      resolve_url("https://example.com/schemas/main.xsd", "types.xsd");
  REQUIRE(result == "https://example.com/schemas/types.xsd");
}

TEST_CASE("resolve_url: ../ traversal in HTTP URL", "[schema_fetcher]") {
  auto result = resolve_url("https://example.com/schemas/sub/main.xsd",
                            "../common/types.xsd");
  REQUIRE(result == "https://example.com/schemas/common/types.xsd");
}

TEST_CASE("resolve_url: absolute URL returned as-is", "[schema_fetcher]") {
  auto result = resolve_url("https://example.com/schemas/main.xsd",
                            "https://other.com/types.xsd");
  REQUIRE(result == "https://other.com/types.xsd");
}

TEST_CASE("resolve_url: relative path against local filesystem base",
          "[schema_fetcher]") {
  auto result = resolve_url("/home/user/schemas/main.xsd", "types.xsd");
  REQUIRE(result == "/home/user/schemas/types.xsd");
}

TEST_CASE("resolve_url: ../ traversal in local path", "[schema_fetcher]") {
  auto result =
      resolve_url("/home/user/schemas/sub/main.xsd", "../common/types.xsd");
  REQUIRE(result == "/home/user/schemas/common/types.xsd");
}

// ---------------------------------------------------------------------------
// Phase B: Crawl logic (tests 6-11)
// ---------------------------------------------------------------------------

static transport_fn
make_mock_transport(const std::unordered_map<std::string, std::string>& files) {
  return [files](const std::string& url) -> std::string {
    auto it = files.find(url);
    if (it == files.end()) throw std::runtime_error("not found: " + url);
    return it->second;
  };
}

static const char* standalone_schema =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/test">
  <xs:element name="Foo" type="xs:string"/>
</xs:schema>)";

TEST_CASE("crawl_schemas: single schema, no dependencies", "[schema_fetcher]") {
  auto transport = make_mock_transport(
      {{"https://example.com/main.xsd", standalone_schema}});

  auto result = crawl_schemas("https://example.com/main.xsd", transport);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].source_url == "https://example.com/main.xsd");
  REQUIRE(result[0].content == standalone_schema);
}

static const char* schema_with_import =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/main">
  <xs:import namespace="http://example.com/types"
             schemaLocation="types.xsd"/>
  <xs:element name="Root" type="xs:string"/>
</xs:schema>)";

static const char* types_schema =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/types">
  <xs:simpleType name="MyString">
    <xs:restriction base="xs:string"/>
  </xs:simpleType>
</xs:schema>)";

TEST_CASE("crawl_schemas: schema with one import returns two entries",
          "[schema_fetcher]") {
  auto transport =
      make_mock_transport({{"https://example.com/main.xsd", schema_with_import},
                           {"https://example.com/types.xsd", types_schema}});

  auto result = crawl_schemas("https://example.com/main.xsd", transport);
  REQUIRE(result.size() == 2);

  // Verify both URLs present (order is BFS)
  bool has_main = false, has_types = false;
  for (const auto& s : result) {
    if (s.source_url == "https://example.com/main.xsd") has_main = true;
    if (s.source_url == "https://example.com/types.xsd") has_types = true;
  }
  REQUIRE(has_main);
  REQUIRE(has_types);
}

static const char* schema_a =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/a">
  <xs:import namespace="http://example.com/b"
             schemaLocation="b.xsd"/>
</xs:schema>)";

static const char* schema_b =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/b">
  <xs:include schemaLocation="c.xsd"/>
</xs:schema>)";

static const char* schema_c =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/b">
  <xs:element name="Leaf" type="xs:string"/>
</xs:schema>)";

TEST_CASE("crawl_schemas: transitive imports A->B->C returns three entries",
          "[schema_fetcher]") {
  auto transport =
      make_mock_transport({{"https://example.com/a.xsd", schema_a},
                           {"https://example.com/b.xsd", schema_b},
                           {"https://example.com/c.xsd", schema_c}});

  auto result = crawl_schemas("https://example.com/a.xsd", transport);
  REQUIRE(result.size() == 3);
}

static const char* circular_a =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/a">
  <xs:import namespace="http://example.com/b"
             schemaLocation="b.xsd"/>
</xs:schema>)";

static const char* circular_b =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/b">
  <xs:import namespace="http://example.com/a"
             schemaLocation="a.xsd"/>
</xs:schema>)";

TEST_CASE("crawl_schemas: circular import terminates with two entries",
          "[schema_fetcher]") {
  auto transport =
      make_mock_transport({{"https://example.com/a.xsd", circular_a},
                           {"https://example.com/b.xsd", circular_b}});

  auto result = crawl_schemas("https://example.com/a.xsd", transport);
  REQUIRE(result.size() == 2);
}

TEST_CASE("crawl_schemas: missing schema with best-effort continues",
          "[schema_fetcher]") {
  // types.xsd is referenced but not in the transport
  auto transport = make_mock_transport(
      {{"https://example.com/main.xsd", schema_with_import}});

  fetch_options opts;
  opts.fail_fast = false;
  auto result = crawl_schemas("https://example.com/main.xsd", transport, opts);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].source_url == "https://example.com/main.xsd");
}

static const char* schema_empty_location =
    R"(<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://example.com/test">
  <xs:import namespace="http://example.com/other"/>
  <xs:element name="Foo" type="xs:string"/>
</xs:schema>)";

TEST_CASE("crawl_schemas: empty schemaLocation is skipped",
          "[schema_fetcher]") {
  auto transport = make_mock_transport(
      {{"https://example.com/main.xsd", schema_empty_location}});

  auto result = crawl_schemas("https://example.com/main.xsd", transport);
  REQUIRE(result.size() == 1);
}

// ---------------------------------------------------------------------------
// Phase C: Local path computation (tests 12-15)
// ---------------------------------------------------------------------------

TEST_CASE("compute_local_paths: single URL produces filename only",
          "[schema_fetcher]") {
  std::vector<fetched_schema> schemas = {
      {"https://example.com/schemas/main.xsd", "<content>"}};

  auto entries = compute_local_paths(schemas);
  REQUIRE(entries.size() == 1);
  REQUIRE(entries[0].local_path == "main.xsd");
  REQUIRE(entries[0].url == "https://example.com/schemas/main.xsd");
  REQUIRE(entries[0].size == 9); // "<content>" is 9 bytes
}

TEST_CASE("compute_local_paths: shared prefix produces relative paths",
          "[schema_fetcher]") {
  std::vector<fetched_schema> schemas = {
      {"https://example.com/schemas/main.xsd", "aaa"},
      {"https://example.com/schemas/common/types.xsd", "bb"}};

  auto entries = compute_local_paths(schemas);
  REQUIRE(entries.size() == 2);

  // After stripping "schemas/", we get relative paths
  bool has_main = false, has_types = false;
  for (const auto& e : entries) {
    if (e.local_path == "main.xsd") has_main = true;
    if (e.local_path == "common/types.xsd") has_types = true;
  }
  REQUIRE(has_main);
  REQUIRE(has_types);
}

TEST_CASE("compute_local_paths: paths with ../ normalized correctly",
          "[schema_fetcher]") {
  // The URLs would already be normalized by resolve_url, but verify
  // compute_local_paths handles them correctly
  std::vector<fetched_schema> schemas = {
      {"https://example.com/a/main.xsd", "x"},
      {"https://example.com/b/other.xsd", "y"}};

  auto entries = compute_local_paths(schemas);
  REQUIRE(entries.size() == 2);

  // Common prefix is "https://example.com/" so paths are "a/main.xsd",
  // "b/other.xsd"
  bool has_a = false, has_b = false;
  for (const auto& e : entries) {
    if (e.local_path == "a/main.xsd") has_a = true;
    if (e.local_path == "b/other.xsd") has_b = true;
  }
  REQUIRE(has_a);
  REQUIRE(has_b);
}

TEST_CASE("compute_local_paths: local filesystem paths", "[schema_fetcher]") {
  std::vector<fetched_schema> schemas = {
      {"/home/user/schemas/main.xsd", "content1"},
      {"/home/user/schemas/sub/types.xsd", "content2"}};

  auto entries = compute_local_paths(schemas);
  REQUIRE(entries.size() == 2);

  bool has_main = false, has_sub = false;
  for (const auto& e : entries) {
    if (e.local_path == "main.xsd") has_main = true;
    if (e.local_path == "sub/types.xsd") has_sub = true;
  }
  REQUIRE(has_main);
  REQUIRE(has_sub);
}

// ---------------------------------------------------------------------------
// Phase D: Manifest (test 16)
// ---------------------------------------------------------------------------

TEST_CASE("write_manifest: produces valid JSON structure", "[schema_fetcher]") {
  auto path = std::string("/tmp/xb_test_manifest.json");

  fetch_manifest manifest;
  manifest.root_url = "https://example.com/main.xsd";
  manifest.fetched_at = "2026-02-25T14:30:00Z";
  manifest.schemas = {{"https://example.com/main.xsd", "main.xsd", 100},
                      {"https://example.com/types.xsd", "types.xsd", 200}};

  write_manifest(path, manifest);

  std::ifstream in(path);
  REQUIRE(in.good());
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  // Verify key fields are present
  REQUIRE(content.find("\"root\"") != std::string::npos);
  REQUIRE(content.find("https://example.com/main.xsd") != std::string::npos);
  REQUIRE(content.find("\"fetched\"") != std::string::npos);
  REQUIRE(content.find("2026-02-25T14:30:00Z") != std::string::npos);
  REQUIRE(content.find("\"schemas\"") != std::string::npos);
  REQUIRE(content.find("\"path\"") != std::string::npos);
  REQUIRE(content.find("main.xsd") != std::string::npos);
  REQUIRE(content.find("types.xsd") != std::string::npos);
  REQUIRE(content.find("\"size\"") != std::string::npos);

  std::remove(path.c_str());
}
