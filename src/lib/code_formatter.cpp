#include <xb/code_formatter.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace xb {

  bool
  clang_format_available() {
    int result = std::system("clang-format --version > /dev/null 2>&1");
    return result == 0;
  }

  namespace {

    // Run clang-format with the given style argument and return the
    // formatted output.  Returns an empty string on failure.
    std::string
    run_clang_format(const std::string& code, const std::string& filename,
                     const std::string& style_arg) {
      std::string cmd = "clang-format";
      cmd += " --assume-filename=" + filename;
      if (!style_arg.empty()) cmd += " " + style_arg;

      char tmp_in[] = "/tmp/xb-fmt-XXXXXX";
      int fd = mkstemp(tmp_in);
      if (fd < 0) return {};

      FILE* tmp_file = fdopen(fd, "w");
      if (!tmp_file) {
        ::close(fd);
        std::remove(tmp_in);
        return {};
      }
      std::fwrite(code.data(), 1, code.size(), tmp_file);
      std::fclose(tmp_file);

      std::string full_cmd = cmd + " < " + tmp_in + " 2>/dev/null";
      FILE* proc = popen(full_cmd.c_str(), "r");
      if (!proc) {
        std::remove(tmp_in);
        return {};
      }

      std::string result;
      std::array<char, 4096> buf;
      while (auto n = std::fread(buf.data(), 1, buf.size(), proc))
        result.append(buf.data(), n);

      int status = pclose(proc);
      std::remove(tmp_in);

      if (status != 0) return {};
      return result;
    }

  } // namespace

  std::string
  format_cpp_code(const std::string& code, const std::string& filename,
                  const std::string& style_file) {
    if (code.empty()) return {};

    if (!clang_format_available()) return code;

    // Try the requested style first
    std::string style_arg;
    if (!style_file.empty()) style_arg = "--style=file:" + style_file;

    auto result = run_clang_format(code, filename, style_arg);
    if (!result.empty()) return result;

    // Fall back to built-in LLVM style
    result = run_clang_format(code, filename, "--style=LLVM");
    if (!result.empty()) return result;

    // clang-format is not cooperating; return unformatted
    return code;
  }

} // namespace xb
