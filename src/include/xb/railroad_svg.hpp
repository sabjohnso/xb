#pragma once

#include <xb/railroad.hpp>

#include <iosfwd>
#include <string>

namespace xb::railroad {

  enum class color_scheme { light, dark };

  struct svg_options {
    double font_size = 14.0;
    double padding = 8.0;
    double arc_radius = 10.0;
    double line_height = 32.0;
    double char_width = 8.4; // approximate width per character
    std::string font_family = "monospace";
    std::string terminal_fill = "#ddeeff";
    std::string terminal_stroke = "#336699";
    std::string attribute_fill = "#fff3cd";
    std::string attribute_stroke = "#997700";
    std::string reference_fill = "#d4edda";
    std::string reference_stroke = "#28a745";
    std::string text_color = "#111111";
    std::string subtitle_color = "#666666";
    std::string line_color = "#333333";
    std::string background = "white";
    bool transparent_background = false;

    static svg_options
    for_scheme(color_scheme scheme);
  };

  /// Render a single diagram to SVG.
  void
  render_svg(const diagram& diag, std::ostream& out,
             const svg_options& opts = {});

  /// Render multiple diagrams to a single SVG document.
  void
  render_svg(const std::vector<diagram>& diagrams, std::ostream& out,
             const svg_options& opts = {});

} // namespace xb::railroad
