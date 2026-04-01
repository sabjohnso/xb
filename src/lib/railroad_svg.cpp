#include <xb/railroad_svg.hpp>

#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>

namespace xb::railroad {

  namespace {

    struct bounds {
      double width = 0;
      double height = 0;
    };

    std::string
    xml_escape(const std::string& s) {
      std::string result;
      for (char c : s) {
        switch (c) {
          case '&':
            result += "&amp;";
            break;
          case '<':
            result += "&lt;";
            break;
          case '>':
            result += "&gt;";
            break;
          case '"':
            result += "&quot;";
            break;
          default:
            result += c;
        }
      }
      return result;
    }

    double
    text_width(const std::string& text, const svg_options& opts) {
      return static_cast<double>(text.size()) * opts.char_width;
    }

    double
    box_width(const std::string& label, const svg_options& opts) {
      return text_width(label, opts) + opts.padding * 5;
    }

    double
    box_height(const svg_options& opts) {
      return opts.font_size * 2.2;
    }

    // -- Size computation (bottom-up) --

    bounds
    compute_size(const node& n, const svg_options& opts);

    bounds
    compute_terminal_size(const terminal& t, const svg_options& opts) {
      std::string display_label = t.label;
      if (t.is_attribute) display_label += t.required ? " (req)" : " (opt)";
      double w = box_width(display_label, opts);
      double h = box_height(opts);
      if (!t.type_label.empty()) {
        w = std::max(w, box_width(t.type_label, opts));
        h += opts.font_size * 0.9; // extra room for type label below box
      }
      return {w, h};
    }

    bounds
    compute_reference_size(const reference& r, const svg_options& opts) {
      double w = box_width(r.label, opts);
      double h = box_height(opts);
      std::string type_label = r.type_name.local_name();
      if (!type_label.empty()) {
        w = std::max(w, box_width(type_label, opts));
        h += opts.font_size * 0.9;
      }
      return {w, h};
    }

    bounds
    compute_sequence_size(const sequence& seq, const svg_options& opts) {
      double w = 0;
      double h = 0;
      double gap = opts.arc_radius;
      for (const auto& child : seq.children) {
        auto b = compute_size(child, opts);
        w += b.width + gap;
        h = std::max(h, b.height);
      }
      if (!seq.children.empty()) w -= gap;
      return {w, h};
    }

    bounds
    compute_choice_size(const choice& ch, const svg_options& opts) {
      double w = 0;
      double h = 0;
      double r = opts.arc_radius;
      double gap = r * 1.5;
      for (const auto& alt : ch.alternatives) {
        auto b = compute_size(alt, opts);
        w = std::max(w, b.width);
        h += b.height + gap;
      }
      if (!ch.alternatives.empty()) h -= gap;
      w += r * 4;
      return {w, h};
    }

    bounds
    compute_optional_size(const optional_node& opt, const svg_options& opts) {
      auto b = compute_size(*opt.child, opts);
      double r = opts.arc_radius;
      // Bypass must clear the entire child above the mainline.
      // Child extends b.height/2 above mainline; bypass needs r*2 clearance
      // above that plus r*2 for the arcs themselves.
      double bypass_overhead = b.height / 2 + r * 3;
      return {b.width + r * 4, b.height / 2 + bypass_overhead};
    }

    bounds
    compute_repeat_size(const repeat_node& rep, const svg_options& opts) {
      auto b = compute_size(*rep.child, opts);
      double r = opts.arc_radius;
      double label_w =
          rep.count_label.empty()
              ? 0
              : text_width(rep.count_label, opts) + opts.padding * 2;
      // Loop must clear below the entire child content.
      // Child extends b.height/2 below mainline; loop needs r*2 clearance
      // below that plus r*2 for the arcs.
      double loop_overhead = b.height / 2 + r * 3;
      return {std::max(b.width, label_w) + r * 4, b.height / 2 + loop_overhead};
    }

