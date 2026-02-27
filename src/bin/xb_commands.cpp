// GCC 12 emits a false -Wmaybe-uninitialized for std::variant containing
// std::unique_ptr in particle::term_type at -O3. Suppress it here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "xb_main.hpp"

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/doc_generator.hpp>
#include <xb/expat_reader.hpp>
#include <xb/naming.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/rng_compact_parser.hpp>
#include <xb/rng_parser.hpp>
#include <xb/rng_simplify.hpp>
#include <xb/rng_translator.hpp>
#include <xb/schema_fetcher.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#ifdef XB_HAS_CURL
#include <curl/curl.h>
#endif

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

  constexpr int exit_success = 0;
  constexpr int exit_usage = 1;
  constexpr int exit_io = 2;
  constexpr int exit_parse = 3;
  constexpr int exit_codegen = 4;

  // ---------------------------------------------------------------------------
  // Shared utilities
  // ---------------------------------------------------------------------------

  std::string
  read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      std::cerr << "xb: cannot open file: " << path << "\n";
      std::exit(exit_io);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  // ---------------------------------------------------------------------------
  // Schema format detection and parsing
  // ---------------------------------------------------------------------------

  bool
  has_extension(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    auto suffix = path.substr(path.size() - ext.size());
    // Case-insensitive comparison
    for (std::size_t i = 0; i < ext.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(suffix[i])) !=
          std::tolower(static_cast<unsigned char>(ext[i])))
        return false;
    }
    return true;
  }

  // Parse a schema file into a schema_set, auto-detecting format by extension.
  // Supports .xsd (XSD), .rng (RELAX NG XML), and .rnc (RELAX NG compact).
  void
  parse_schema_file(const std::string& file, const std::string& content,
                    xb::schema_set& schemas) {
    if (has_extension(file, ".rnc")) {
      xb::rng_compact_parser parser;
      auto pattern = parser.parse(content);
      auto simplified = xb::rng_simplify(std::move(pattern));
      auto ss = xb::rng_translate(simplified);
      for (auto& s : ss.take_schemas())
        schemas.add(std::move(s));
    } else if (has_extension(file, ".rng")) {
      xb::expat_reader reader(content);
      xb::rng_xml_parser parser;
      auto pattern = parser.parse(reader);
      auto simplified = xb::rng_simplify(std::move(pattern));
      auto ss = xb::rng_translate(simplified);
      for (auto& s : ss.take_schemas())
        schemas.add(std::move(s));
    } else {
      // Default: XSD
      xb::expat_reader reader(content);
      xb::schema_parser parser;
      schemas.add(parser.parse(reader));
    }
  }

  // ---------------------------------------------------------------------------
  // generate (root command)
  // ---------------------------------------------------------------------------

  int
  run_generate(const nlohmann::json& config) {
    // Extract schema files
    std::vector<std::string> schema_files;
    if (config.contains("schemas") && config["schemas"].is_array()) {
      for (const auto& s : config["schemas"])
        schema_files.push_back(s.get<std::string>());
    }

    if (schema_files.empty()) {
      std::cerr << "xb: no input files\n";
      return exit_usage;
    }

    // Extract options
    std::string output_dir = config.value("output-dir", ".");
    std::string type_map_file = config.value("type-map", "");
    bool list_outputs = config.value("list-outputs", false);

    // Extract namespace map from repeated pairs.
    // json-commander pair type produces [["key","val"], ...], not
    // [{"first":...}].
    std::unordered_map<std::string, std::string> namespace_map;
    if (config.contains("namespace-map") &&
        config["namespace-map"].is_array()) {
      for (const auto& pair : config["namespace-map"]) {
        namespace_map[pair[0].get<std::string>()] = pair[1].get<std::string>();
      }
    }

    // Determine output mode from flag_group
    std::string mode_str = config.value("output-mode", "split");
    xb::output_mode mode = xb::output_mode::split;
    if (mode_str == "header-only")
      mode = xb::output_mode::header_only;
    else if (mode_str == "file-per-type")
      mode = xb::output_mode::file_per_type;

    // Parse all schema files (auto-detects .xsd, .rng, .rnc by extension)
    xb::schema_set schemas;
    for (const auto& file : schema_files) {
      std::string content = read_file(file);
      try {
        parse_schema_file(file, content, schemas);
      } catch (const std::exception& e) {
        std::cerr << "xb: error parsing schema " << file << ": " << e.what()
                  << "\n";
        return exit_parse;
      }
    }

    // Resolve cross-references
    try {
      schemas.resolve();
    } catch (const std::exception& e) {
      std::cerr << "xb: schema resolution error: " << e.what() << "\n";
      return exit_parse;
    }

    // Load type map
    auto types = xb::type_map::defaults();
    if (!type_map_file.empty()) {
      std::string xml = read_file(type_map_file);
      try {
        xb::expat_reader reader(xml);
        auto overrides = xb::type_map::load(reader);
        types.merge(overrides);
      } catch (const std::exception& e) {
        std::cerr << "xb: error loading type map " << type_map_file << ": "
                  << e.what() << "\n";
        return exit_parse;
      }
    }

    // Set up codegen options
    xb::codegen_options codegen_opts;
    codegen_opts.namespace_map = namespace_map;
    codegen_opts.mode = mode;

    // Generate code
    std::vector<xb::cpp_file> files;
    try {
      xb::codegen gen(schemas, types, codegen_opts);
      files = gen.generate();
    } catch (const std::exception& e) {
      std::cerr << "xb: code generation error: " << e.what() << "\n";
      return exit_codegen;
    }

    // --list-outputs: print filenames and exit
    if (list_outputs) {
      for (const auto& file : files)
        std::cout << file.filename << "\n";
      return exit_success;
    }

    // Create output directory
    fs::create_directories(output_dir);

    // Write output files
    xb::cpp_writer writer;
    for (const auto& file : files) {
      auto path = fs::path(output_dir) / file.filename;
      std::ofstream out(path);
      if (!out) {
        std::cerr << "xb: cannot write file: " << path.string() << "\n";
        return exit_io;
      }
      out << writer.write(file);
    }

    return exit_success;
  }

  // ---------------------------------------------------------------------------
  // sample-doc subcommand
  // ---------------------------------------------------------------------------

  int
  run_sample_doc(const nlohmann::json& config) {
    std::string element_name = config.value("element", "");
    std::string namespace_uri = config.value("namespace", "");
    std::string output_file = config.value("output", "");
    bool populate_optional = config.value("populate-optional", false);
    std::size_t max_depth =
        static_cast<std::size_t>(config.value("max-depth", 20));

    std::vector<std::string> schema_files;
    if (config.contains("schemas") && config["schemas"].is_array()) {
      for (const auto& s : config["schemas"])
        schema_files.push_back(s.get<std::string>());
    }

    if (schema_files.empty()) {
      std::cerr << "xb sample-doc: no input files\n";
      return exit_usage;
    }

    // Parse all schema files (auto-detects .xsd, .rng, .rnc by extension)
    xb::schema_set schemas;
    for (const auto& file : schema_files) {
      std::string content = read_file(file);
      try {
        parse_schema_file(file, content, schemas);
      } catch (const std::exception& e) {
        std::cerr << "xb sample-doc: error parsing schema " << file << ": "
                  << e.what() << "\n";
        return exit_parse;
      }
    }

    // Resolve cross-references
    try {
      schemas.resolve();
    } catch (const std::exception& e) {
      std::cerr << "xb sample-doc: schema resolution error: " << e.what()
                << "\n";
      return exit_parse;
    }

    // Find the target element's namespace if not specified
    std::string ns_uri = namespace_uri;
    if (ns_uri.empty()) {
      const xb::element_decl* found = nullptr;
      for (const auto& s : schemas.schemas()) {
        for (const auto& e : s.elements()) {
          if (e.name().local_name() == element_name) {
            found = &e;
            ns_uri = e.name().namespace_uri();
            break;
          }
        }
        if (found) break;
      }
      if (!found) {
        std::cerr << "xb sample-doc: element '" << element_name
                  << "' not found in any schema\n";
        return exit_codegen;
      }
    }

    xb::qname element_qname{ns_uri, element_name};
    xb::doc_generator_options gen_opts;
    gen_opts.populate_optional = populate_optional;
    gen_opts.max_depth = max_depth;

    try {
      if (output_file.empty()) {
        xb::ostream_writer writer(std::cout);
        xb::doc_generator gen(schemas, gen_opts);
        gen.generate(element_qname, writer);
        std::cout << "\n";
      } else {
        std::ofstream out(output_file);
        if (!out) {
          std::cerr << "xb sample-doc: cannot write file: " << output_file
                    << "\n";
          return exit_io;
        }
        xb::ostream_writer writer(out);
        xb::doc_generator gen(schemas, gen_opts);
        gen.generate(element_qname, writer);
        out << "\n";
      }
    } catch (const std::exception& e) {
      std::cerr << "xb sample-doc: generation error: " << e.what() << "\n";
      return exit_codegen;
    }

    return exit_success;
  }

  // ---------------------------------------------------------------------------
  // fetch subcommand
  // ---------------------------------------------------------------------------

  bool
  is_http_url(const std::string& s) {
    return s.starts_with("http://") || s.starts_with("https://");
  }

