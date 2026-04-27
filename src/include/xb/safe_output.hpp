#pragma once

/// @file
/// Helpers for writing files in CLI codegen output paths in a way that
/// refuses to follow symlinks at the final path component.  An attacker
/// who can pre-create a symlink in a shared output directory could
/// otherwise redirect generated source into a sensitive location (e.g.
/// @c ~/.ssh/authorized_keys ).

#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace xb {

  /// Outcome of @ref write_text_file_no_follow.
  enum class write_outcome {
    ok,
    refused_symlink, ///< Path resolved to an existing symlink — refused.
    open_failed,     ///< open(2) failed for some other reason.
    write_failed,    ///< A short write or @c EIO occurred while writing.
  };

  /// Atomically open @p path for writing with @c O_NOFOLLOW so that an
  /// attacker-prepared symlink at the final component is rejected,
  /// truncate any existing regular file, write @p content, and close.
  /// Returns the outcome; @c errno_out (if non-null) receives @c errno
  /// on a non-ok result.
  inline write_outcome
  write_text_file_no_follow(const std::filesystem::path& path,
                            std::string_view content,
                            int* errno_out = nullptr) {
    int fd =
        ::open(path.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) {
      const int saved = errno;
      if (errno_out) *errno_out = saved;
      // ELOOP is the standard signal for "the final component is a
      // symlink and O_NOFOLLOW is set". Some platforms also report
      // EMLINK in this case.
      if (saved == ELOOP || saved == EMLINK) {
        return write_outcome::refused_symlink;
      }
      return write_outcome::open_failed;
    }

    const char* p = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
      auto n = ::write(fd, p, remaining);
      if (n < 0) {
        if (errno == EINTR) continue;
        const int saved = errno;
        ::close(fd);
        if (errno_out) *errno_out = saved;
        return write_outcome::write_failed;
      }
      if (n == 0) {
        ::close(fd);
        if (errno_out) *errno_out = 0;
        return write_outcome::write_failed;
      }
      p += n;
      remaining -= static_cast<std::size_t>(n);
    }

    if (::close(fd) != 0) {
      const int saved = errno;
      if (errno_out) *errno_out = saved;
      return write_outcome::write_failed;
    }
    return write_outcome::ok;
  }

} // namespace xb
