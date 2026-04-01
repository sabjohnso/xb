#include "xb_main.hpp"

#include <xb/code_formatter.hpp>
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
#include <xb/railroad.hpp>
#include <xb/railroad_svg.hpp>
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
#include <xb/wire/bes_resolver.hpp>
#include <xb/wire/bes_to_xsd.hpp>
#include <xb/wire/binary_codegen.hpp>
#include <xb/wire/encoding.hpp>
#include <xb/wire/encoding_resolver.hpp>
#include <xb/wire/layout_engine.hpp>
#include <xb/wire/layout_validation.hpp>
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

    // Extract options
    std::string output_dir = config.value("output-dir", ".");
    std::string type_map_file = config.value("type-map", "");
    bool list_outputs = config.value("list-outputs", false);
    bool no_format = config.value("no-format", false);

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

    // Check: BES-only mode requires --encoding
    bool binary_only = config.value("binary-only", false);
    std::string encoding_file = config.value("encoding", "");
    bool bes_only_mode =
        binary_only && !encoding_file.empty() && schema_files.empty();

    if (schema_files.empty() && !bes_only_mode) {
      std::cerr << "xb: no input files\n";
      return exit_usage;
    }

    // Separate structural schemas from Schematron files
    std::vector<std::string> sch_files;
    xb::schema_set schemas;

    if (bes_only_mode) {
      // BES-only mode: synthesize schemas from BES
      try {
        std::string bes_xml = read_file(encoding_file);
        xb::expat_reader bes_reader(bes_xml);
        bes_reader.read();
        auto bes_encoding = xb::bes::read_encoding_type(bes_reader);
        auto resolved = xb::wire::resolve_imports(
            std::move(bes_encoding),
            [&](const std::string& href) -> xb::bes::encoding_type {
              auto import_path =
                  (fs::path(encoding_file).parent_path() / href).string();
              std::string import_xml = read_file(import_path);
              xb::expat_reader import_reader(import_xml);
              import_reader.read();
              return xb::bes::read_encoding_type(import_reader);
            });
        schemas = xb::wire::build_synthetic_schemas(resolved);
      } catch (const std::exception& e) {
        std::cerr << "xb: error building synthetic schemas from BES: "
                  << e.what() << "\n";
        return exit_parse;
      }
    } else {
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
          std::cerr << "xb: error parsing schematron " << file << ": "
                    << e.what() << "\n";
          return exit_parse;
        }
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

    std::string ns_style_str = config.value("namespace-style", "short");
    if (ns_style_str == "full")
      codegen_opts.ns_style = xb::namespace_style::full_uri;

    // Generate XML types (unless --binary-only)
    std::vector<xb::cpp_file> files;
    if (!binary_only) {
      try {
        xb::codegen gen(schemas, types, codegen_opts);
        files = gen.generate();
      } catch (const std::exception& e) {
        std::cerr << "xb: code generation error: " << e.what() << "\n";
        return exit_codegen;
      }
    }

    // Generate binary types (when --encoding is provided)
    std::string binary_header_text;
    if (!encoding_file.empty()) {
      try {
        // Parse BES document
        std::string bes_xml = read_file(encoding_file);
        xb::expat_reader bes_reader(bes_xml);
        bes_reader.read(); // advance to root element
        auto encoding = xb::bes::read_encoding_type(bes_reader);

        // Resolve imports
        auto resolved = xb::wire::resolve_imports(
            std::move(encoding),
            [&](const std::string& href) -> xb::bes::encoding_type {
              auto import_path =
                  (fs::path(encoding_file).parent_path() / href).string();
              std::string import_xml = read_file(import_path);
              xb::expat_reader import_reader(import_xml);
              import_reader.read();
              return xb::bes::read_encoding_type(import_reader);
            });

        // Build encoding plan
        xb::wire::encoding_plan<xb::bes::encoding_type> plan(resolved, schemas);

        // Get defaults
        xb::bes::defaults_type defaults;
        if (resolved.defaults.has_value()) defaults = *resolved.defaults;

        // Generate binary types for each bound message
        std::ostringstream binary_header;
        binary_header << "#pragma once\n\n";
        binary_header << "#include <cstddef>\n";
        binary_header << "#include <cstdint>\n";
        binary_header << "#include <cstring>\n";
        binary_header << "#include <optional>\n";
        binary_header << "#include <span>\n";
        binary_header << "#include <stdexcept>\n";
        binary_header << "#include <string_view>\n";
        binary_header << "#include <variant>\n";
        binary_header << "#include <vector>\n\n";
        binary_header << "#include <xb/wire/bits.hpp>\n";
        binary_header << "#include <xb/wire/byteswap.hpp>\n";
        binary_header << "#include <xb/wire/types.hpp>\n\n";

        // Derive C++ namespace from BES target-namespace
        std::string binary_ns;
        if (resolved.target_namespace.has_value() &&
            !resolved.target_namespace->empty()) {
          // Check namespace_map for explicit override first
          auto ns_it = namespace_map.find(*resolved.target_namespace);
          if (ns_it != namespace_map.end()) {
            binary_ns = ns_it->second;
          } else if (codegen_opts.ns_style == xb::namespace_style::short_name) {
            std::set<std::string> all_ns;
            if (resolved.target_namespace.has_value())
              all_ns.insert(*resolved.target_namespace);
            binary_ns = xb::default_namespace_for(*resolved.target_namespace,
                                                  all_ns, codegen_opts);
          } else {
            binary_ns =
                xb::cpp_namespace_for(*resolved.target_namespace, codegen_opts);
          }
        }

        if (!binary_ns.empty()) {
          binary_header << "namespace " << binary_ns << " {\n\n";
        }

        // Forward-declare all view/owned classes so that cross-references
        // (e.g. repeat element-type) resolve regardless of generation order.
        for (const auto& bound : plan.messages()) {
          auto base = bound.message->name;
          binary_header << "template <xb::wire::validation_level>\n"
                        << "class " << base << "_view;\n";
          binary_header << "template <xb::wire::validation_level>\n"
                        << "class " << base << "_mutable_view;\n";
          binary_header << "class " << base << "_owned;\n";
        }
        binary_header << "\n";

        // Collect discriminator entries for framing dispatch
        std::vector<xb::wire::message_entry> disc_entries;

        // Sort messages so that types referenced via element-type are
        // generated before messages that reference them.
        auto sorted_messages = [&]() {
          auto msgs = plan.messages();
          // Collect names referenced via element-type
          std::set<std::string> referenced;
          for (const auto& bound : msgs) {
            for (const auto& item : bound.message->choice) {
              std::visit(
                  [&](const auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<
                                      V,
                                      std::unique_ptr<xb::bes::repeat_type>>) {
                      if (v && v->element_type.has_value())
                        referenced.insert(*v->element_type);
                    }
                  },
                  item);
            }
          }
          // Two-pass: referenced types first, then the rest
          decltype(msgs) result;
          for (const auto& b : msgs)
            if (referenced.count(b.message->name)) result.push_back(b);
          for (const auto& b : msgs)
            if (!referenced.count(b.message->name)) result.push_back(b);
          return result;
        }();

        for (const auto& bound : sorted_messages) {
          auto& msg = *bound.message;
          auto layout = xb::wire::compute_layout(msg, defaults);

          // Validate field widths against XSD type annotations
          auto field_errors = xb::wire::validate_message_fields(msg);
          for (const auto& err : field_errors) {
            std::cerr << "xb: warning: " << err.message << "\n";
          }

          std::string base_name = msg.name;

          // Concept
          binary_header << xb::wire::generate_concept(base_name + "_message",
                                                      msg, layout, defaults);
          binary_header << "\n";

          // Base class
          binary_header << xb::wire::generate_base_class(base_name + "_base",
                                                         msg, layout, defaults);
          binary_header << "\n";

          // View class
          binary_header << xb::wire::generate_view_class(base_name + "_view",
                                                         msg, layout, defaults);
          binary_header << "\n";

          // Mutable view class
          binary_header << xb::wire::generate_mutable_view_class(
              base_name + "_mutable_view", msg, layout, defaults);
          binary_header << "\n";

          // Owned class
          binary_header << xb::wire::generate_owned_class(
              base_name + "_owned", msg, layout, defaults);
          binary_header << "\n";

          // Collect discriminator entry if message has a value
          if (msg.discriminant_value.has_value()) {
            disc_entries.push_back(
                {base_name + "_view", *msg.discriminant_value});
          }
        }

        // Generate discriminator if we have entries and framing
        if (!disc_entries.empty()) {
          // Find discriminant field from first framing
          const auto* framing = [&]() -> const xb::bes::framing_type* {
            for (const auto& bound : plan.messages()) {
              if (bound.message->framing.has_value()) {
                return plan.find_framing(*bound.message->framing);
              }
            }
            return nullptr;
          }();

          if (framing) {
            // Find discriminant field offset in framing
            xb::bes::defaults_type framing_defaults = defaults;
            auto framing_layout =
                xb::wire::compute_layout(*framing, framing_defaults);

            for (const auto& item : framing->choice) {
              std::visit(
                  [&](const auto& v) {
                    if constexpr (requires { v.field; }) {
                      // discriminant_type — find the field
                      for (const auto& fl : framing_layout.fields) {
                        if (fl.name == v.field && fl.offset_bits.has_value()) {
                          xb::wire::discriminant_info disc_info;
                          disc_info.field_name = v.field;
                          disc_info.offset_bits = *fl.offset_bits;
                          disc_info.width_bits = fl.width_bits;

                          // Resolve byte order for discriminant field
                          // Check the field in the framing for per-field
                          // override
                          disc_info.endian = "std::endian::big";
                          for (const auto& fi : framing->choice) {
                            std::visit(
                                [&](const auto& fv) {
                                  if constexpr (requires {
                                                  fv.name;
                                                  fv.byte_order;
                                                }) {
                                    if (fv.name == v.field &&
                                        fv.byte_order.has_value()) {
                                      auto s =
                                          xb::bes::to_string(*fv.byte_order);
                                      if (s == "little-endian")
                                        disc_info.endian =
                                            "std::endian::little";
                                      else if (s == "native")
                                        disc_info.endian =
                                            "std::endian::native";
                                    }
                                  }
                                },
                                fi);
                          }
                          // Fall back to framing byte_order
                          if (disc_info.endian == "std::endian::big" &&
                              framing->byte_order.has_value()) {
                            auto s = xb::bes::to_string(*framing->byte_order);
                            if (s == "little-endian")
                              disc_info.endian = "std::endian::little";
                            else if (s == "native")
                              disc_info.endian = "std::endian::native";
                          }
                          // Fall back to global defaults
                          if (disc_info.endian == "std::endian::big" &&
                              defaults.byte_order.has_value()) {
                            auto s = xb::bes::to_string(*defaults.byte_order);
                            if (s == "little-endian")
                              disc_info.endian = "std::endian::little";
                            else if (s == "native")
                              disc_info.endian = "std::endian::native";
                          }

                          binary_header << xb::wire::generate_discriminator(
                              "discriminate", disc_info, disc_entries);
                        }
                      }
                    }
                  },
                  item);
            }
          }
        }

        // Build framing-name → message-view-name map.
        // Convention: the BES has matching message and framing entries
        // with the same field layout. We find messages whose name
        // matches a framing name (case-insensitive prefix match) or
        // whose fields match.
        // Sanitize name for C++ identifier (replace - with _)
        auto sanitize = [](std::string s) {
          for (auto& c : s)
            if (c == '-') c = '_';
          return s;
        };

        // Strip non-alphanumeric and lowercase for fuzzy matching
        auto normalize = [](const std::string& s) {
          std::string r;
          for (auto c : s)
            if (std::isalnum(static_cast<unsigned char>(c)))
              r += static_cast<char>(
                  std::tolower(static_cast<unsigned char>(c)));
          return r;
        };

        auto find_view_for_framing =
            [&](const std::string& framing_name) -> std::string {
          auto frm_norm = normalize(framing_name);
          for (const auto& bound : plan.messages()) {
            auto msg_norm = normalize(bound.message->name);
            if (msg_norm.starts_with(frm_norm) ||
                frm_norm.starts_with(msg_norm))
              return bound.message->name + "_view";
          }
          return sanitize(framing_name) + "_view";
        };

        // Generate frame parsers for each frame-stack
        for (const auto& choice_item : resolved.choice) {
          const auto* fs = std::get_if<xb::bes::frame_stack_type>(&choice_item);
          if (!fs) continue;

          std::vector<xb::wire::frame_layer> layers;

          for (const auto& layer : fs->layer) {
            const auto* framing = plan.find_framing(layer.ref);
            if (!framing) continue;

            xb::wire::frame_layer fl;
            fl.view_class_name = find_view_for_framing(layer.ref);

            // Match condition from layer
            if (layer.match.has_value()) fl.match_field = *layer.match;
            if (layer.value.has_value()) fl.match_value = *layer.value;

            // Compute fixed header size (fields before any repeat)
            unsigned fixed_bits = 0;
            bool has_repeat = false;
            for (const auto& fi : framing->choice) {
              if (std::holds_alternative<std::unique_ptr<xb::bes::repeat_type>>(
                      fi)) {
                has_repeat = true;

                auto& rp = std::get<std::unique_ptr<xb::bes::repeat_type>>(fi);
                if (rp && rp->count_field.has_value()) {
                  fl.count_field = *rp->count_field;

                  for (const auto& ri : rp->choice) {
                    if (auto* lf = std::get_if<xb::bes::length_type>(&ri))
                      fl.block_length_field = lf->field;
                    if (auto* ff = std::get_if<xb::bes::field_type>(&ri))
                      fl.block_wire_size += (ff->bits + 7) / 8;
                  }

                  // Find the message block view class
                  fl.block_view_class =
                      find_view_for_framing("moldudp64-message-block");
                }
                break; // stop summing fixed bits
              }

              // Sum field bits for the fixed header portion
              std::visit(
                  [&](const auto& v) {
                    if constexpr (requires { v.bits; }) {
                      if constexpr (std::is_integral_v<
                                        std::decay_t<decltype(v.bits)>>)
                        fixed_bits += v.bits;
                    }
                  },
                  fi);
            }

            if (!has_repeat) {
              // Fully fixed framing — use compute_layout
              auto framing_layout =
                  xb::wire::compute_layout(*framing, defaults);
              fl.wire_size_bytes = framing_layout.is_fixed
                                       ? (framing_layout.total_bits + 7) / 8
                                       : (fixed_bits + 7) / 8;
            } else {
              fl.wire_size_bytes = (fixed_bits + 7) / 8;
            }

            layers.push_back(std::move(fl));
          }

          if (!layers.empty()) {
            binary_header << xb::wire::generate_frame_parser(
                "parse_" + sanitize(fs->name), layers);
            binary_header << "\n";
          }
        }

        if (!binary_ns.empty()) {
          binary_header << "} // namespace " << binary_ns << "\n";
        }

        binary_header_text = binary_header.str();

      } catch (const std::exception& e) {
        std::cerr << "xb: binary encoding error: " << e.what() << "\n";
        return exit_codegen;
      }
    }

    // --list-outputs: print filenames and exit
    if (list_outputs) {
      for (const auto& file : files)
        std::cout << file.filename << "\n";
      if (!binary_header_text.empty()) std::cout << "wire_types.hpp\n";
      return exit_success;
    }

    // Create output directory
    fs::create_directories(output_dir);

    // Locate .clang-format style file for formatting
    std::string style_file;
    if (!no_format) {
      auto style_path = fs::path(output_dir) / ".clang-format";
      if (fs::exists(style_path)) style_file = style_path.string();
    }

    // Write output files
    xb::cpp_writer writer;
    for (const auto& file : files) {
      auto path = fs::path(output_dir) / file.filename;
      std::ofstream out(path);
      if (!out) {
        std::cerr << "xb: cannot write file: " << path.string() << "\n";
        return exit_io;
      }
      std::string code = writer.write(file);
      if (!no_format)
        code = xb::format_cpp_code(code, path.string(), style_file);
      out << code;
    }

    // Write binary header directly (not through cpp_writer)
    if (!binary_header_text.empty()) {
      auto path = fs::path(output_dir) / "wire_types.hpp";
      std::ofstream out(path);
      if (!out) {
        std::cerr << "xb: cannot write file: " << path.string() << "\n";
        return exit_io;
      }
      if (!no_format)
        binary_header_text =
            xb::format_cpp_code(binary_header_text, path.string(), style_file);
      out << binary_header_text;
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
  // railroad subcommand
  // ---------------------------------------------------------------------------

  int
  run_railroad(const nlohmann::json& config) {
    std::string type_name = config.value("type", "");
    std::string element_name = config.value("element", "");
    std::string namespace_uri = config.value("namespace", "");
    std::string output_file = config.value("output", "");

    std::vector<std::string> schema_files;
    if (config.contains("schemas") && config["schemas"].is_array()) {
      for (const auto& s : config["schemas"])
        schema_files.push_back(s.get<std::string>());
    }

    if (schema_files.empty()) {
      std::cerr << "xb railroad: no input files\n";
      return exit_usage;
    }

    if (type_name.empty() && element_name.empty()) {
      std::cerr
          << "xb railroad: either --type or --element must be specified\n";
      return exit_usage;
    }

    // Load & parse schemas
    xb::schema_set schemas;
    for (const auto& file : schema_files) {
      std::string content = read_file(file);
      try {
        parse_schema_file(file, content, schemas);
      } catch (const std::exception& e) {
        std::cerr << "xb railroad: error parsing schema " << file << ": "
                  << e.what() << "\n";
        return exit_parse;
      }
    }

    try {
      schemas.resolve();
    } catch (const std::exception& e) {
      std::cerr << "xb railroad: schema resolution error: " << e.what() << "\n";
      return exit_parse;
    }

    // Resolve namespace if not specified
    std::string ns_uri = namespace_uri;
    if (ns_uri.empty()) {
      std::string target = type_name.empty() ? element_name : type_name;
      for (const auto& s : schemas.schemas()) {
        if (!type_name.empty()) {
          for (const auto& ct : s.complex_types()) {
            if (ct.name().local_name() == target) {
              ns_uri = ct.name().namespace_uri();
              break;
            }
          }
        }
        if (!element_name.empty() && ns_uri.empty()) {
          for (const auto& e : s.elements()) {
            if (e.name().local_name() == target) {
              ns_uri = e.name().namespace_uri();
              break;
            }
          }
        }
        if (!ns_uri.empty()) break;
      }
    }

    // Build diagrams
    std::vector<xb::railroad::diagram> diagrams;
    try {
      if (!type_name.empty()) {
        diagrams.push_back(
            xb::railroad::build_diagram(schemas, xb::qname{ns_uri, type_name}));
      } else {
        diagrams = xb::railroad::build_diagrams(
            schemas, xb::qname{ns_uri, element_name});
      }
    } catch (const std::exception& e) {
      std::cerr << "xb railroad: diagram build error: " << e.what() << "\n";
      return exit_codegen;
    }

    if (diagrams.empty()) {
      std::cerr << "xb railroad: no diagrams generated\n";
      return exit_codegen;
    }

    // Render SVG
    try {
      if (output_file.empty()) {
        xb::railroad::render_svg(diagrams, std::cout);
      } else {
        std::ofstream out(output_file);
        if (!out) {
          std::cerr << "xb railroad: cannot write file: " << output_file
                    << "\n";
          return exit_io;
        }
        xb::railroad::render_svg(diagrams, out);
      }
    } catch (const std::exception& e) {
      std::cerr << "xb railroad: SVG render error: " << e.what() << "\n";
      return exit_codegen;
    }

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

  // ---------------------------------------------------------------------------
  // generate-xsd subcommand
  // ---------------------------------------------------------------------------

  int
  run_generate_xsd(const nlohmann::json& config) {
    std::string encoding_file = config.value("encoding", "");
    std::string output_file = config.value("output", "");

    if (encoding_file.empty()) {
      std::cerr << "xb generate-xsd: --encoding is required\n";
      return exit_usage;
    }

    try {
      std::string bes_xml = read_file(encoding_file);
      xb::expat_reader bes_reader(bes_xml);
      bes_reader.read();
      auto encoding = xb::bes::read_encoding_type(bes_reader);

      auto resolved = xb::wire::resolve_imports(
          std::move(encoding),
          [&](const std::string& href) -> xb::bes::encoding_type {
            auto import_path =
                (fs::path(encoding_file).parent_path() / href).string();
            std::string import_xml = read_file(import_path);
            xb::expat_reader import_reader(import_xml);
            import_reader.read();
            return xb::bes::read_encoding_type(import_reader);
          });

      auto xsd = xb::wire::generate_xsd(resolved);

      if (output_file.empty()) {
        std::cout << xsd;
      } else {
        std::ofstream out(output_file);
        if (!out) {
          std::cerr << "xb: cannot write file: " << output_file << "\n";
          return exit_io;
        }
        out << xsd;
      }

      return exit_success;
    } catch (const std::exception& e) {
      std::cerr << "xb generate-xsd: " << e.what() << "\n";
      return exit_codegen;
    }
  }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point — dispatches to subcommand handlers
// ---------------------------------------------------------------------------

int
xb_cli::run(const nlohmann::json& config) {
  auto command = config.at("command").get<std::string>();
  const auto& cmd_config = config.at(command);

  if (command == "generate") return run_generate(cmd_config);
  if (command == "generate-xsd") return run_generate_xsd(cmd_config);
  if (command == "sample-doc") return run_sample_doc(cmd_config);
  if (command == "fetch") return run_fetch(cmd_config);
  if (command == "convert") return run_convert(cmd_config);
  if (command == "railroad") return run_railroad(cmd_config);

  std::cerr << "xb: unknown command: " << command << "\n";
  return exit_usage;
}
