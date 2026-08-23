#include <catch2/catch_test_macros.hpp>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static const std::string source_dir = STRINGIFY(XB_SOURCE_DIR);

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
  auto tmp = fs::temp_directory_path() / "xb_re2_fetch_output.txt";
  std::string full = cmd + " >" + tmp.string() + " 2>&1";
  int rc = exit_code(std::system(full.c_str()));
  std::ifstream in(tmp);
  output.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
  fs::remove(tmp);
  return rc;
}

// Mirror the host's pkg-config search path into @p dest, leaving out
// re2.pc.  Pointing PKG_CONFIG_LIBDIR at the result hides RE2 from
// pkg_check_modules while every other module (curl's dependencies, for
// one) stays discoverable.
static void
mirror_pkg_config_path_without_re2(const fs::path& dest) {
  fs::create_directories(dest);

  std::string pc_path;
  REQUIRE(run_cmd("pkg-config --variable pc_path pkg-config", pc_path) == 0);
  while (!pc_path.empty() && std::isspace(pc_path.back()))
    pc_path.pop_back();

  size_t pos = 0;
  while (pos <= pc_path.size()) {
    auto sep = pc_path.find(':', pos);
    auto dir = pc_path.substr(pos, sep - pos);
    pos = (sep == std::string::npos) ? pc_path.size() + 1 : sep + 1;
    if (dir.empty() || !fs::is_directory(dir)) continue;
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.path().extension() != ".pc") continue;
      if (entry.path().filename() == "re2.pc") continue;
      std::error_code ec;
      fs::create_symlink(entry.path(), dest / entry.path().filename(), ec);
    }
  }
}

// A host with neither an re2 CMake package nor an re2 pkg-config file must
// still configure: xb falls back to building RE2 (and Abseil) from pinned
// sources.  That is what a bare CI image looks like.
TEST_CASE("configure without a system RE2 fetches RE2 from source",
          "[cmake][re2][fetch]") {
  auto tmp = fs::temp_directory_path() / "xb_re2_fetch_test";
  fs::remove_all(tmp);
  auto bld = tmp / "build";
  auto pkg_config_dir = tmp / "pkgconfig";
  mirror_pkg_config_path_without_re2(pkg_config_dir);

  std::string output;
  int rc = run_cmd("PKG_CONFIG_LIBDIR=" + pkg_config_dir.string() +
                       " cmake -S " + source_dir + " -B " + bld.string() +
                       " -Dxb_BUILD_TESTING=OFF"
                       " -DCMAKE_DISABLE_FIND_PACKAGE_re2=ON",
                   output);
  INFO("configure output:\n" << output);
  REQUIRE(rc == 0);

  CHECK(fs::exists(bld / "_deps" / "re2-src" / "CMakeLists.txt"));
  CHECK(fs::exists(bld / "_deps" / "absl-src" / "CMakeLists.txt"));

  fs::remove_all(tmp);
}
