/// @file
/// Tests for xb::write_text_file_no_follow — verifies that pre-existing
/// symlinks at the destination cause the write to be refused, while
/// regular files and missing files behave like a normal create-truncate.

#include <xb/safe_output.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

  /// Generate a unique path in the system temp dir for one test case.
  fs::path
  unique_temp_path(std::string_view stem) {
    std::random_device rd;
    std::ostringstream name;
    name << "xb-safe-output-" << stem << "-" << rd() << "-" << ::getpid();
    return fs::temp_directory_path() / name.str();
  }

} // namespace

TEST_CASE("write_text_file_no_follow creates a fresh file", "[safe_output]") {
  auto path = unique_temp_path("fresh");
  std::error_code ec;
  fs::remove(path, ec);

  auto outcome = xb::write_text_file_no_follow(path, "hello world");
  CHECK(outcome == xb::write_outcome::ok);

  std::ifstream in(path);
  std::stringstream buf;
  buf << in.rdbuf();
  CHECK(buf.str() == "hello world");

  fs::remove(path, ec);
}

TEST_CASE("write_text_file_no_follow truncates an existing regular file",
          "[safe_output]") {
  auto path = unique_temp_path("truncate");
  std::error_code ec;
  fs::remove(path, ec);

  // Pre-existing content larger than what we'll write.
  {
    std::ofstream out(path);
    out << "AAAAAAAAAAAAAAAAAAAAAAAA";
  }

  auto outcome = xb::write_text_file_no_follow(path, "ok");
  CHECK(outcome == xb::write_outcome::ok);

  std::ifstream in(path);
  std::stringstream buf;
  buf << in.rdbuf();
  CHECK(buf.str() == "ok");

  fs::remove(path, ec);
}

TEST_CASE("write_text_file_no_follow refuses to overwrite a symlink",
          "[safe_output][security]") {
  auto link = unique_temp_path("link");
  auto target = unique_temp_path("target");
  std::error_code ec;
  fs::remove(link, ec);
  fs::remove(target, ec);

  // Create a target file with sentinel content and a symlink pointing
  // at it.
  {
    std::ofstream out(target);
    out << "sentinel";
  }
  fs::create_symlink(target, link);

  int errno_val = 0;
  auto outcome =
      xb::write_text_file_no_follow(link, "evil overwrite", &errno_val);
  CHECK(outcome == xb::write_outcome::refused_symlink);

  // The target must NOT have been overwritten.
  std::ifstream in(target);
  std::stringstream buf;
  buf << in.rdbuf();
  CHECK(buf.str() == "sentinel");

  fs::remove(link, ec);
  fs::remove(target, ec);
}
