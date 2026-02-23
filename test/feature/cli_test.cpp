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

static int
run_cli_stderr(const std::string& args, std::string& stderr_output) {
  auto tmp = fs::temp_directory_path() / "xb_cli_stderr.txt";
  std::string cmd = xb_cli + " " + args + " 2>" + tmp.string();
  int rc = exit_code(std::system(cmd.c_str()));
  std::ifstream in(tmp);
  stderr_output.assign(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
  fs::remove(tmp);
  return rc;
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

TEST_CASE("--help exits 0 and produces output", "[cli]") {
  std::string err;
  int rc = run_cli_stderr("--help", err);
  CHECK(rc == 0);
  CHECK(err.find("Usage") != std::string::npos);
}

TEST_CASE("-h exits 0 and produces output", "[cli]") {
  CHECK(run_cli("-h") == 0);
}

TEST_CASE("--version exits 0 and contains version", "[cli]") {
  std::string err;
  int rc = run_cli_stderr("--version", err);
  CHECK(rc == 0);
  CHECK(err.find("xb") != std::string::npos);
}

TEST_CASE("no arguments exits 1 (usage error)", "[cli]") {
  CHECK(run_cli("") == 1);
}

TEST_CASE("nonexistent schema file exits 2 (file error)", "[cli]") {
  CHECK(run_cli("nonexistent.xsd") == 2);
}

TEST_CASE("nonexistent type map file exits 2", "[cli]") {
  std::string out_dir = make_tmp_dir("tmapnotfound");
  int rc = run_cli("-t nonexistent.xml -o " + out_dir + " " + schema_dir +
                   "/xb-typemap.xsd");
  cleanup_dir(out_dir);
  CHECK(rc == 2);
}

TEST_CASE("generate from xb-typemap.xsd produces output file", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_typemap");
  cleanup_dir(out_dir); // start fresh

  int rc = run_cli("-o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
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

TEST_CASE("generated output contains expected content", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_content");
  cleanup_dir(out_dir);

  int rc = run_cli("-o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
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

TEST_CASE("namespace mapping overrides generated namespace", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_nsmap");
  cleanup_dir(out_dir);

  int rc = run_cli("-n \"http://xb.dev/typemap=custom_ns\" -o " + out_dir +
                   " " + schema_dir + "/xb-typemap.xsd");
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

TEST_CASE("default mode produces hpp and cpp files", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_default_mode");
  cleanup_dir(out_dir);

  int rc = run_cli("-o " + out_dir + " " + schema_dir + "/xb-typemap.xsd");
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

TEST_CASE("--header-only produces only hpp files", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_header_only");
  cleanup_dir(out_dir);

  int rc = run_cli("--header-only -o " + out_dir + " " + schema_dir +
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

TEST_CASE("--file-per-type produces multiple hpp files", "[cli]") {
  std::string out_dir = make_tmp_dir("gen_file_per_type");
  cleanup_dir(out_dir);

  int rc = run_cli("--file-per-type -o " + out_dir + " " + schema_dir +
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

TEST_CASE("--header-only --file-per-type is an error", "[cli]") {
  CHECK(run_cli("--header-only --file-per-type " + schema_dir +
                "/xb-typemap.xsd") == 1);
}

TEST_CASE("--list-outputs prints filenames without generating", "[cli]") {
  std::string stdout_out;
  int rc = run_cli_stdout("--list-outputs " + schema_dir + "/xb-typemap.xsd",
                          stdout_out);
  CHECK(rc == 0);
  CHECK(!stdout_out.empty());
  // Should contain at least one filename
  CHECK(stdout_out.find(".hpp") != std::string::npos);
  CHECK(stdout_out.find(".cpp") != std::string::npos);
}

TEST_CASE("output to non-existent directory creates it", "[cli]") {
  auto base = fs::temp_directory_path() / "xb_cli_mkdir_test";
  auto nested = base / "sub" / "dir";
  fs::remove_all(base);

  int rc =
      run_cli("-o " + nested.string() + " " + schema_dir + "/xb-typemap.xsd");
  CHECK(rc == 0);
  CHECK(fs::exists(nested));

  fs::remove_all(base);
}