    bounds
    compute_size(const node& n, const svg_options& opts) {
      return std::visit(
          [&](const auto& content) -> bounds {
            using T = std::decay_t<decltype(content)>;
            if constexpr (std::is_same_v<T, terminal>)
              return compute_terminal_size(content, opts);
            else if constexpr (std::is_same_v<T, sequence>)
              return compute_sequence_size(content, opts);
            else if constexpr (std::is_same_v<T, choice>)
              return compute_choice_size(content, opts);
            else if constexpr (std::is_same_v<T, optional_node>)
              return compute_optional_size(content, opts);
            else if constexpr (std::is_same_v<T, repeat_node>)
              return compute_repeat_size(content, opts);
            else if constexpr (std::is_same_v<T, reference>)
              return compute_reference_size(content, opts);
            else
              return {0, 0};
          },
          n.content);
    }

    // -- SVG rendering primitives --

    void
    render_node(std::ostream& out, const node& n, double x, double y,
                const svg_options& opts);

    void
    render_line(std::ostream& out, double x1, double y1, double x2, double y2,
                const svg_options& opts) {
      out << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2
          << "\" y2=\"" << y2 << "\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"2\"/>\n";
    }

    void
    render_text(std::ostream& out, double x, double y, const std::string& text,
                const svg_options& opts, double size_factor = 1.0,
                const std::string& fill = "") {
      double fs = opts.font_size * size_factor;
      out << "<text x=\"" << x << "\" y=\"" << y << "\" font-family=\""
          << opts.font_family << "\" font-size=\"" << fs
          << "\" text-anchor=\"middle\" dominant-baseline=\"central\""
          << " fill=\"" << (fill.empty() ? opts.text_color : fill) << "\">"
          << xml_escape(text) << "</text>\n";
    }

    // Entry symbol: filled circle
    void
    render_entry(std::ostream& out, double x, double y,
                 const svg_options& opts) {
      double r = 5;
      out << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"" << r
          << "\" fill=\"" << opts.line_color << "\"/>\n";
    }

    // Exit symbol: double vertical bar
    void
    render_exit(std::ostream& out, double x, double y,
                const svg_options& opts) {
      double h = box_height(opts) * 0.4;
      out << "<line x1=\"" << x << "\" y1=\"" << y - h << "\" x2=\"" << x
          << "\" y2=\"" << y + h << "\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"2.5\"/>\n";
      out << "<line x1=\"" << x + 4 << "\" y1=\"" << y - h << "\" x2=\""
          << x + 4 << "\" y2=\"" << y + h << "\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"2.5\"/>\n";
    }

    // Pill shape (fully rounded ends) for terminals
    void
    render_pill(std::ostream& out, double x, double y, double w, double h,
                const std::string& fill, const std::string& stroke) {
      double r = h / 2;
      out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w
          << "\" height=\"" << h << "\" rx=\"" << r << "\" ry=\"" << r
          << "\" fill=\"" << fill << "\" stroke=\"" << stroke
          << "\" stroke-width=\"1.5\"/>\n";
    }

    // Rectangle with slight rounding for non-terminals/references
    void
    render_rect(std::ostream& out, double x, double y, double w, double h,
                const std::string& fill, const std::string& stroke) {
      out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w
          << "\" height=\"" << h << "\" rx=\"3\" ry=\"3\" fill=\"" << fill
          << "\" stroke=\"" << stroke << "\" stroke-width=\"1.5\"/>\n";
    }

