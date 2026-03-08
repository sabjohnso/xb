#pragma once

#include <string>

namespace xb {

  /// Check whether clang-format is available on the system PATH.
  bool
  clang_format_available();

  /// Format C++ code using clang-format.
  ///
  /// If clang-format is not available, returns the input unchanged.
  /// @param code        The C++ source code to format.
  /// @param filename    Assumed filename (used by clang-format for style
  ///                    lookup).
  /// @param style_file  Optional path to a .clang-format file. If non-empty,
  ///                    clang-format uses this style; otherwise it searches
  ///                    from the assumed filename's directory.
  std::string
  format_cpp_code(const std::string& code, const std::string& filename,
                  const std::string& style_file = {});

} // namespace xb
