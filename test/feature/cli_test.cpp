#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static const std::string xb_cli = STRINGIFY(XB_CLI);
static const std::string schema_dir = STRINGIFY(XB_SCHEMA_DIR);

// Portable exit code extraction: WEXITSTATUS on POSIX, raw value on Windows
static int
exit_code(int status) {
#ifdef _WIN32
  return status;
#else
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
#endif
}

static int
run_cli(const std::string& args) {
  std::string cmd = xb_cli + " " + args + " 2>/dev/null";
  return exit_code(std::system(cmd.c_str()));
}

static std::string
make_tmp_dir(const std::string& name) {
  auto dir = fs::temp_directory_path() / ("xb_cli_" + name);
  fs::create_directories(dir);
  return dir.string();
}

static void
cleanup_dir(const std::string& path) {
  fs::remove_all(path);
}

static int
run_cli_stdout(const std::string& args, std::string& stdout_output) {
  auto tmp = fs::temp_directory_path() / "xb_cli_stdout.txt";
  std::string cmd = xb_cli + " " + args + " >" + tmp.string() + " 2>/dev/null";
  int rc = exit_code(std::system(cmd.c_str()));
  std::ifstream in(tmp);
  stdout_output.assign(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
  fs::remove(tmp);
  return rc;
}

TEST_CASE("--help exits 0 and produces output", "[cli]") {
  std::string out;
  int rc = run_cli_stdout("--help", out);
  CHECK(rc == 0);
  CHECK(out.find("xb") != std::string::npos);
}

TEST_CASE("-h exits 0 and produces output", "[cli]") {
  CHECK(run_cli("-h") == 0);
}

TEST_CASE("--version exits 0 and contains version", "[cli]") {
  std::string out;
  int rc = run_cli_stdout("--version", out);
  CHECK(rc == 0);
  CHECK(out.find("xb") != std::string::npos);
}

TEST_CASE("no subcommand exits 1 (usage error)", "[cli]") {
  CHECK(run_cli("") == 1);
}

TEST_CASE("generate --help exits 0", "[cli][generate]") {
  std::string out;
  int rc = run_cli_stdout("generate --help", out);
  CHECK(rc == 0);
  CHECK(out.find("output-dir") != std::string::npos);
}

TEST_CASE("generate with no arguments exits 1 (usage error)",
          "[cli][generate]") {
  CHECK(run_cli("generate") == 1);
}

TEST_CASE("nonexistent schema file exits 2 (file error)", "[cli][generate]") {
  CHECK(run_cli("generate nonexistent.xsd") == 2);
}

TEST_CASE("nonexistent type map file exits 2", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("tmapnotfound");
  int rc = run_cli("generate -t nonexistent.xml -o " + out_dir + " " +
                   schema_dir + "/xb-typemap.xsd");
  cleanup_dir(out_dir);
  CHECK(rc == 2);
}

TEST_CASE("generate from xb-typemap.xsd produces output file",
          "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_typemap");
  cleanup_dir(out_dir); // start fresh

  int rc =
      run_cli("generate -o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
  CHECK(rc == 0);

  // At least one file should exist in the output directory
  bool found_file = false;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      if (entry.is_regular_file()) {
        found_file = true;
        // File should be non-empty
        CHECK(fs::file_size(entry.path()) > 0);
      }
    }
  }
  CHECK(found_file);

  cleanup_dir(out_dir);
}

TEST_CASE("generated output contains expected content", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_content");
  cleanup_dir(out_dir);

  int rc =
      run_cli("generate -o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
  REQUIRE(rc == 0);

  // Read the first generated file and check it has C++ content
  bool found_pragma = false;
  for (const auto& entry : fs::directory_iterator(out_dir)) {
    if (entry.is_regular_file()) {
      std::ifstream in(entry.path());
      std::string content{std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>()};
      if (content.find("#pragma once") != std::string::npos)
        found_pragma = true;
    }
  }
  CHECK(found_pragma);

  cleanup_dir(out_dir);
}

TEST_CASE("namespace mapping overrides generated namespace",
          "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_nsmap");
  cleanup_dir(out_dir);

  int rc = run_cli("generate -n \"http://xb.dev/typemap=custom_ns\" -o " +
                   out_dir + " " + schema_dir + "/xb-typemap.xsd");
  REQUIRE(rc == 0);

  // Read generated file and verify the custom namespace appears
  bool found_ns = false;
  for (const auto& entry : fs::directory_iterator(out_dir)) {
    if (entry.is_regular_file()) {
      std::ifstream in(entry.path());
      std::string content{std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>()};
      if (content.find("custom_ns") != std::string::npos) found_ns = true;
    }
  }
  CHECK(found_ns);

  cleanup_dir(out_dir);
}

// ===== Output mode flags =====

TEST_CASE("default mode produces hpp and cpp files", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_default_mode");
  cleanup_dir(out_dir);

  int rc =
      run_cli("generate -o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
  CHECK(rc == 0);

  bool found_hpp = false;
  bool found_cpp = false;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      auto ext = entry.path().extension().string();
      if (ext == ".hpp") found_hpp = true;
      if (ext == ".cpp") found_cpp = true;
    }
  }
  CHECK(found_hpp);
  CHECK(found_cpp);

  cleanup_dir(out_dir);
}