    // Draw a rounded corner as a quarter-circle arc.
    // (cx, cy) is the corner point where two perpendicular lines meet.
    // Quadrant selects which corner shape to draw:
    //
    //   "tl" = ╭   "tr" = ╮
    //   "bl" = ╰   "br" = ╯
    //
    // The arc connects the two line endpoints adjacent to the corner,
    // replacing the sharp right angle with a smooth curve.
    //
    // Each corner has two possible directions of travel. The 'fwd' flag
    // selects the direction:
    //   tl fwd: bottom→right   tl rev: right→bottom
    //   tr fwd: left→bottom    tr rev: bottom→left
    //   bl fwd: top→right      bl rev: right→top
    //   br fwd: left→top       br rev: top→left
    //
    // (fwd = the "natural" reading direction for that corner shape)
    void
    corner(std::ostream& out, double cx, double cy, double r,
           const std::string& quadrant, bool fwd, const svg_options& opts) {
      double x1, y1, x2, y2;
      int sweep;

      if (quadrant == "tl") {
        // ╭ shape: connects (cx, cy+r) and (cx+r, cy)
        if (fwd) {
          x1 = cx;
          y1 = cy + r;
          x2 = cx + r;
          y2 = cy;
          sweep = 1;
        } else {
          x1 = cx + r;
          y1 = cy;
          x2 = cx;
          y2 = cy + r;
          sweep = 0;
        }
      } else if (quadrant == "tr") {
        // ╮ shape: connects (cx - r, cy) and (cx, cy + r)
        if (fwd) {
          x1 = cx - r;
          y1 = cy;
          x2 = cx;
          y2 = cy + r;
          sweep = 1;
        } else {
          x1 = cx;
          y1 = cy + r;
          x2 = cx - r;
          y2 = cy;
          sweep = 0;
        }
      } else if (quadrant == "bl") {
        // ╰ shape: connects (cx, cy - r) and (cx + r, cy)
        if (fwd) {
          x1 = cx;
          y1 = cy - r;
          x2 = cx + r;
          y2 = cy;
          sweep = 0;
        } else {
          x1 = cx + r;
          y1 = cy;
          x2 = cx;
          y2 = cy - r;
          sweep = 1;
        }
      } else { // "br"
        // ╯ shape: connects (cx - r, cy) and (cx, cy - r)
        if (fwd) {
          x1 = cx - r;
          y1 = cy;
          x2 = cx;
          y2 = cy - r;
          sweep = 0;
        } else {
          x1 = cx;
          y1 = cy - r;
          x2 = cx - r;
          y2 = cy;
          sweep = 1;
        }
      }

      out << "<path d=\"M " << x1 << " " << y1 << " A " << r << " " << r
          << " 0 0 " << sweep << " " << x2 << " " << y2
          << "\" fill=\"none\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"2\"/>\n";
    }

    // -- Node rendering --

    void
    render_terminal(std::ostream& out, const terminal& t, double x, double y,
                    const svg_options& opts) {
      auto sz = compute_terminal_size(t, opts);
      double bh = box_height(opts);
      double box_y = y - bh / 2;

      const auto& fill =
          t.is_attribute ? opts.attribute_fill : opts.terminal_fill;
      const auto& stroke =
          t.is_attribute ? opts.attribute_stroke : opts.terminal_stroke;

      // Terminals get pill shape, attributes get pill with different color
      render_pill(out, x, box_y, sz.width, bh, fill, stroke);

      // Label: for attributes, append required/optional indicator
      std::string display_label = t.label;
      if (t.is_attribute) display_label += t.required ? " (req)" : " (opt)";
      render_text(out, x + sz.width / 2, y, display_label, opts);

      // Type label below the box
      if (!t.type_label.empty()) {
        render_text(out, x + sz.width / 2, y + bh / 2 + opts.font_size * 0.55,
                    t.type_label, opts, 0.75, "#888888");
      }
    }

    void
    render_reference(std::ostream& out, const reference& r, double x, double y,
                     const svg_options& opts) {
      auto sz = compute_reference_size(r, opts);
      double bh = box_height(opts);
      double box_y = y - bh / 2;

      // References get rectangular shape
      render_rect(out, x, box_y, sz.width, bh, opts.reference_fill,
                  opts.reference_stroke);
      render_text(out, x + sz.width / 2, y, r.label, opts);

      // Type name below the box
      std::string type_label = r.type_name.local_name();
      if (!type_label.empty()) {
        render_text(out, x + sz.width / 2, y + bh / 2 + opts.font_size * 0.55,
                    type_label, opts, 0.75, "#888888");
      }
    }

    void
    render_sequence(std::ostream& out, const sequence& seq, double x, double y,
                    const svg_options& opts) {
      double cx = x;
      double gap = opts.arc_radius;
      for (std::size_t i = 0; i < seq.children.size(); ++i) {
        auto b = compute_size(seq.children[i], opts);

        if (i > 0) {
          render_line(out, cx, y, cx + gap, y, opts);
          cx += gap;
        }

        render_node(out, seq.children[i], cx, y, opts);
        cx += b.width;
      }
    }

