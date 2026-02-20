#pragma once

#include <ostream>
#include <string_view>

namespace xb {

  inline void
  escape_text(std::ostream& os, std::string_view text) {
    for (char c : text) {
      switch (c) {
        case '<':
          os << "&lt;";
          break;
        case '>':
          os << "&gt;";
          break;
        case '&':
          os << "&amp;";
          break;
        default:
          os << c;
          break;
      }
    }
  }

  inline void
  escape_attribute(std::ostream& os, std::string_view text) {
    for (char c : text) {
      switch (c) {
        case '<':
          os << "&lt;";
          break;
        case '>':
          os << "&gt;";
          break;
        case '&':
          os << "&amp;";
          break;
        case '"':
          os << "&quot;";
          break;
        default:
          os << c;
          break;
      }
    }
  }

} // namespace xb
