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

  std::string
  format_cpp_code(const std::string& code, const std::string& filename,
                  const std::string& style_file) {
    if (code.empty()) return {};

    if (!clang_format_available()) return code;

    // Build command
    std::string cmd = "clang-format";
    cmd += " --assume-filename=" + filename;

    if (!style_file.empty()) cmd += " --style=file:" + style_file;

    // Write input to a temporary file
    char tmp_in[] = "/tmp/xb-fmt-XXXXXX";
    int fd = mkstemp(tmp_in);
    if (fd < 0) return code;

    FILE* tmp_file = fdopen(fd, "w");
    if (!tmp_file) {
      ::close(fd);
      std::remove(tmp_in);
      return code;
    }
    std::fwrite(code.data(), 1, code.size(), tmp_file);
    std::fclose(tmp_file);

    // Run clang-format on the temp file, capture stdout
    std::string full_cmd = cmd + " < " + tmp_in;
    FILE* proc = popen(full_cmd.c_str(), "r");
    if (!proc) {
      std::remove(tmp_in);
      return code;
    }

    std::string result;
    std::array<char, 4096> buf;
    while (auto n = std::fread(buf.data(), 1, buf.size(), proc))
      result.append(buf.data(), n);

    int status = pclose(proc);
    std::remove(tmp_in);

    if (status != 0) return code;
    return result;
  }

} // namespace xb