    void
    render_choice(std::ostream& out, const choice& ch, double x, double y,
                  const svg_options& opts) {
      auto total = compute_choice_size(ch, opts);
      double r = opts.arc_radius;
      double gap = r * 1.5;

      // Compute y positions for each alternative
      std::vector<double> alt_y;
      double cur_y = y - total.height / 2;
      for (const auto& alt : ch.alternatives) {
        auto b = compute_size(alt, opts);
        alt_y.push_back(cur_y + b.height / 2);
        cur_y += b.height + gap;
      }

      double inner_x = x + r * 2;
      double right_x = x + total.width;

      for (std::size_t i = 0; i < ch.alternatives.size(); ++i) {
        auto b = compute_size(ch.alternatives[i], opts);
        double ay = alt_y[i];
        double alt_x = inner_x;

        if (std::abs(ay - y) < 0.5) {
          // Main line — straight through
          render_line(out, x, y, alt_x, y, opts);
          render_node(out, ch.alternatives[i], alt_x, ay, opts);
          render_line(out, alt_x + b.width, ay, right_x, y, opts);
        } else if (ay > y) {
          // Branch going DOWN: ╮ down ╰ → node → ╯ up ╭
          //
          //  mainline ──╮              ╭── mainline
          //              │              │
          //              ╰── [node] ───╯
          //
          // Left side
          corner(out, x + r, y, r, "tr", true, opts); // ╮
          if (y + r < ay - r)
            render_line(out, x + r, y + r, x + r, ay - r, opts);
          corner(out, x + r, ay, r, "bl", true, opts); // ╰

          render_node(out, ch.alternatives[i], alt_x, ay, opts);

          // Right side
          double rj = right_x - r;
          render_line(out, alt_x + b.width, ay, rj - r, ay, opts);
          corner(out, rj, ay, r, "br", false, opts); // ╯
          if (y + r < ay - r) render_line(out, rj, ay - r, rj, y + r, opts);
          corner(out, rj, y, r, "tl", false, opts); // ╭
        } else {
          // Branch going UP: ╯ up ╭ → node → ╮ down ╰
          //
          //              ╭── [node] ───╮
          //              │              │
          //  mainline ──╯              ╰── mainline
          //
          // Left side
          corner(out, x + r, y, r, "br", true, opts); // ╯
          if (ay + r < y - r)
            render_line(out, x + r, y - r, x + r, ay + r, opts);
          corner(out, x + r, ay, r, "tl", true, opts); // ╭

          render_node(out, ch.alternatives[i], alt_x, ay, opts);

          // Right side
          double rj = right_x - r;
          render_line(out, alt_x + b.width, ay, rj - r, ay, opts);
          corner(out, rj, ay, r, "tr", false, opts); // ╮
          if (ay + r < y - r) render_line(out, rj, ay + r, rj, y - r, opts);
          corner(out, rj, y, r, "bl", false, opts); // ╰
        }
      }
    }

    void
    render_optional(std::ostream& out, const optional_node& opt, double x,
                    double y, const svg_options& opts) {
      auto total = compute_optional_size(opt, opts);
      auto child_sz = compute_size(*opt.child, opts);
      double r = opts.arc_radius;

      double child_x = x + r * 2;
      double right_x = x + total.width;
      // Bypass must clear above the entire child content.
      // Child extends child_sz.height/2 above the mainline.
      double bypass_y = y - child_sz.height / 2 - r * 2;

      // Main path through child
      render_line(out, x, y, child_x, y, opts);
      render_node(out, *opt.child, child_x, y, opts);
      render_line(out, child_x + child_sz.width, y, right_x, y, opts);

      // Bypass path above: ╯ up ╭─────╮ down ╰
      //
      //         ╭──────────────────╮
      //         │                  │
      //   ──────╯   [child node]  ╰──────
      //
      // Bottom-left: mainline up
      corner(out, x + r, y, r, "br", true, opts); // ╯
      // Vertical up if needed
      if (y - r > bypass_y + r)
        render_line(out, x + r, y - r, x + r, bypass_y + r, opts);
      // Top-left corner
      corner(out, x + r, bypass_y, r, "tl", true, opts); // ╭
      // Horizontal across the top
      render_line(out, x + r * 2, bypass_y, right_x - r * 2, bypass_y, opts);
      // Top-right corner
      corner(out, right_x - r, bypass_y, r, "tr", true, opts); // ╮
      // Vertical down if needed
      if (y - r > bypass_y + r)
        render_line(out, right_x - r, bypass_y + r, right_x - r, y - r, opts);
      // Bottom-right: back to mainline
      corner(out, right_x - r, y, r, "bl", true, opts); // ╰
    }

