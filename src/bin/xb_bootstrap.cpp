/// Minimal XSD-to-C++ code generator for bootstrap builds.
///
/// This binary breaks the circular dependency between the xb CLI and its
/// own generated BES encoding types.  It links xb::library (which has no wire
/// dependency) and drives the codegen pipeline directly — no JSON command
/// parsing, no BES/wire code.
///
/// Usage:
///   xb-bootstrap --output-dir DIR [--type-map FILE] [--namespace-map URI=NS]
///                 [--no-format] SCHEMA...

#include <xb/code_formatter.hpp>
#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/naming.hpp>
#include <xb/safe_output.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

  std::string
  read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      std::cerr << "xb-bootstrap: cannot open file: " << path << "\n";
      std::exit(2);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

} // namespace

int
main(int argc, char* argv[]) {
  std::string output_dir = ".";
  std::string type_map_file;
  std::vector<std::pair<std::string, std::string>> ns_map_pairs;
  bool no_format = false;
  std::vector<std::string> schema_files;

  // Simple argument parsing (no json-commander dependency)
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--output-dir" || arg == "-o") && i + 1 < argc) {
      output_dir = argv[++i];
    } else if ((arg == "--type-map" || arg == "-t") && i + 1 < argc) {
      type_map_file = argv[++i];
    } else if ((arg == "--namespace-map" || arg == "-n") && i + 1 < argc) {
      std::string val = argv[++i];
      auto eq = val.find('=');
      if (eq != std::string::npos)
        ns_map_pairs.emplace_back(val.substr(0, eq), val.substr(eq + 1));
    } else if (arg == "--no-format") {
      no_format = true;
    } else if (arg[0] != '-') {
      schema_files.push_back(arg);
    } else {
      std::cerr << "xb-bootstrap: unknown option: " << arg << "\n";
      return 1;
    }
  }

  if (schema_files.empty()) {
    std::cerr << "xb-bootstrap: no schema files specified\n";
    return 1;
  }

  // Parse schemas
  xb::schema_set schemas;
  for (const auto& file : schema_files) {
    std::string content = read_file(file);
    try {
      xb::expat_reader reader(content);
      xb::schema_parser parser;
      schemas.add(parser.parse(reader));
    } catch (const std::exception& e) {
      std::cerr << "xb-bootstrap: error parsing " << file << ": " << e.what()
                << "\n";
      return 3;
    }
  }

  try {
    schemas.resolve();
  } catch (const std::exception& e) {
    std::cerr << "xb-bootstrap: schema resolution error: " << e.what() << "\n";
    return 3;
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
      std::cerr << "xb-bootstrap: error loading type map: " << e.what() << "\n";
      return 3;
    }
  }

  // Codegen options: header-only mode
  xb::codegen_options opts;
  opts.mode = xb::output_mode::header_only;
  for (const auto& [uri, ns] : ns_map_pairs)
    opts.namespace_map[uri] = ns;

  // Generate
  std::vector<xb::cpp_file> files;
  try {
    xb::codegen gen(schemas, types, opts);
    files = gen.generate();
  } catch (const std::exception& e) {
    std::cerr << "xb-bootstrap: codegen error: " << e.what() << "\n";
    return 4;
  }

  // Write output
  fs::create_directories(output_dir);

  std::string style_file;
  if (!no_format) {
    auto style_path = fs::path(output_dir) / ".clang-format";
    if (fs::exists(style_path)) style_file = style_path.string();
  }

  xb::cpp_writer writer;
  for (const auto& file : files) {
    auto path = fs::path(output_dir) / file.filename;
    std::string code = writer.write(file);
    if (!no_format) code = xb::format_cpp_code(code, path.string(), style_file);
    auto outcome = xb::write_text_file_no_follow(path, code);
    if (outcome == xb::write_outcome::refused_symlink) {
      std::cerr << "xb-bootstrap: refusing to follow symlink at output path: "
                << path.string() << "\n";
      return 2;
    }
    if (outcome != xb::write_outcome::ok) {
      std::cerr << "xb-bootstrap: cannot write file: " << path.string() << "\n";
      return 2;
    }
  }

  return 0;
}
