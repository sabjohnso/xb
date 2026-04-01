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

    // Escape XML special characters for SVG text content.
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
      return text_width(label, opts) + opts.padding * 4;
    }

    // -- Size computation (bottom-up) --

    bounds
    compute_size(const node& n, const svg_options& opts);

    bounds
    compute_terminal_size(const terminal& t, const svg_options& opts) {
      double w = box_width(t.label, opts);
      if (!t.type_label.empty()) w = std::max(w, box_width(t.type_label, opts));
      return {w, opts.line_height};
    }

    bounds
    compute_reference_size(const reference& r, const svg_options& opts) {
      return {box_width(r.label, opts), opts.line_height};
    }

    bounds
    compute_sequence_size(const sequence& seq, const svg_options& opts) {
      double w = 0;
      double h = 0;
      for (const auto& child : seq.children) {
        auto b = compute_size(child, opts);
        w += b.width + opts.arc_radius * 2;
        h = std::max(h, b.height);
      }
      if (!seq.children.empty()) w -= opts.arc_radius * 2; // no gap after last
      return {w, h};
    }

    bounds
    compute_choice_size(const choice& ch, const svg_options& opts) {
      double w = 0;
      double h = 0;
      for (const auto& alt : ch.alternatives) {
        auto b = compute_size(alt, opts);
        w = std::max(w, b.width);
        h += b.height;
      }
      // Add space for branch curves
      w += opts.arc_radius * 4;
      if (ch.alternatives.size() > 1)
        h += opts.padding * static_cast<double>(ch.alternatives.size() - 1);
      return {w, h};
    }

    bounds
    compute_optional_size(const optional_node& opt, const svg_options& opts) {
      auto b = compute_size(*opt.child, opts);
      // Extra height for bypass path
      return {b.width + opts.arc_radius * 4, b.height + opts.line_height * 0.5};
    }

    bounds
    compute_repeat_size(const repeat_node& rep, const svg_options& opts) {
      auto b = compute_size(*rep.child, opts);
      double label_w =
          rep.count_label.empty()
              ? 0
              : text_width(rep.count_label, opts) + opts.padding * 2;
      return {std::max(b.width, label_w) + opts.arc_radius * 4,
              b.height + opts.line_height * 0.75};
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

    // -- SVG rendering (top-down) --

    // All render functions draw at (x, y) where y is the center line.

    void
    render_node(std::ostream& out, const node& n, double x, double y,
                const svg_options& opts);

    void
    render_box(std::ostream& out, double x, double y, double w, double h,
               const std::string& fill, const std::string& stroke,
               double rx = 4) {
      out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w
          << "\" height=\"" << h << "\" rx=\"" << rx << "\" fill=\"" << fill
          << "\" stroke=\"" << stroke << "\" stroke-width=\"1.5\"/>\n";
    }

    void
    render_text(std::ostream& out, double x, double y, const std::string& text,
                const svg_options& opts, double size_factor = 1.0) {
      double fs = opts.font_size * size_factor;
      out << "<text x=\"" << x << "\" y=\"" << y << "\" font-family=\""
          << opts.font_family << "\" font-size=\"" << fs
          << "\" text-anchor=\"middle\" dominant-baseline=\"central\">"
          << xml_escape(text) << "</text>\n";
    }

    void
    render_line(std::ostream& out, double x1, double y1, double x2, double y2,
                const svg_options& opts) {
      out << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2
          << "\" y2=\"" << y2 << "\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"1.5\"/>\n";
    }

    void
    render_terminal(std::ostream& out, const terminal& t, double x, double y,
                    const svg_options& opts) {
      auto sz = compute_terminal_size(t, opts);
      double box_h = opts.line_height * 0.8;
      double box_y = y - box_h / 2;

      const auto& fill =
          t.is_attribute ? opts.attribute_fill : opts.terminal_fill;
      const auto& stroke =
          t.is_attribute ? opts.attribute_stroke : opts.terminal_stroke;

      render_box(out, x, box_y, sz.width, box_h, fill, stroke,
                 t.is_attribute ? box_h / 2 : 4);
      render_text(out, x + sz.width / 2, y, t.label, opts);

      if (!t.type_label.empty()) {
        render_text(out, x + sz.width / 2, y + box_h * 0.35, t.type_label, opts,
                    0.7);
      }
    }

    void
    render_reference(std::ostream& out, const reference& r, double x, double y,
                     const svg_options& opts) {
      auto sz = compute_reference_size(r, opts);
      double box_h = opts.line_height * 0.8;
      double box_y = y - box_h / 2;

      render_box(out, x, box_y, sz.width, box_h, opts.reference_fill,
                 opts.reference_stroke, 4);
      render_text(out, x + sz.width / 2, y, r.label, opts);
    }

    void
    render_sequence(std::ostream& out, const sequence& seq, double x, double y,
                    const svg_options& opts) {
      double cx = x;
      for (std::size_t i = 0; i < seq.children.size(); ++i) {
        auto b = compute_size(seq.children[i], opts);

        if (i > 0) {
          // Connect with horizontal line
          render_line(out, cx, y, cx + opts.arc_radius * 2, y, opts);
          cx += opts.arc_radius * 2;
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

      // Compute y positions for each alternative
      std::vector<double> alt_y;
      double cur_y = y - total.height / 2;
      for (const auto& alt : ch.alternatives) {
        auto b = compute_size(alt, opts);
        alt_y.push_back(cur_y + b.height / 2);
        cur_y += b.height + opts.padding;
      }

      double inner_x = x + r * 2;
      double inner_w = total.width - r * 4;

      for (std::size_t i = 0; i < ch.alternatives.size(); ++i) {
        auto b = compute_size(ch.alternatives[i], opts);
        double ay = alt_y[i];

        // Left branch: arc from (x, y) down/up to (inner_x, ay)
        out << "<path d=\"M " << x << " " << y << " Q " << x + r << " " << y
            << " " << x + r << " " << (y + ay) / 2 << " T " << inner_x << " "
            << ay << "\" fill=\"none\" stroke=\"" << opts.line_color
            << "\" stroke-width=\"1.5\"/>\n";

        // Render alternative centered
        double alt_x = inner_x + (inner_w - b.width) / 2;
        render_node(out, ch.alternatives[i], alt_x, ay, opts);

        // Right branch: arc from end of alt to (x+total.width, y)
        double right_x = x + total.width;
        out << "<path d=\"M " << alt_x + b.width << " " << ay << " Q "
            << right_x - r << " " << ay << " " << right_x - r << " "
            << (y + ay) / 2 << " T " << right_x << " " << y
            << "\" fill=\"none\" stroke=\"" << opts.line_color
            << "\" stroke-width=\"1.5\"/>\n";
      }
    }

    void
    render_optional(std::ostream& out, const optional_node& opt, double x,
                    double y, const svg_options& opts) {
      auto total = compute_optional_size(opt, opts);
      auto child_sz = compute_size(*opt.child, opts);
      double r = opts.arc_radius;

      double child_x = x + r * 2;
      double child_y = y;

      // Main path through child
      render_line(out, x, y, child_x, y, opts);
      render_node(out, *opt.child, child_x, child_y, opts);
      render_line(out, child_x + child_sz.width, y, x + total.width, y, opts);

      // Bypass path (arc above)
      double bypass_y = y - opts.line_height * 0.4;
      out << "<path d=\"M " << x << " " << y << " Q " << x << " " << bypass_y
          << " " << x + r << " " << bypass_y << " L " << x + total.width - r
          << " " << bypass_y << " Q " << x + total.width << " " << bypass_y
          << " " << x + total.width << " " << y << "\" fill=\"none\" stroke=\""
          << opts.line_color
          << "\" stroke-width=\"1.5\" stroke-dasharray=\"4,3\"/>\n";
    }

    void
    render_repeat(std::ostream& out, const repeat_node& rep, double x, double y,
                  const svg_options& opts) {
      auto total = compute_repeat_size(rep, opts);
      auto child_sz = compute_size(*rep.child, opts);
      double r = opts.arc_radius;

      double child_x = x + r * 2;

      // Main path through child
      render_line(out, x, y, child_x, y, opts);
      render_node(out, *rep.child, child_x, y, opts);
      render_line(out, child_x + child_sz.width, y, x + total.width, y, opts);

      // Loop-back path (arc below)
      double loop_y = y + opts.line_height * 0.55;
      out << "<path d=\"M " << x + total.width << " " << y << " Q "
          << x + total.width << " " << loop_y << " " << x + total.width - r
          << " " << loop_y << " L " << x + r << " " << loop_y << " Q " << x
          << " " << loop_y << " " << x << " " << y
          << "\" fill=\"none\" stroke=\"" << opts.line_color
          << "\" stroke-width=\"1.5\" marker-mid=\"url(#arrow)\"/>\n";

      // Count label
      if (!rep.count_label.empty()) {
        render_text(out, x + total.width / 2, loop_y + opts.font_size * 0.4,
                    rep.count_label, opts, 0.75);
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
      double title_y = y;
      out << "<text x=\"" << x << "\" y=\"" << title_y << "\" font-family=\""
          << opts.font_family << "\" font-size=\"" << opts.font_size * 1.2
          << "\" font-weight=\"bold\">" << xml_escape(diag.name) << "</text>\n";

      double content_y = title_y + opts.line_height * 1.2;
      double center_y = content_y + sz.height / 2;

      // Entry line
      double entry_x = x;
      render_line(out, entry_x, center_y, entry_x + opts.arc_radius * 2,
                  center_y, opts);

      // Main diagram
      double body_x = entry_x + opts.arc_radius * 2;
      render_node(out, diag.root, body_x, center_y, opts);

      // Exit line
      double exit_x = body_x + sz.width;
      render_line(out, exit_x, center_y, exit_x + opts.arc_radius * 2, center_y,
                  opts);

      // Attributes (rendered as a row below the main diagram)
      if (!diag.attributes.empty()) {
        double attr_y = content_y + sz.height + opts.line_height;
        double attr_x = x + opts.padding;

        out << "<text x=\"" << attr_x << "\" y=\"" << attr_y
            << "\" font-family=\"" << opts.font_family << "\" font-size=\""
            << opts.font_size * 0.85 << "\" fill=\"#666\">attributes:</text>\n";

        attr_x += text_width("attributes: ", opts);
        for (const auto& attr : diag.attributes) {
          auto asz = compute_terminal_size(attr, opts);
          render_terminal(out, attr, attr_x, attr_y, opts);
          attr_x += asz.width + opts.padding;
        }
      }
    }

    double
    diagram_height(const diagram& diag, const svg_options& opts) {
      auto sz = compute_size(diag.root, opts);
      double h = opts.line_height * 1.2 + sz.height + opts.padding * 2;
      if (!diag.attributes.empty()) h += opts.line_height * 1.5;
      return h;
    }

  } // namespace

  void
  render_svg(const diagram& diag, std::ostream& out, const svg_options& opts) {
    auto sz = compute_size(diag.root, opts);
    double margin = opts.padding * 3;
    double total_w = sz.width + opts.arc_radius * 4 + margin * 2;
    double total_h = diagram_height(diag, opts) + margin * 2;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << total_w
        << "\" height=\"" << total_h << "\">\n";

    // Arrow marker definition
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
    double margin = opts.padding * 3;
    double total_h = margin;
    double total_w = 0;

    // Compute total height and max width
    for (const auto& diag : diagrams) {
      auto sz = compute_size(diag.root, opts);
      double w = sz.width + opts.arc_radius * 4 + margin * 2;
      total_w = std::max(total_w, w);
      total_h += diagram_height(diag, opts) + opts.padding * 2;
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
      y += diagram_height(diag, opts) + opts.padding * 2;
    }

    out << "</svg>\n";
  }

} // namespace xb::railroad
