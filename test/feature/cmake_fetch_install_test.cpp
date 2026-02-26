#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static const std::string build_dir = STRINGIFY(XB_BUILD_DIR);
static const std::string schema_dir = STRINGIFY(XB_SCHEMA_DIR);

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
run_cmd(const std::string& cmd, std::string& output) {
  auto tmp = fs::temp_directory_path() / "xb_fetch_install_output.txt";
  std::string full = cmd + " >" + tmp.string() + " 2>&1";
  int rc = exit_code(std::system(full.c_str()));
  std::ifstream in(tmp);
  output.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
  fs::remove(tmp);
  return rc;
}

static void
write_file(const fs::path& path, const std::string& content) {
  std::ofstream out(path);
  out << content;
}

TEST_CASE("install and find_package with xb_fetch_schemas",
          "[cmake][fetch][install]") {
  auto tmp = fs::temp_directory_path() / "xb_fetch_install_test";
  fs::remove_all(tmp);

  auto prefix = tmp / "install";
  auto project = tmp / "project";
  auto bld = tmp / "build";

  fs::create_directories(project);

  std::string output;
  int rc;

  // Step 1: Install xb to a local prefix
  rc = run_cmd("cmake --install " + build_dir + " --config Release --prefix " +
                   prefix.string(),
               output);
  INFO("install output:\n" << output);
  REQUIRE(rc == 0);

  // Step 2: Write the mini-project
  std::string schema_path = schema_dir + "/xb-typemap.xsd";

  write_file(project / "CMakeLists.txt",
             "cmake_minimum_required(VERSION 3.12)\n"
             "project(xb_fetch_test LANGUAGES CXX)\n"
             "find_package(xb REQUIRED)\n"
             "xb_fetch_schemas(\n"
             "  URL " +
                 schema_path +
                 "\n"
                 "  OUTPUT_DIR ${CMAKE_BINARY_DIR}/fetched\n"
                 "  SCHEMAS_VAR FETCHED_SCHEMAS)\n"
                 "xb_generate_cpp(\n"
                 "  TARGET gen SCHEMAS ${FETCHED_SCHEMAS} MODE HEADER_ONLY)\n"
                 "add_executable(test_exe main.cpp)\n"
                 "target_link_libraries(test_exe PRIVATE gen xb::xb)\n"
                 "target_compile_features(test_exe PRIVATE cxx_std_20)\n");

  write_file(project / "main.cpp", "#include \"typemap.hpp\"\n"
                                   "int main() {\n"
                                   "  xb::dev::typemap::typemap_type val;\n"
                                   "  return val.mapping.empty() ? 0 : 1;\n"
                                   "}\n");

  // Step 3: Configure the mini-project
  rc = run_cmd("cmake -S " + project.string() + " -B " + bld.string() +
                   " -DCMAKE_PREFIX_PATH=" + prefix.string(),
               output);
  INFO("configure output:\n" << output);
  REQUIRE(rc == 0);

  // Step 4: Build the mini-project
  rc = run_cmd("cmake --build " + bld.string(), output);
  INFO("build output:\n" << output);
  REQUIRE(rc == 0);

  // Step 5: Find and run the built executable
  fs::path exe_path;
  for (auto& entry : fs::recursive_directory_iterator(bld)) {
    if (entry.is_regular_file() && entry.path().filename() == "test_exe") {
      exe_path = entry.path();
      break;
    }
  }
  REQUIRE(!exe_path.empty());
  CHECK(run_cmd(exe_path.string(), output) == 0);

  // Cleanup
  fs::remove_all(tmp);
}