#ifdef XB_HAS_CURL
  std::size_t
  xb_curl_write_cb(char* ptr, std::size_t size, std::size_t nmemb,
                   void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
  }

  std::string
  curl_fetch(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, xb_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      std::string err = curl_easy_strerror(res);
      curl_easy_cleanup(curl);
      throw std::runtime_error("fetch failed: " + url + ": " + err);
    }

    curl_easy_cleanup(curl);
    return response;
  }
#endif

  xb::transport_fn
  make_transport() {
    return [](const std::string& url) -> std::string {
      if (is_http_url(url)) {
#ifdef XB_HAS_CURL
        return curl_fetch(url);
#else
        throw std::runtime_error(
            "HTTP fetch not available (built without curl): " + url);
#endif
      }
      // Local filesystem
      std::ifstream in(url, std::ios::binary);
      if (!in) throw std::runtime_error("cannot open file: " + url);
      std::ostringstream ss;
      ss << in.rdbuf();
      return ss.str();
    };
  }

  int
  run_fetch(const nlohmann::json& config) {
    std::string url_or_path = config.value("source", "");
    std::string output_dir = config.value("output-dir", ".");
    std::string manifest_file = config.value("manifest", "");
    bool fail_fast = config.value("fail-fast", false);

    auto transport = make_transport();

    // Make the source path absolute for local files
    std::string root_url = url_or_path;
    if (!is_http_url(root_url)) root_url = fs::absolute(root_url).string();

    xb::fetch_options fetch_opts;
    fetch_opts.fail_fast = fail_fast;

    std::vector<xb::fetched_schema> schemas;
    try {
      schemas = xb::crawl_schemas(root_url, transport, fetch_opts);
    } catch (const std::exception& e) {
      std::cerr << "xb fetch: " << e.what() << "\n";
      return exit_io;
    }

    if (schemas.empty()) {
      std::cerr << "xb fetch: no schemas fetched\n";
      return exit_io;
    }

    auto entries = xb::compute_local_paths(schemas);

    // Create output directory and write files
    fs::create_directories(output_dir);

    for (std::size_t i = 0; i < schemas.size(); ++i) {
      auto out_path = fs::path(output_dir) / entries[i].local_path;
      fs::create_directories(out_path.parent_path());
      std::ofstream out(out_path, std::ios::binary);
      if (!out) {
        std::cerr << "xb fetch: cannot write: " << out_path.string() << "\n";
        return exit_io;
      }
      out << schemas[i].content;
      std::cout << entries[i].local_path << " (" << entries[i].size
                << " bytes)\n";
    }

    // Write manifest if requested
    if (!manifest_file.empty()) {
      // Get current time as ISO 8601
      auto now = std::chrono::system_clock::now();
      auto t = std::chrono::system_clock::to_time_t(now);
      char time_buf[32];
      std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ",
                    std::gmtime(&t));

      xb::fetch_manifest manifest;
      manifest.root_url = root_url;
      manifest.fetched_at = time_buf;
      manifest.schemas = entries;

      try {
        xb::write_manifest(manifest_file, manifest);
        std::cout << "Manifest: " << manifest_file << "\n";
      } catch (const std::exception& e) {
        std::cerr << "xb fetch: " << e.what() << "\n";
        return exit_io;
      }
    }

    std::cout << "Fetched " << schemas.size() << " schema(s) to " << output_dir
              << "\n";
    return exit_success;
  }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point — dispatches to subcommand handlers
// ---------------------------------------------------------------------------

int
xb_cli::run(const nlohmann::json& config) {
  // Detect subcommand from unique config keys:
  // - "element" key → sample-doc
  // - "source" key → fetch
  // - otherwise → generate (root command)
  if (config.contains("element")) return run_sample_doc(config);
  if (config.contains("source")) return run_fetch(config);
  return run_generate(config);
}
