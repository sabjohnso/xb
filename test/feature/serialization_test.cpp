// GCC 12 emits a false -Wmaybe-uninitialized for std::variant containing
// std::unique_ptr in particle::term_type at -O3. Suppress it here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/codegen.hpp>
#include <xb/cpp_writer.hpp>
#include <xb/expat_reader.hpp>
#include <xb/schema_parser.hpp>
#include <xb/schema_set.hpp>
#include <xb/type_map.hpp>

#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace xb;

namespace fs = std::filesystem;

// Write generated files + test main to a temp directory, compile, link, and run
static bool
build_and_run(const std::vector<cpp_file>& files, const std::string& test_name,
              const std::string& test_code) {
  auto tmp_dir = fs::temp_directory_path() / ("xb_rt_" + test_name);
  fs::create_directories(tmp_dir);

  cpp_writer writer;

  for (const auto& file : files) {
    auto path = tmp_dir / file.filename;
    std::ofstream out(path);
    out << writer.write(file);
  }

  // Write test main.cpp — only include headers, not source files
  auto main_path = tmp_dir / "main.cpp";
  {
    std::ofstream out(main_path);
    // GCC warning suppression for generated variant code
    out << "#if defined(__GNUC__) && !defined(__clang__)\n";
    out << "#pragma GCC diagnostic push\n";
    out << "#pragma GCC diagnostic ignored \"-Wmaybe-uninitialized\"\n";
    out << "#endif\n\n";
    for (const auto& file : files) {
      if (file.kind == file_kind::header)
        out << "#include \"" << file.filename << "\"\n";
    }
    out << "\n";
    out << "#include <xb/ostream_writer.hpp>\n";
    out << "#include <xb/expat_reader.hpp>\n";
    out << "#include <sstream>\n";
    out << "#include <cassert>\n";
    out << "#include <iostream>\n";
    out << "\n";
    out << test_code;
  }

  // Collect generated source files for compilation
  std::string source_files;
  for (const auto& file : files) {
    if (file.kind == file_kind::source) {
      source_files += ' ';
      source_files += (tmp_dir / file.filename).string();
    }
  }

  std::string include_dir = STRINGIFY(XB_INCLUDE_DIR);
  std::string lib_file = STRINGIFY(XB_LIB_FILE);
  auto exe_path = tmp_dir / "test_exe";

  // When the library was built with sanitizers (RelWithDebInfo), pass
  // the same flags to the subprocess so it can link the sanitizer runtime.
  std::string sanitizer_flags;
  if (XB_SANITIZERS)
    sanitizer_flags = "-fsanitize=undefined -fsanitize=address ";

  // Compile and link
  std::string cmd = "c++ -std=c++20 " + sanitizer_flags + "-I" +
                    tmp_dir.string() + " -I" + include_dir + " -o " +
                    exe_path.string() + " " + main_path.string() +
                    source_files + " " + lib_file + " -lexpat 2>&1";
  int rc = std::system(cmd.c_str());

  if (rc != 0) {
    std::cerr << "Build failed for " << test_name << "\n";
    std::cerr << "Command: " << cmd << "\n";
    fs::remove_all(tmp_dir);
    return false;
  }

  // Run the generated test executable
  rc = std::system(exe_path.string().c_str());

  if (rc != 0) {
    std::cerr << "Run failed for " << test_name << " (exit code " << rc
              << ")\n";
  }

  fs::remove_all(tmp_dir);
  return rc == 0;
}

TEST_CASE("round-trip: sequence with attributes and enum",
          "[serialization][round-trip]") {
  schema s;
  s.set_target_namespace("http://example.com/order");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  // Enum type for side
  facet_set side_facets;
  side_facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/order", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, side_facets));

  // Complex type with sequence + attributes
  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "symbol"}, qname{xs, "string"}));
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "quantity"}, qname{xs, "int"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/order", "price"},
                   qname{xs, "double"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs, "string"}, true, {}, {}});
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/order", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/order", "OrderType"}, false, false,
                   std::move(ct), std::move(attrs)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);

  std::string test_code = R"(
int main() {
  using namespace example::com::order;

  // Construct value
  order_type val;
  val.id = "ABC123";
  val.side = side_type::buy;
  val.symbol = "AAPL";
  val.quantity = 100;
  val.price = 150.5;

  // Serialize
  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://example.com/order", "Order"});
    writer.namespace_declaration("", "http://example.com/order");
    write_order_type(val, writer);
    writer.end_element();
  }

  // Deserialize
  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_order_type(reader);

  // Compare
  assert(result == val);

  // Round-trip without optional price
  order_type val2;
  val2.id = "DEF456";
  val2.side = side_type::sell;
  val2.symbol = "MSFT";
  val2.quantity = 50;

  std::ostringstream os2;
  {
    xb::ostream_writer writer(os2);
    writer.start_element(xb::qname{"http://example.com/order", "Order"});
    writer.namespace_declaration("", "http://example.com/order");
    write_order_type(val2, writer);
    writer.end_element();
  }

  xb::expat_reader reader2(os2.str());
  reader2.read();
  auto result2 = read_order_type(reader2);

  assert(result2 == val2);
  assert(!result2.price.has_value());

  return 0;
}
)";

  CHECK(build_and_run(files, "sequence_attrs", test_code));
}

