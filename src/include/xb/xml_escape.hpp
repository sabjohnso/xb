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
        case '\r':
          os << "&#13;";
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
        case '\t':
          os << "&#9;";
          break;
        case '\n':
          os << "&#10;";
          break;
        case '\r':
          os << "&#13;";
          break;
        default:
          os << c;
          break;
      }
    }
  }

} // namespace xb
