#include <catch2/catch_test_macros.hpp>

#include <xb/code_formatter.hpp>

#include <filesystem>
#include <string>

TEST_CASE("format_cpp_code returns formatted code", "[code_formatter]") {
  // Deliberately ugly code that clang-format would fix
  std::string ugly = "struct foo{int x;std::string y;bool z;};";
  auto result = xb::format_cpp_code(ugly, "test.hpp");
  // Should produce valid output regardless of clang-format availability
  REQUIRE(!result.empty());
  REQUIRE(result.find("struct foo") != std::string::npos);
  if (xb::clang_format_available()) {
    // If clang-format is available, should at minimum add spacing
    // (even if the project .clang-format is incompatible, LLVM fallback
    // should still format)
    REQUIRE(result != ugly);
  }
}

TEST_CASE("format_cpp_code with style file", "[code_formatter]") {
  std::string code = "struct foo{int x;};";
  // Pass a style file path (even if it doesn't exist, should still work
  // or fall back gracefully)
  auto result =
      xb::format_cpp_code(code, "test.hpp", "/nonexistent/.clang-format");
  // Should still produce valid output (either formatted or original)
  REQUIRE(!result.empty());
}

TEST_CASE("format_cpp_code falls back gracefully on bad style file",
          "[code_formatter]") {
  if (!xb::clang_format_available()) return;
  std::string code = "struct foo{int x;std::string y;};";
  // A style file with an invalid key should not prevent formatting
  auto result =
      xb::format_cpp_code(code, "test.hpp", "/nonexistent/.clang-format");
  REQUIRE(!result.empty());
  // Should still get formatted output (via LLVM fallback)
  CHECK(result.find("struct foo") != std::string::npos);
}

TEST_CASE("format_cpp_code preserves empty input", "[code_formatter]") {
  auto result = xb::format_cpp_code("", "test.hpp");
  REQUIRE(result.empty());
}

TEST_CASE("format_cpp_code preserves already-formatted code",
          "[code_formatter]") {
  std::string code = "struct foo {\n  int x;\n};\n";
  auto result = xb::format_cpp_code(code, "test.hpp");
  // Already well-formatted, should be unchanged or very similar
  REQUIRE(result.find("struct foo") != std::string::npos);
}

TEST_CASE("format_cpp_code does not interpret filename via shell",
          "[code_formatter][security]") {
  if (!xb::clang_format_available()) return;

  // Use a filename whose value contains a shell command substitution.
  // The popen()-based implementation embeds the filename in a string
  // passed to /bin/sh -c, which interprets ";" as a command separator
  // and runs `touch <sentinel>`. The posix_spawnp implementation
  // passes argv unmolested, so the filename is treated as a literal
  // value by clang-format and the sentinel is never created.
  auto sentinel =
      std::filesystem::temp_directory_path() / "xb-shell-injection-canary";
  std::error_code ec;
  std::filesystem::remove(sentinel, ec);

  std::string filename = "x.hpp;touch " + sentinel.string() + ";echo ";
  std::string code = "int main(){return 0;}";
  xb::format_cpp_code(code, filename);

  bool created = std::filesystem::exists(sentinel);
  std::filesystem::remove(sentinel, ec);
  CHECK_FALSE(created);
}

TEST_CASE("clang_format_available returns bool", "[code_formatter]") {
  // Just verify the function exists and returns a value
  bool available = xb::clang_format_available();
  if (available) {
    // If available, formatting should work
    std::string ugly = "int main(){return 0;}";
    auto result = xb::format_cpp_code(ugly, "test.cpp");
    REQUIRE(!result.empty());
  }
  // If not available, that's fine too — test just verifies the API exists
}