TEST_CASE("round-trip: choice type", "[serialization][round-trip]") {
  schema s;
  s.set_target_namespace("http://example.com/msg");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  std::vector<particle> particles;
  particles.emplace_back(element_decl(qname{"http://example.com/msg", "text"},
                                      qname{xs, "string"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/msg", "code"}, qname{xs, "int"}));
  model_group choice(compositor_kind::choice, std::move(particles));

  content_type ct(content_kind::element_only,
                  complex_content(qname{}, derivation_method::restriction,
                                  std::move(choice)));

  s.add_complex_type(
      complex_type(qname{"http://example.com/msg", "MessageType"}, false, false,
                   std::move(ct)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 1);

  std::string test_code = R"(
int main() {
  using namespace example::com::msg;

  // Test string alternative
  {
    message_type val;
    val.choice = std::string("hello world");

    std::ostringstream os;
    {
      xb::ostream_writer writer(os);
      writer.start_element(xb::qname{"http://example.com/msg", "Message"});
      writer.namespace_declaration("", "http://example.com/msg");
      write_message_type(val, writer);
      writer.end_element();
    }

    xb::expat_reader reader(os.str());
    reader.read();
    auto result = read_message_type(reader);

    assert(result == val);
  }

  // Test int alternative
  {
    message_type val;
    val.choice = int32_t(42);

    std::ostringstream os;
    {
      xb::ostream_writer writer(os);
      writer.start_element(xb::qname{"http://example.com/msg", "Message"});
      writer.namespace_declaration("", "http://example.com/msg");
      write_message_type(val, writer);
      writer.end_element();
    }

    xb::expat_reader reader(os.str());
    reader.read();
    auto result = read_message_type(reader);

    assert(result == val);
  }

  return 0;
}
)";

  CHECK(build_and_run(files, "choice", test_code));
}

TEST_CASE("round-trip: cross-namespace schemas",
          "[serialization][round-trip]") {
  std::string xs = "http://www.w3.org/2001/XMLSchema";

  schema s1;
  s1.set_target_namespace("http://example.com/base");
  s1.add_simple_type(simple_type(qname{"http://example.com/base", "IDType"},
                                 simple_type_variety::atomic,
                                 qname{xs, "string"}));

  schema s2;
  s2.set_target_namespace("http://example.com/app");
  s2.add_import(schema_import{"http://example.com/base", ""});

  std::vector<particle> particles;
  particles.emplace_back(
      element_decl(qname{"http://example.com/app", "id"},
                   qname{"http://example.com/base", "IDType"}));
  particles.emplace_back(element_decl(qname{"http://example.com/app", "name"},
                                      qname{xs, "string"}));
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  s2.add_complex_type(
      complex_type(qname{"http://example.com/app", "EntityType"}, false, false,
                   std::move(ct)));

  schema_set ss;
  ss.add(std::move(s1));
  ss.add(std::move(s2));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(files.size() == 2);

  std::string test_code = R"(
int main() {
  using namespace example::com::app;

  entity_type val;
  val.id = "E001";
  val.name = "Test Entity";

  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://example.com/app", "Entity"});
    writer.namespace_declaration("", "http://example.com/app");
    write_entity_type(val, writer);
    writer.end_element();
  }

  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_entity_type(reader);

  assert(result == val);

  return 0;
}
)";

  CHECK(build_and_run(files, "cross_ref", test_code));
}

TEST_CASE("round-trip: xb-typemap.xsd", "[serialization][round-trip]") {
  // Parse the actual xb-typemap.xsd
  std::string schema_path =
      std::string(STRINGIFY(XB_SCHEMA_DIR)) + "/xb-typemap.xsd";
  std::ifstream file(schema_path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  auto s = parser.parse(reader);

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen gen(ss, types);
  auto files = gen.generate();
  REQUIRE(!files.empty());

  std::string test_code = R"(
int main() {
  using namespace xb::dev::typemap;

  // Construct a typemap with two mappings
  typemap_type val;

  mapping_type m1;
  m1.xsd_type = xsd_builtin_type::string;
  m1.cpp_type = "std::string";
  m1.cpp_header = "<string>";
  val.mapping.push_back(m1);

  mapping_type m2;
  m2.xsd_type = xsd_builtin_type::int_;
  m2.cpp_type = "int32_t";
  m2.cpp_header = "<cstdint>";
  val.mapping.push_back(m2);

  // Serialize
  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://xb.dev/typemap", "typemap"});
    writer.namespace_declaration("", "http://xb.dev/typemap");
    write_typemap_type(val, writer);
    writer.end_element();
  }

  // Deserialize
  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_typemap_type(reader);

  // Compare
  assert(result == val);
  assert(result.mapping.size() == 2);
  assert(result.mapping[0].xsd_type == xsd_builtin_type::string);
  assert(result.mapping[0].cpp_type == "std::string");
  assert(result.mapping[1].xsd_type == xsd_builtin_type::int_);

  return 0;
}
)";

  CHECK(build_and_run(files, "typemap_xsd", test_code));
}