TEST_CASE("--header-only produces only hpp files", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_header_only");
  cleanup_dir(out_dir);

  int rc = run_cli("generate --header-only -o " + out_dir + " " + schema_dir +
                   "/xb-typemap.xsd");
  CHECK(rc == 0);

  bool found_hpp = false;
  bool found_cpp = false;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      auto ext = entry.path().extension().string();
      if (ext == ".hpp") found_hpp = true;
      if (ext == ".cpp") found_cpp = true;
    }
  }
  CHECK(found_hpp);
  CHECK_FALSE(found_cpp);

  cleanup_dir(out_dir);
}

TEST_CASE("--file-per-type produces multiple hpp files", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_file_per_type");
  cleanup_dir(out_dir);

  int rc = run_cli("generate --file-per-type -o " + out_dir + " " + schema_dir +
                   "/xb-typemap.xsd");
  CHECK(rc == 0);

  int hpp_count = 0;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      if (entry.path().extension() == ".hpp") ++hpp_count;
    }
  }
  // Should have more than 1 header (per-type + umbrella)
  CHECK(hpp_count > 1);

  cleanup_dir(out_dir);
}

TEST_CASE("--header-only --file-per-type uses last flag", "[cli][generate]") {
  std::string out_dir = make_tmp_dir("gen_last_wins");
  cleanup_dir(out_dir);

  // json-commander flag_group: last flag wins
  int rc = run_cli("generate --header-only --file-per-type -o " + out_dir +
                   " " + schema_dir + "/xb-typemap.xsd");
  CHECK(rc == 0);

  // --file-per-type was last, so expect multiple hpp files
  int hpp_count = 0;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      if (entry.path().extension() == ".hpp") ++hpp_count;
    }
  }
  CHECK(hpp_count > 1);

  cleanup_dir(out_dir);
}

TEST_CASE("--list-outputs prints filenames without generating",
          "[cli][generate]") {
  std::string stdout_out;
  int rc = run_cli_stdout(
      "generate --list-outputs " + schema_dir + "/xb-typemap.xsd", stdout_out);
  CHECK(rc == 0);
  CHECK(!stdout_out.empty());
  // Should contain at least one filename
  CHECK(stdout_out.find(".hpp") != std::string::npos);
  CHECK(stdout_out.find(".cpp") != std::string::npos);
}

TEST_CASE("output to non-existent directory creates it", "[cli][generate]") {
  auto base = fs::temp_directory_path() / "xb_cli_mkdir_test";
  auto nested = base / "sub" / "dir";
  fs::remove_all(base);

  int rc = run_cli("generate -o " + nested.string() + " " + schema_dir +
                   "/xb-typemap.xsd");
  CHECK(rc == 0);
  CHECK(fs::exists(nested));

  fs::remove_all(base);
}

// ===== fetch subcommand =====

static std::string
read_file_contents(const fs::path& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST_CASE("fetch --help exits 0", "[cli][fetch]") {
  std::string out;
  int rc = run_cli_stdout("fetch --help", out);
  CHECK(rc == 0);
  CHECK(out.find("output-dir") != std::string::npos);
}

TEST_CASE("fetch with no arguments exits 1", "[cli][fetch]") {
  CHECK(run_cli("fetch") == 1);
}

TEST_CASE("fetch with nonexistent file exits 2", "[cli][fetch]") {
  CHECK(run_cli("fetch nonexistent.xsd") == 2);
}

TEST_CASE("fetch writes schema files to output directory", "[cli][fetch]") {
  std::string out_dir = make_tmp_dir("fetch_output");
  cleanup_dir(out_dir);

  int rc = run_cli("fetch " + schema_dir + "/xb-typemap.xsd --output-dir " +
                   out_dir);
  CHECK(rc == 0);

  bool found_xsd = false;
  if (fs::exists(out_dir)) {
    for (const auto& entry : fs::directory_iterator(out_dir)) {
      if (entry.path().extension() == ".xsd") {
        found_xsd = true;
        CHECK(fs::file_size(entry.path()) > 0);
      }
    }
  }
  CHECK(found_xsd);

  cleanup_dir(out_dir);
}

TEST_CASE("fetch writes manifest with correct structure", "[cli][fetch]") {
  std::string out_dir = make_tmp_dir("fetch_manifest");
  cleanup_dir(out_dir);

  auto manifest = out_dir + "/manifest.json";
  int rc = run_cli("fetch " + schema_dir + "/xb-typemap.xsd --output-dir " +
                   out_dir + " --manifest " + manifest);
  REQUIRE(rc == 0);
  REQUIRE(fs::exists(manifest));

  auto content = read_file_contents(manifest);
  CHECK(content.find("\"root\"") != std::string::npos);
  CHECK(content.find("\"schemas\"") != std::string::npos);
  CHECK(content.find("\"path\"") != std::string::npos);
  CHECK(content.find("xb-typemap.xsd") != std::string::npos);

  cleanup_dir(out_dir);
}

TEST_CASE("fetch is idempotent", "[cli][fetch]") {
  std::string out_dir = make_tmp_dir("fetch_idempotent");
  cleanup_dir(out_dir);

  auto manifest = out_dir + "/manifest.json";
  std::string args = "fetch " + schema_dir + "/xb-typemap.xsd --output-dir " +
                     out_dir + " --manifest " + manifest;

  REQUIRE(run_cli(args) == 0);
  auto first_content = read_file_contents(manifest);

  // Second fetch should also succeed
  CHECK(run_cli(args) == 0);
  auto second_content = read_file_contents(manifest);

  // Manifest content should have the same structure
  CHECK(second_content.find("\"root\"") != std::string::npos);
  CHECK(second_content.find("xb-typemap.xsd") != std::string::npos);

  cleanup_dir(out_dir);
}
