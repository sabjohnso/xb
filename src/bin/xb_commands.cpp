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
#include <xb/dtd_parser.hpp>
#include <xb/dtd_to_rng.hpp>
#include <xb/dtd_translator.hpp>
#include <xb/dtd_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/naming.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/rng_compact_parser.hpp>
#include <xb/rng_compact_writer.hpp>
#include <xb/rng_parser.hpp>
#include <xb/rng_simplify.hpp>
#include <xb/rng_translator.hpp>
#include <xb/rng_writer.hpp>
#include <xb/schema_fetcher.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/schema_to_dtd.hpp>
#include <xb/schematron_overlay.hpp>
#include <xb/schematron_parser.hpp>
#include <xb/type_map.hpp>
#include <xb/xsd_to_rng.hpp>
#include <xb/xsd_writer.hpp>

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
  // Supports .xsd (XSD), .rng (RELAX NG XML), .rnc (RELAX NG compact),
  // and .dtd (DTD).
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
    } else if (has_extension(file, ".dtd")) {
      xb::dtd_parser parser;
      auto doc = parser.parse(content);
      auto ss = xb::dtd_translate(doc);
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

    // Separate structural schemas from Schematron files
    std::vector<std::string> sch_files;
    xb::schema_set schemas;
    for (const auto& file : schema_files) {
      std::string content = read_file(file);
      if (has_extension(file, ".sch")) {
        sch_files.push_back(file);
        continue;
      }
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

    // Apply Schematron overlays (after resolution)
    for (const auto& file : sch_files) {
      std::string content = read_file(file);
      try {
        xb::expat_reader reader(content);
        xb::schematron_parser sch_parser;
        auto sch = sch_parser.parse(reader);
        auto ov = xb::schematron_overlay(schemas, sch);
        for (const auto& w : ov.warnings)
          std::cerr << "xb: schematron warning: " << w << "\n";
      } catch (const std::exception& e) {
        std::cerr << "xb: error parsing schematron " << file << ": " << e.what()
                  << "\n";
        return exit_parse;
      }
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

    // Parse naming style string to enum
    auto parse_naming_style = [](const std::string& s) -> xb::naming_style {
      if (s == "pascal") return xb::naming_style::pascal_case;
      if (s == "camel") return xb::naming_style::camel_case;
      if (s == "upper-snake") return xb::naming_style::upper_snake;
      if (s == "original") return xb::naming_style::original;
      return xb::naming_style::snake_case;
    };

    // Set up codegen options
    xb::codegen_options codegen_opts;
    codegen_opts.namespace_map = namespace_map;
    codegen_opts.mode = mode;
    codegen_opts.header_suffix = config.value("header-suffix", ".hpp");
    codegen_opts.source_suffix = config.value("source-suffix", ".cpp");
    codegen_opts.generate_docs = config.value("generate-docs", false);
    codegen_opts.separate_fwd_header =
        config.value("separate-fwd-header", false);
    codegen_opts.naming.type_style =
        parse_naming_style(config.value("type-style", "snake"));
    codegen_opts.naming.field_style =
        parse_naming_style(config.value("field-style", "snake"));
    codegen_opts.naming.enum_style =
        parse_naming_style(config.value("enum-style", "snake"));

    std::string encap_str = config.value("encapsulation", "raw-struct");
    if (encap_str == "wrapped")
      codegen_opts.encapsulation = xb::encapsulation_mode::wrapped;

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

    // Separate structural schemas from Schematron files
    std::vector<std::string> sch_files;
    xb::schema_set schemas;
    for (const auto& file : schema_files) {
      if (has_extension(file, ".sch")) {
        sch_files.push_back(file);
        continue;
      }
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

    // Apply Schematron overlays
    for (const auto& file : sch_files) {
      std::string content = read_file(file);
      try {
        xb::expat_reader reader(content);
        xb::schematron_parser sch_parser;
        auto sch = sch_parser.parse(reader);
        xb::schematron_overlay(schemas, sch);
      } catch (const std::exception& e) {
        std::cerr << "xb sample-doc: error parsing schematron " << file << ": "
                  << e.what() << "\n";
        return exit_parse;
      }
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

  // ---------------------------------------------------------------------------
  // convert subcommand
  // ---------------------------------------------------------------------------

  int
  run_convert(const nlohmann::json& config) {
    std::string input_file = config.value("input", "");
    std::string output_format = config.value("output-format", "");

    if (input_file.empty()) {
      std::cerr << "xb convert: no input file\n";
      return exit_usage;
    }

    std::string content = read_file(input_file);

    // Auto-detect input format
    bool input_is_rng = has_extension(input_file, ".rng");
    bool input_is_rnc = has_extension(input_file, ".rnc");
    bool input_is_xsd = has_extension(input_file, ".xsd");
    bool input_is_dtd = has_extension(input_file, ".dtd");

    if (!input_is_rng && !input_is_rnc && !input_is_xsd && !input_is_dtd) {
      std::cerr << "xb convert: cannot detect format from extension: "
                << input_file << " (expected .rng, .rnc, .xsd, or .dtd)\n";
      return exit_usage;
    }

    // Default output format
    if (output_format.empty()) {
      if (input_is_xsd || input_is_dtd) {
        output_format = "rng";
      } else {
        output_format = input_is_rng ? "rnc" : "rng";
      }
    }

    if (output_format != "rng" && output_format != "rnc" &&
        output_format != "dtd" && output_format != "xsd") {
      std::cerr << "xb convert: unknown output format: " << output_format
                << " (expected 'rng', 'rnc', 'dtd', or 'xsd')\n";
      return exit_usage;
    }

    // XSD output path: input → schema_set → xsd_write
    if (output_format == "xsd") {
      xb::schema_set ss = [&]() -> xb::schema_set {
        if (input_is_xsd) {
          xb::expat_reader reader(content);
          xb::schema_parser sp;
          xb::schema_set result;
          result.add(sp.parse(reader));
          result.resolve();
          return result;
        }
        if (input_is_dtd) {
          xb::dtd_parser dp;
          return xb::dtd_translate(dp.parse(content));
        }
        // RNG/RNC → schema_set
        xb::rng::pattern pat = [&]() -> xb::rng::pattern {
          if (input_is_rnc) {
            xb::rng_compact_parser parser;
            return parser.parse(content);
          }
          xb::expat_reader reader(content);
          xb::rng_xml_parser parser;
          return parser.parse(reader);
        }();
        auto simplified = xb::rng_simplify(std::move(pat));
        return xb::rng_translate(simplified);
      }();

      int indent = config.value("indent", -1);
      if (indent < 0) indent = 0;

      for (const auto& schema : ss.schemas()) {
        if (indent > 0) {
          std::cout << xb::xsd_write_string(schema, indent);
        } else {
          std::cout << xb::xsd_write_string(schema);
        }
      }
      return exit_success;
    }

    // DTD output path: input → schema_set → dtd::document → text
    if (output_format == "dtd") {
      xb::schema_set ss = [&]() -> xb::schema_set {
        if (input_is_xsd) {
          xb::expat_reader reader(content);
          xb::schema_parser sp;
          xb::schema_set result;
          result.add(sp.parse(reader));
          result.resolve();
          return result;
        }
        if (input_is_dtd) {
          xb::dtd_parser dp;
          return xb::dtd_translate(dp.parse(content));
        }
        // RNG/RNC → XSD → DTD
        xb::rng::pattern pat = [&]() -> xb::rng::pattern {
          if (input_is_rnc) {
            xb::rng_compact_parser parser;
            return parser.parse(content);
          }
          xb::expat_reader reader(content);
          xb::rng_xml_parser parser;
          return parser.parse(reader);
        }();
        auto simplified = xb::rng_simplify(std::move(pat));
        return xb::rng_translate(simplified);
      }();
      auto doc = xb::schema_to_dtd(ss);
      std::cout << xb::dtd_write(doc);
      return exit_success;
    }

    // RNG/RNC output path: input → rng::pattern → text
    xb::rng::pattern pattern = [&]() -> xb::rng::pattern {
      if (input_is_dtd) {
        xb::dtd_parser dp;
        return xb::dtd_to_rng(dp.parse(content));
      }
      if (input_is_xsd) {
        xb::expat_reader reader(content);
        xb::schema_parser sp;
        xb::schema_set ss;
        ss.add(sp.parse(reader));
        ss.resolve();
        return xb::xsd_to_rng(ss);
      }
      if (input_is_rnc) {
        xb::rng_compact_parser parser;
        return parser.parse(content);
      }
      xb::expat_reader reader(content);
      xb::rng_xml_parser parser;
      return parser.parse(reader);
    }();

    // Determine indent: explicit --indent, or default 2 for RNC
    int indent = config.value("indent", -1);
    if (indent < 0) { indent = (output_format == "rnc") ? 2 : 0; }

    // Write output
    if (output_format == "rnc") {
      std::cout << xb::rng_compact_write(pattern, indent);
    } else if (indent > 0) {
      std::cout << xb::rng_write_string(pattern, indent);
    } else {
      std::cout << xb::rng_write_string(pattern) << "\n";
    }

    return exit_success;
  }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point — dispatches to subcommand handlers
// ---------------------------------------------------------------------------

int
xb_cli::run(const nlohmann::json& config) {
  // Detect subcommand from unique config keys:
  // - "input" key → convert
  // - "element" key → sample-doc
  // - "source" key → fetch
  // - otherwise → generate (root command)
  if (config.contains("input")) return run_convert(config);
  if (config.contains("element")) return run_sample_doc(config);
  if (config.contains("source")) return run_fetch(config);
  return run_generate(config);
}