// ===== Split mode round-trip tests =====

TEST_CASE("split mode round-trip: sequence with attributes",
          "[serialization][round-trip][split]") {
  schema s;
  s.set_target_namespace("http://example.com/order");

  std::string xs = "http://www.w3.org/2001/XMLSchema";

  facet_set side_facets;
  side_facets.enumeration = {"Buy", "Sell"};
  s.add_simple_type(simple_type(qname{"http://example.com/order", "SideType"},
                                simple_type_variety::atomic,
                                qname{xs, "string"}, side_facets));

  std::vector<particle> particles;
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "symbol"}, qname{xs, "string"}));
  particles.emplace_back(element_decl(
      qname{"http://example.com/order", "quantity"}, qname{xs, "int"}));
  particles.emplace_back(
      element_decl(qname{"http://example.com/order", "price"},
                   qname{xs, "double"}),
      occurrence{0, 1});
  model_group seq(compositor_kind::sequence, std::move(particles));

  content_type ct(
      content_kind::element_only,
      complex_content(qname{}, derivation_method::restriction, std::move(seq)));

  std::vector<attribute_use> attrs;
  attrs.push_back({qname{"", "id"}, qname{xs, "string"}, true, {}, {}});
  attrs.push_back({qname{"", "side"},
                   qname{"http://example.com/order", "SideType"},
                   true,
                   {},
                   {}});

  s.add_complex_type(
      complex_type(qname{"http://example.com/order", "OrderType"}, false, false,
                   std::move(ct), std::move(attrs)));

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();
  REQUIRE(files.size() == 2);

  std::string test_code = R"(
int main() {
  using namespace example::com::order;

  order_type val;
  val.id = "ABC123";
  val.side = side_type::buy;
  val.symbol = "AAPL";
  val.quantity = 100;
  val.price = 150.5;

  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://example.com/order", "Order"});
    writer.namespace_declaration("", "http://example.com/order");
    write_order_type(val, writer);
    writer.end_element();
  }

  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_order_type(reader);

  assert(result == val);
  return 0;
}
)";

  CHECK(build_and_run(files, "split_sequence_attrs", test_code));
}

TEST_CASE("split mode round-trip: xb-typemap.xsd",
          "[serialization][round-trip][split]") {
  std::string schema_path =
      std::string(STRINGIFY(XB_SCHEMA_DIR)) + "/xb-typemap.xsd";
  std::ifstream file(schema_path);
  REQUIRE(file.good());
  std::string xml((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  expat_reader reader(xml);
  schema_parser parser;
  auto s = parser.parse(reader);

  schema_set ss;
  ss.add(std::move(s));
  ss.resolve();

  auto types = type_map::defaults();
  codegen_options opts;
  opts.mode = output_mode::split;
  codegen gen(ss, types, opts);
  auto files = gen.generate();
  REQUIRE(files.size() == 2);

  std::string test_code = R"(
int main() {
  using namespace xb::dev::typemap;

  typemap_type val;

  mapping_type m1;
  m1.xsd_type = xsd_builtin_type::string;
  m1.cpp_type = "std::string";
  m1.cpp_header = "<string>";
  val.mapping.push_back(m1);

  mapping_type m2;
  m2.xsd_type = xsd_builtin_type::int_;
  m2.cpp_type = "int32_t";
  m2.cpp_header = "<cstdint>";
  val.mapping.push_back(m2);

  std::ostringstream os;
  {
    xb::ostream_writer writer(os);
    writer.start_element(xb::qname{"http://xb.dev/typemap", "typemap"});
    writer.namespace_declaration("", "http://xb.dev/typemap");
    write_typemap_type(val, writer);
    writer.end_element();
  }

  xb::expat_reader reader(os.str());
  reader.read();
  auto result = read_typemap_type(reader);

  assert(result == val);
  assert(result.mapping.size() == 2);
  assert(result.mapping[0].xsd_type == xsd_builtin_type::string);
  assert(result.mapping[0].cpp_type == "std::string");

  return 0;
}
)";

  CHECK(build_and_run(files, "split_typemap_xsd", test_code));
}
