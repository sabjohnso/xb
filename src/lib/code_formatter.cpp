#include <xb/code_formatter.hpp>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace xb {

  bool
  clang_format_available() {
    // Argument string is a fixed literal — no user input flows into the
    // shell, so std::system is safe here. The user-input-driven path is
    // run_clang_format below, which uses posix_spawnp with an argv
    // array.
    int result = std::system("clang-format --version > /dev/null 2>&1");
    return result == 0;
  }

  namespace {

    /// RAII wrapper for a POSIX pipe pair.
    struct pipe_pair {
      int fds[2] = {-1, -1};

      pipe_pair() {
        if (::pipe(fds) != 0) { fds[0] = fds[1] = -1; }
      }

      ~pipe_pair() {
        if (fds[0] >= 0) ::close(fds[0]);
        if (fds[1] >= 0) ::close(fds[1]);
      }

      pipe_pair(const pipe_pair&) = delete;
      pipe_pair&
      operator=(const pipe_pair&) = delete;

      bool
      ok() const {
        return fds[0] >= 0 && fds[1] >= 0;
      }

      int
      release_read() {
        int r = fds[0];
        fds[0] = -1;
        return r;
      }

      int
      release_write() {
        int r = fds[1];
        fds[1] = -1;
        return r;
      }
    };

    /// Run @c clang-format with the given style argument and return the
    /// formatted output. Returns an empty string on failure.
    ///
    /// Uses @c posix_spawnp with an explicit argv array so that
    /// caller-supplied @p filename — which on this codepath flows from
    /// schema-derived class names — cannot be interpreted by a shell.
    /// stdin / stdout are connected via pipes; @c stderr is redirected
    /// to @c /dev/null inside the child.
    std::string
    run_clang_format(const std::string& code, const std::string& filename,
                     const std::string& style_arg) {
      pipe_pair in_pipe;
      pipe_pair out_pipe;
      if (!in_pipe.ok() || !out_pipe.ok()) return {};

      // Build argv. Each element is passed verbatim to execvp; the
      // shell is never involved.
      std::string assume_arg = "--assume-filename=" + filename;
      std::vector<char*> argv;
      argv.push_back(const_cast<char*>("clang-format"));
      argv.push_back(const_cast<char*>(assume_arg.c_str()));
      if (!style_arg.empty()) {
        argv.push_back(const_cast<char*>(style_arg.c_str()));
      }
      argv.push_back(nullptr);

      posix_spawn_file_actions_t actions;
      if (posix_spawn_file_actions_init(&actions) != 0) return {};

      bool actions_ok = true;
      // Child reads code from in_pipe[0] -> fd 0
      actions_ok = actions_ok &&
                   posix_spawn_file_actions_adddup2(&actions, in_pipe.fds[0],
                                                    STDIN_FILENO) == 0;
      actions_ok = actions_ok && posix_spawn_file_actions_addclose(
                                     &actions, in_pipe.fds[1]) == 0;
      // Child writes formatted output to out_pipe[1] -> fd 1
      actions_ok = actions_ok &&
                   posix_spawn_file_actions_adddup2(&actions, out_pipe.fds[1],
                                                    STDOUT_FILENO) == 0;
      actions_ok = actions_ok && posix_spawn_file_actions_addclose(
                                     &actions, out_pipe.fds[0]) == 0;
      // Silence stderr — clang-format prints diagnostics there that we
      // discard (mirrors the original popen "2>/dev/null" behaviour).
      actions_ok = actions_ok &&
                   posix_spawn_file_actions_addopen(
                       &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) == 0;
      if (!actions_ok) {
        posix_spawn_file_actions_destroy(&actions);
        return {};
      }

      pid_t pid = -1;
      int rc = posix_spawnp(&pid, "clang-format", &actions, nullptr,
                            argv.data(), nullptr);
      posix_spawn_file_actions_destroy(&actions);
      if (rc != 0) return {};

      // Parent: close child-side fds, then write input and read output.
      ::close(in_pipe.release_read());
      ::close(out_pipe.release_write());

      // Stream the input. clang-format reads everything before
      // emitting; safe to do this fully before reading output as long
      // as the input fits in the pipe buffer or is consumed by the
      // child concurrently. Use POLLOUT-style write loop in case of a
      // large source file.
      const char* p = code.data();
      std::size_t remaining = code.size();
      int in_w = in_pipe.fds[1];
      while (remaining > 0) {
        auto n = ::write(in_w, p, remaining);
        if (n <= 0) {
          if (n < 0 && errno == EINTR) continue;
          break;
        }
        p += n;
        remaining -= static_cast<std::size_t>(n);
      }
      // Closing in_w signals EOF to clang-format.
      ::close(in_pipe.release_write());

      std::string result;
      std::array<char, 4096> buf;
      int out_r = out_pipe.fds[0];
      for (;;) {
        auto n = ::read(out_r, buf.data(), buf.size());
        if (n > 0) {
          result.append(buf.data(), static_cast<std::size_t>(n));
        } else if (n == 0) {
          break;
        } else if (errno != EINTR) {
          break;
        }
      }

      int status = 0;
      while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
          status = -1;
          break;
        }
      }

      if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
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