    void
    render_repeat(std::ostream& out, const repeat_node& rep, double x, double y,
                  const svg_options& opts) {
      auto total = compute_repeat_size(rep, opts);
      auto child_sz = compute_size(*rep.child, opts);
      double r = opts.arc_radius;

      double child_x = x + r * 2;
      double right_x = x + total.width;
      // Loop must clear below the entire child content.
      double loop_y = y + child_sz.height / 2 + r * 2;

      // Main path through child
      render_line(out, x, y, child_x, y, opts);
      render_node(out, *rep.child, child_x, y, opts);
      render_line(out, child_x + child_sz.width, y, right_x, y, opts);

      // Loop-back path below: ╮ down ╯──←──╰ up ╭
      //
      //   ──────╭   [child node]  ╮──────
      //         │                  │
      //         ╰────────←─────────╯
      //
      // Top-right: mainline turns down
      corner(out, right_x - r, y, r, "tr", false, opts); // ╮
      // Vertical down if needed
      if (y + r < loop_y - r)
        render_line(out, right_x - r, y + r, right_x - r, loop_y - r, opts);
      // Bottom-right corner
      corner(out, right_x - r, loop_y, r, "br", false, opts); // ╯

      // Horizontal loop-back with arrow
      double loop_mid = (x + r * 2 + right_x - r * 2) / 2;
      render_line(out, right_x - r * 2, loop_y, loop_mid + 1, loop_y, opts);

      out << "<polygon points=\"" << loop_mid + 6 << "," << loop_y - 4 << " "
          << loop_mid << "," << loop_y << " " << loop_mid + 6 << ","
          << loop_y + 4 << "\" fill=\"" << opts.line_color << "\"/>\n";

      render_line(out, loop_mid, loop_y, x + r * 2, loop_y, opts);

      // Bottom-left corner
      corner(out, x + r, loop_y, r, "bl", false, opts); // ╰
      // Vertical up if needed
      if (y + r < loop_y - r)
        render_line(out, x + r, loop_y - r, x + r, y + r, opts);
      // Top-left: back to mainline
      corner(out, x + r, y, r, "tl", false, opts); // ╭

      // Count label below loop
      if (!rep.count_label.empty()) {
        render_text(out, (x + right_x) / 2, loop_y + opts.font_size * 0.8,
                    rep.count_label, opts, 0.75, "#666666");
      }
    }

    void
    render_node(std::ostream& out, const node& n, double x, double y,
                const svg_options& opts) {
      std::visit(
          [&](const auto& content) {
            using T = std::decay_t<decltype(content)>;
            if constexpr (std::is_same_v<T, terminal>)
              render_terminal(out, content, x, y, opts);
            else if constexpr (std::is_same_v<T, sequence>)
              render_sequence(out, content, x, y, opts);
            else if constexpr (std::is_same_v<T, choice>)
              render_choice(out, content, x, y, opts);
            else if constexpr (std::is_same_v<T, optional_node>)
              render_optional(out, content, x, y, opts);
            else if constexpr (std::is_same_v<T, repeat_node>)
              render_repeat(out, content, x, y, opts);
            else if constexpr (std::is_same_v<T, reference>)
              render_reference(out, content, x, y, opts);
          },
          n.content);
    }

