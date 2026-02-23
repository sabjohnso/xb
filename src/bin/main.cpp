// GCC 12 emits a false -Wmaybe-uninitialized for std::variant containing
// std::unique_ptr in particle::term_type at -O3. Suppress it here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/naming.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static constexpr int exit_success = 0;
static constexpr int exit_usage = 1;
static constexpr int exit_io = 2;
static constexpr int exit_parse = 3;
static constexpr int exit_codegen = 4;

struct cli_options {
  std::vector<std::string> schema_files;
  std::string output_dir = ".";
  std::string type_map_file;
  std::unordered_map<std::string, std::string> namespace_map;
  bool show_help = false;
  bool show_version = false;
};

static void
print_usage(std::ostream& os) {
  os << "Usage: xb [options] <schema.xsd> [schema2.xsd ...]\n"
     << "\n"
     << "Options:\n"
     << "  -o <dir>        Output directory (default: current directory)\n"
     << "  -t <file>       Type map override file (xb-typemap.xml)\n"
     << "  -n <uri=ns>     Namespace mapping (XML namespace URI = C++ "
        "namespace)\n"
     << "  -h, --help      Show this help message\n"
     << "  --version       Show version information\n";
}

static void
print_version(std::ostream& os) {
  os << "xb " << XB_VERSION << "\n";
}

static cli_options
parse_args(int argc, char* argv[]) {
  cli_options opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      opts.show_help = true;
      return opts;
    }

    if (arg == "--version") {
      opts.show_version = true;
      return opts;
    }

    if (arg == "-o") {
      if (i + 1 >= argc) {
        std::cerr << "xb: -o requires an argument\n";
        std::exit(exit_usage);
      }
      opts.output_dir = argv[++i];
      continue;
    }

    if (arg == "-t") {
      if (i + 1 >= argc) {
        std::cerr << "xb: -t requires an argument\n";
        std::exit(exit_usage);
      }
      opts.type_map_file = argv[++i];
      continue;
    }

    if (arg == "-n") {
      if (i + 1 >= argc) {
        std::cerr << "xb: -n requires an argument\n";
        std::exit(exit_usage);
      }
      std::string mapping = argv[++i];
      auto eq = mapping.find('=');
      if (eq == std::string::npos) {
        std::cerr << "xb: -n argument must be URI=namespace\n";
        std::exit(exit_usage);
      }
      opts.namespace_map[mapping.substr(0, eq)] = mapping.substr(eq + 1);
      continue;
    }

    if (arg[0] == '-') {
      std::cerr << "xb: unknown option: " << arg << "\n";
      std::exit(exit_usage);
    }

    opts.schema_files.push_back(arg);
  }

  return opts;
}

static std::string
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

static int
run(const cli_options& opts) {
  // Parse all schema files
  xb::schema_set schemas;
  for (const auto& file : opts.schema_files) {
    std::string xml = read_file(file);
    try {
      xb::expat_reader reader(xml);
      xb::schema_parser parser;
      schemas.add(parser.parse(reader));
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
  if (!opts.type_map_file.empty()) {
    std::string xml = read_file(opts.type_map_file);
    try {
      xb::expat_reader reader(xml);
      auto overrides = xb::type_map::load(reader);
      types.merge(overrides);
    } catch (const std::exception& e) {
      std::cerr << "xb: error loading type map " << opts.type_map_file << ": "
                << e.what() << "\n";
      return exit_parse;
    }
  }

  // Set up codegen options
  xb::codegen_options codegen_opts;
  codegen_opts.namespace_map = opts.namespace_map;

  // Generate code
  std::vector<xb::cpp_file> files;
  try {
    xb::codegen gen(schemas, types, codegen_opts);
    files = gen.generate();
  } catch (const std::exception& e) {
    std::cerr << "xb: code generation error: " << e.what() << "\n";
    return exit_codegen;
  }

  // Create output directory
  fs::create_directories(opts.output_dir);

  // Write output files
  xb::cpp_writer writer;
  for (const auto& file : files) {
    auto path = fs::path(opts.output_dir) / file.filename;
    std::ofstream out(path);
    if (!out) {
      std::cerr << "xb: cannot write file: " << path.string() << "\n";
      return exit_io;
    }
    out << writer.write(file);
  }

  return exit_success;
}

int
main(int argc, char* argv[]) {
  cli_options opts = parse_args(argc, argv);

  if (opts.show_help) {
    print_usage(std::cerr);
    return exit_success;
  }

  if (opts.show_version) {
    print_version(std::cerr);
    return exit_success;
  }

  if (opts.schema_files.empty()) {
    std::cerr << "xb: no input files\n";
    print_usage(std::cerr);
    return exit_usage;
  }

  return run(opts);
}
