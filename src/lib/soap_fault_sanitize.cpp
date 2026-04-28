#include <xb/soap_fault_sanitize.hpp>

#include <cctype>
#include <string>
#include <string_view>

namespace xb::soap {

  namespace {

    /// True if @p c is a valid hexadecimal digit.
    bool
    is_hex(char c) {
      return std::isdigit(static_cast<unsigned char>(c)) ||
             (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    /// True if @p c can appear inside a path segment.  Conservatively
    /// includes alphanumerics plus a few punctuation characters that
    /// commonly appear in real paths.
    bool
    is_path_char(char c) {
      return std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
             c == '\\' || c == '.' || c == '-' || c == '_';
    }

    /// Replace every absolute-path-like substring with @c <path>.
    /// Recognises both Unix (=/foo/bar=) and Windows (=C:\foo\bar=)
    /// shapes.  A single bare slash or "/" + 1 char is not enough —
    /// we require at least one path separator after the leading
    /// component.
    std::string
    redact_paths(std::string_view in) {
      std::string out;
      out.reserve(in.size());
      std::size_t i = 0;
      while (i < in.size()) {
        bool unix_path = (in[i] == '/');
        bool windows_path =
            (i + 2 < in.size() &&
             std::isalpha(static_cast<unsigned char>(in[i])) &&
             in[i + 1] == ':' && (in[i + 2] == '\\' || in[i + 2] == '/'));
        if (unix_path || windows_path) {
          // Scan forward as long as we keep seeing path characters.
          std::size_t start = i;
          if (windows_path)
            i += 3; // skip "C:\"
          else
            i += 1; // skip "/"
          while (i < in.size() && is_path_char(in[i]))
            ++i;
          // Was this actually long enough to be a path? At minimum
          // we want a separator inside (so plain "/" or "/foo" with
          // no further slash is left alone — that's a JSON pointer
          // or a URL fragment, not a filesystem path).
          auto len = i - start;
          std::string_view candidate = in.substr(start, len);
          bool has_inner_separator =
              candidate.size() > 1 &&
              candidate.find_first_of("/\\", 1) != std::string_view::npos;
          if (has_inner_separator && len >= 4) {
            out += "<path>";
          } else {
            out.append(candidate);
          }
          continue;
        }
        out += in[i++];
      }
      return out;
    }

    /// Replace every hex-address-like substring (=0x= prefix followed
    /// by 8 or more hex digits) with @c <addr>.
    std::string
    redact_addresses(std::string_view in) {
      std::string out;
      out.reserve(in.size());
      std::size_t i = 0;
      while (i < in.size()) {
        if (i + 1 < in.size() && in[i] == '0' &&
            (in[i + 1] == 'x' || in[i + 1] == 'X')) {
          std::size_t j = i + 2;
          while (j < in.size() && is_hex(in[j]))
            ++j;
          if (j - (i + 2) >= 8) {
            out += "<addr>";
            i = j;
            continue;
          }
        }
        out += in[i++];
      }
      return out;
    }

    /// Drop SQL-error-shaped runs (entries that contain a recognised
    /// constraint / syntax phrase).  Replaces the matching substring
    /// (greedy, up to the next sentence boundary) with @c <sql-error>.
    std::string
    redact_sql_fragments(std::string_view in) {
      static constexpr std::string_view triggers[] = {
          "UNIQUE constraint",
          "FOREIGN KEY constraint",
          "NOT NULL constraint",
          "syntax error",
          "near \"",
      };

      // Lower-case copy for case-insensitive matching.
      std::string lower(in);
      for (auto& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      std::string out;
      out.reserve(in.size());
      std::size_t i = 0;
      while (i < in.size()) {
        std::size_t earliest = std::string::npos;
        std::size_t earliest_len = 0;
        for (auto t : triggers) {
          std::string lower_t(t);
          for (auto& c : lower_t)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          auto pos = lower.find(lower_t, i);
          if (pos != std::string::npos &&
              (earliest == std::string::npos || pos < earliest)) {
            earliest = pos;
            earliest_len = t.size();
          }
        }
        if (earliest == std::string::npos) {
          out.append(in.substr(i));
          break;
        }
        out.append(in.substr(i, earliest - i));
        // Emit placeholder, then skip to the next sentence boundary.
        out += "<sql-error>";
        std::size_t end = earliest + earliest_len;
        while (end < in.size() && in[end] != '.' && in[end] != '\n' &&
               in[end] != '\r')
          ++end;
        i = end;
      }
      return out;
    }

    std::string
    apply_pattern_redaction(std::string_view in) {
      // Order matters: redact paths before addresses (a hex address
      // would never live inside a path), and SQL fragments before
      // either to avoid matching their tokens as paths.
      auto step1 = redact_sql_fragments(in);
      auto step2 = redact_paths(step1);
      auto step3 = redact_addresses(step2);
      return step3;
    }

    void
    redact(std::string& s, const fault_sanitize_options& opts) {
      if (opts.replace_with_generic) {
        s = opts.generic_message;
      } else {
        s = apply_pattern_redaction(s);
      }
    }

    void
    clear_or_replace(std::string& s, const fault_sanitize_options& opts) {
      if (opts.replace_with_generic) {
        s.clear();
      } else {
        s = apply_pattern_redaction(s);
      }
    }

    void
    sanitize_one(fault_1_1& f, const fault_sanitize_options& opts) {
      redact(f.fault_string, opts);
      clear_or_replace(f.fault_actor, opts);
      // The detail element commonly carries an application's internal
      // exception payload.  Drop it unconditionally — callers that have
      // an audit-safe detail block can attach it AFTER sanitisation.
      f.detail.reset();
    }

    void
    sanitize_one(fault_1_2& f, const fault_sanitize_options& opts) {
      for (auto& rt : f.reason) {
        redact(rt.text, opts);
      }
      clear_or_replace(f.node, opts);
      clear_or_replace(f.role, opts);
      f.detail.reset();
    }

  } // namespace

  void
  sanitize_fault(fault& f, const fault_sanitize_options& opts) {
    if (auto* f11 = std::get_if<fault_1_1>(&f)) {
      sanitize_one(*f11, opts);
    } else if (auto* f12 = std::get_if<fault_1_2>(&f)) {
      sanitize_one(*f12, opts);
    }
  }

} // namespace xb::soap