    void
    render_diagram_body(std::ostream& out, const diagram& diag, double x,
                        double y, const svg_options& opts) {
      auto sz = compute_size(diag.root, opts);

      // Title
      out << "<text x=\"" << x << "\" y=\"" << y << "\" font-family=\""
          << opts.font_family << "\" font-size=\"" << opts.font_size * 1.4
          << "\" font-weight=\"bold\">" << xml_escape(diag.name) << "</text>\n";

      double content_y = y + opts.line_height * 1.5;
      double center_y = content_y + sz.height / 2;

      double entry_x = x;
      double body_x = entry_x + opts.arc_radius * 3;
      double exit_x = body_x + sz.width + opts.arc_radius;

      // Entry: filled circle + line
      render_entry(out, entry_x + 5, center_y, opts);
      render_line(out, entry_x + 10, center_y, body_x, center_y, opts);

      // Main diagram
      render_node(out, diag.root, body_x, center_y, opts);

      // Exit: line + double bar
      render_line(out, body_x + sz.width, center_y, exit_x, center_y, opts);
      render_exit(out, exit_x + 4, center_y, opts);

      // Attributes below
      if (!diag.attributes.empty()) {
        double attr_y = content_y + sz.height + opts.line_height * 1.2;
        double attr_x = x + opts.padding;

        out << "<text x=\"" << attr_x << "\" y=\"" << attr_y
            << "\" font-family=\"" << opts.font_family << "\" font-size=\""
            << opts.font_size * 0.85 << "\" fill=\"#666\">attributes:</text>\n";

        attr_x += text_width("attributes: ", opts);
        for (const auto& attr : diag.attributes) {
          auto asz = compute_terminal_size(attr, opts);
          render_terminal(out, attr, attr_x, attr_y, opts);
          attr_x += asz.width + opts.padding * 2;
        }
      }
    }

    double
    diagram_height(const diagram& diag, const svg_options& opts) {
      auto sz = compute_size(diag.root, opts);
      double h = opts.line_height * 1.5 + sz.height + opts.padding * 4;
      if (!diag.attributes.empty()) h += opts.line_height * 2;
      return h;
    }

  } // namespace

  void
  render_svg(const diagram& diag, std::ostream& out, const svg_options& opts) {
    auto sz = compute_size(diag.root, opts);
    double margin = opts.padding * 4;
    double total_w = sz.width + opts.arc_radius * 6 + margin * 2;
    double total_h = diagram_height(diag, opts) + margin * 2;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << total_w
        << "\" height=\"" << total_h << "\">\n";

    out << "<defs><marker id=\"arrow\" markerWidth=\"6\" markerHeight=\"6\" "
           "refX=\"3\" refY=\"3\" orient=\"auto\">"
           "<path d=\"M 0 0 L 6 3 L 0 6 Z\" fill=\""
        << opts.line_color << "\"/></marker></defs>\n";

    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << opts.background
        << "\"/>\n";

    render_diagram_body(out, diag, margin, margin + opts.font_size, opts);

    out << "</svg>\n";
  }

  void
  render_svg(const std::vector<diagram>& diagrams, std::ostream& out,
             const svg_options& opts) {
    double margin = opts.padding * 4;
    double total_h = margin;
    double total_w = 0;

    for (const auto& diag : diagrams) {
      auto sz = compute_size(diag.root, opts);
      double w = sz.width + opts.arc_radius * 6 + margin * 2;
      total_w = std::max(total_w, w);
      total_h += diagram_height(diag, opts) + opts.padding * 3;
    }
    total_h += margin;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << total_w
        << "\" height=\"" << total_h << "\">\n";

    out << "<defs><marker id=\"arrow\" markerWidth=\"6\" markerHeight=\"6\" "
           "refX=\"3\" refY=\"3\" orient=\"auto\">"
           "<path d=\"M 0 0 L 6 3 L 0 6 Z\" fill=\""
        << opts.line_color << "\"/></marker></defs>\n";

    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << opts.background
        << "\"/>\n";

    double y = margin + opts.font_size;
    for (const auto& diag : diagrams) {
      render_diagram_body(out, diag, margin, y, opts);
      y += diagram_height(diag, opts) + opts.padding * 3;
    }

    out << "</svg>\n";
  }

} // namespace xb::railroad
