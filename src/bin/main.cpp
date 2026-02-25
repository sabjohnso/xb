// GCC 12 emits a false -Wmaybe-uninitialized for std::variant containing
// std::unique_ptr in particle::term_type at -O3. Suppress it here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/doc_generator.hpp>
#include <xb/expat_reader.hpp>
#include <xb/naming.hpp>
#include <xb/ostream_writer.hpp>
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
  xb::output_mode mode = xb::output_mode::split;
  bool show_help = false;
  bool show_version = false;
  bool list_outputs = false;
};

static void
print_usage(std::ostream& os) {
  os << "Usage: xb [options] <schema.xsd> [schema2.xsd ...]\n"
     << "\n"
     << "Options:\n"
     << "  -o <dir>          Output directory (default: current directory)\n"
     << "  -t <file>         Type map override file (xb-typemap.xml)\n"
     << "  -n <uri=ns>       Namespace mapping (XML namespace URI = C++ "
        "namespace)\n"
     << "  --header-only     Generate header-only output (single .hpp)\n"
     << "  --file-per-type   Generate one header per type\n"
     << "  --list-outputs    Print expected output filenames and exit\n"
     << "  -h, --help        Show this help message\n"
     << "  --version         Show version information\n";
}

static void
print_version(std::ostream& os) {
  os << "xb " << XB_VERSION << "\n";
}

static cli_options
parse_args(int argc, char* argv[]) {
  cli_options opts;

  bool saw_header_only = false;
  bool saw_file_per_type = false;

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

    if (arg == "--header-only") {
      saw_header_only = true;
      continue;
    }

    if (arg == "--file-per-type") {
      saw_file_per_type = true;
      continue;
    }

    if (arg == "--list-outputs") {
      opts.list_outputs = true;
      continue;
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

  if (saw_header_only && saw_file_per_type) {
    std::cerr << "xb: --header-only and --file-per-type are mutually "
                 "exclusive\n";
    std::exit(exit_usage);
  }

  if (saw_header_only)
    opts.mode = xb::output_mode::header_only;
  else if (saw_file_per_type)
    opts.mode = xb::output_mode::file_per_type;

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
  codegen_opts.mode = opts.mode;

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
  if (opts.list_outputs) {
    for (const auto& file : files)
      std::cout << file.filename << "\n";
    return exit_success;
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

// ---------------------------------------------------------------------------
// sample-doc subcommand
// ---------------------------------------------------------------------------

struct sample_doc_options {
  std::string element_name;
  std::string namespace_uri;
  std::vector<std::string> schema_files;
  std::string output_file;
  bool populate_optional = false;
  std::size_t max_depth = 20;
  bool show_help = false;
};

static void
print_sample_doc_usage(std::ostream& os) {
  os << "Usage: xb sample-doc --element <name> [options] <schema.xsd> "
        "[...]\n"
     << "\n"
     << "Options:\n"
     << "  --element <name>       Target element local name (required)\n"
     << "  --namespace <uri>      Target element namespace URI\n"
     << "  --populate-optional    Include optional elements and attributes\n"
     << "  --max-depth <N>        Recursion depth limit (default: 20)\n"
     << "  --output <file>        Output file (default: stdout)\n"
     << "  -h, --help             Show this help message\n";
}

static sample_doc_options
parse_sample_doc_args(int argc, char* argv[]) {
  sample_doc_options opts;

  // argv[0] is "xb", argv[1] is "sample-doc", start at 2
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      opts.show_help = true;
      return opts;
    }

    if (arg == "--element") {
      if (i + 1 >= argc) {
        std::cerr << "xb sample-doc: --element requires an argument\n";
        std::exit(exit_usage);
      }
      opts.element_name = argv[++i];
      continue;
    }

    if (arg == "--namespace") {
      if (i + 1 >= argc) {
        std::cerr << "xb sample-doc: --namespace requires an argument\n";
        std::exit(exit_usage);
      }
      opts.namespace_uri = argv[++i];
      continue;
    }

    if (arg == "--populate-optional") {
      opts.populate_optional = true;
      continue;
    }

    if (arg == "--max-depth") {
      if (i + 1 >= argc) {
        std::cerr << "xb sample-doc: --max-depth requires an argument\n";
        std::exit(exit_usage);
      }
      opts.max_depth = std::stoul(argv[++i]);
      continue;
    }

    if (arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "xb sample-doc: --output requires an argument\n";
        std::exit(exit_usage);
      }
      opts.output_file = argv[++i];
      continue;
    }

    if (arg[0] == '-') {
      std::cerr << "xb sample-doc: unknown option: " << arg << "\n";
      std::exit(exit_usage);
    }

    opts.schema_files.push_back(arg);
  }

  return opts;
}

static int
run_sample_doc(const sample_doc_options& opts) {
  // Parse all schema files
  xb::schema_set schemas;
  for (const auto& file : opts.schema_files) {
    std::string xml = read_file(file);
    try {
      xb::expat_reader reader(xml);
      xb::schema_parser parser;
      schemas.add(parser.parse(reader));
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
    std::cerr << "xb sample-doc: schema resolution error: " << e.what() << "\n";
    return exit_parse;
  }

  // Find the target element's namespace if not specified
  std::string ns_uri = opts.namespace_uri;
  if (ns_uri.empty()) {
    const xb::element_decl* found = nullptr;
    for (const auto& s : schemas.schemas()) {
      for (const auto& e : s.elements()) {
        if (e.name().local_name() == opts.element_name) {
          found = &e;
          ns_uri = e.name().namespace_uri();
          break;
        }
      }
      if (found) break;
    }
    if (!found) {
      std::cerr << "xb sample-doc: element '" << opts.element_name
                << "' not found in any schema\n";
      return exit_codegen;
    }
  }

  xb::qname element_qname{ns_uri, opts.element_name};
  xb::doc_generator_options gen_opts;
  gen_opts.populate_optional = opts.populate_optional;
  gen_opts.max_depth = opts.max_depth;

  try {
    if (opts.output_file.empty()) {
      xb::ostream_writer writer(std::cout);
      xb::doc_generator gen(schemas, gen_opts);
      gen.generate(element_qname, writer);
      std::cout << "\n";
    } else {
      std::ofstream out(opts.output_file);
      if (!out) {
        std::cerr << "xb sample-doc: cannot write file: " << opts.output_file
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

int
main(int argc, char* argv[]) {
  // Check for subcommand
  if (argc >= 2 && std::string(argv[1]) == "sample-doc") {
    auto opts = parse_sample_doc_args(argc, argv);

    if (opts.show_help) {
      print_sample_doc_usage(std::cerr);
      return exit_success;
    }

    if (opts.element_name.empty()) {
      std::cerr << "xb sample-doc: --element is required\n";
      print_sample_doc_usage(std::cerr);
      return exit_usage;
    }

    if (opts.schema_files.empty()) {
      std::cerr << "xb sample-doc: no input files\n";
      print_sample_doc_usage(std::cerr);
      return exit_usage;
    }

    return run_sample_doc(opts);
  }

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
