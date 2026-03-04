#include <xb/mime_multipart.hpp>

#include <algorithm>
#include <random>

namespace xb::mime {

  namespace {

    void
    append_string(std::vector<std::byte>& out, const std::string& s) {
      for (char c : s) {
        out.push_back(static_cast<std::byte>(c));
      }
    }

    void
    append_crlf(std::vector<std::byte>& out) {
      out.push_back(static_cast<std::byte>('\r'));
      out.push_back(static_cast<std::byte>('\n'));
    }

    std::string
    bytes_to_string(const std::byte* begin, const std::byte* end) {
      std::string result;
      result.reserve(static_cast<std::size_t>(end - begin));
      for (auto p = begin; p != end; ++p) {
        result.push_back(static_cast<char>(*p));
      }
      return result;
    }

    // Find the next occurrence of needle in haystack starting at pos
    const std::byte*
    find_boundary(const std::byte* data, std::size_t size,
                  const std::string& boundary_line) {
      if (size < boundary_line.size()) return nullptr;
      auto end = data + size - boundary_line.size() + 1;
      for (auto p = data; p < end; ++p) {
        bool match = true;
        for (std::size_t i = 0; i < boundary_line.size(); ++i) {
          if (static_cast<char>(p[i]) != boundary_line[i]) {
            match = false;
            break;
          }
        }
        if (match) return p;
      }
      return nullptr;
    }

    // Skip past CRLF at the given position
    const std::byte*
    skip_crlf(const std::byte* p, const std::byte* end) {
      if (p < end && static_cast<char>(*p) == '\r') ++p;
      if (p < end && static_cast<char>(*p) == '\n') ++p;
      return p;
    }

    // Parse headers from a part, returning pointer past the blank line
    struct parsed_headers {
      std::string content_type;
      std::string content_id;
      std::string content_transfer_encoding;
      const std::byte* body_start;
    };

    parsed_headers
    parse_part_headers(const std::byte* begin, const std::byte* end) {
      parsed_headers result;
      auto p = begin;

      while (p < end) {
        // Find end of line
        auto line_end = p;
        while (line_end < end && static_cast<char>(*line_end) != '\r' &&
               static_cast<char>(*line_end) != '\n') {
          ++line_end;
        }

        // Empty line signals end of headers
        if (line_end == p) {
          result.body_start = skip_crlf(p, end);
          return result;
        }

        auto line = bytes_to_string(p, line_end);

        // Parse header name: value
        auto colon = line.find(':');
        if (colon != std::string::npos) {
          auto name = line.substr(0, colon);
          auto value = line.substr(colon + 1);
          // Trim leading whitespace from value
          auto vstart = value.find_first_not_of(' ');
          if (vstart != std::string::npos) { value = value.substr(vstart); }

          // Case-insensitive header matching
          std::string lower_name = name;
          std::transform(lower_name.begin(), lower_name.end(),
                         lower_name.begin(),
                         [](unsigned char c) { return std::tolower(c); });

          if (lower_name == "content-type") {
            result.content_type = value;
          } else if (lower_name == "content-id") {
            // Strip angle brackets if present
            if (value.size() >= 2 && value.front() == '<' &&
                value.back() == '>') {
              value = value.substr(1, value.size() - 2);
            }
            result.content_id = value;
          } else if (lower_name == "content-transfer-encoding") {
            result.content_transfer_encoding = value;
          }
        }

        p = skip_crlf(line_end, end);
      }

      result.body_start = end;
      return result;
    }

  } // namespace

  std::string
  generate_boundary() {
    static constexpr auto chars =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static constexpr std::size_t len = 30;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, 61);

    std::string boundary = "----=_Part_";
    boundary.reserve(boundary.size() + len);
    for (std::size_t i = 0; i < len; ++i) {
      boundary += chars[dist(gen)];
    }
    return boundary;
  }

  std::vector<std::byte>
  serialize_multipart(const multipart_message& msg) {
    std::vector<std::byte> result;
    auto delim = "--" + msg.boundary;

    for (const auto& part : msg.parts) {
      append_string(result, delim);
      append_crlf(result);

      // Headers
      if (!part.content_type.empty()) {
        append_string(result, "Content-Type: " + part.content_type);
        append_crlf(result);
      }
      if (!part.content_id.empty()) {
        append_string(result, "Content-ID: <" + part.content_id + ">");
        append_crlf(result);
      }
      if (!part.content_transfer_encoding.empty()) {
        append_string(result, "Content-Transfer-Encoding: " +
                                  part.content_transfer_encoding);
        append_crlf(result);
      }

      // Blank line between headers and body
      append_crlf(result);

      // Body
      result.insert(result.end(), part.body.begin(), part.body.end());
      append_crlf(result);
    }

    // Closing delimiter
    append_string(result, delim + "--");
    append_crlf(result);

    return result;
  }

  multipart_message
  parse_multipart(const std::vector<std::byte>& data,
                  const std::string& boundary) {
    multipart_message result;
    result.boundary = boundary;

    auto delim = "--" + boundary;
    auto closing = delim + "--";
    auto* begin = data.data();
    auto* end = begin + data.size();

    // Find first boundary
    auto* pos = find_boundary(begin, data.size(), delim);
    if (!pos) return result;

    // Skip past the first boundary line
    pos += delim.size();
    pos = skip_crlf(pos, end);

    while (pos < end) {
      // Check for closing boundary
      auto remaining = static_cast<std::size_t>(end - pos);
      auto* next = find_boundary(pos, remaining, delim);

      const std::byte* part_end;
      bool is_last = false;

      if (next) {
        // Part ends before the next boundary
        // Back up past the CRLF before the boundary
        part_end = next;
        if (part_end > pos && static_cast<char>(*(part_end - 1)) == '\n')
          --part_end;
        if (part_end > pos && static_cast<char>(*(part_end - 1)) == '\r')
          --part_end;

        // Check if this is the closing boundary
        auto after_delim = next + delim.size();
        if (after_delim + 1 < end && static_cast<char>(*after_delim) == '-' &&
            static_cast<char>(*(after_delim + 1)) == '-') {
          is_last = true;
        }
      } else {
        part_end = end;
        is_last = true;
      }

      // Parse part headers and body
      auto headers = parse_part_headers(pos, part_end);

      mime_part part;
      part.content_type = std::move(headers.content_type);
      part.content_id = std::move(headers.content_id);
      part.content_transfer_encoding =
          std::move(headers.content_transfer_encoding);
      if (headers.body_start < part_end) {
        part.body.assign(headers.body_start, part_end);
      }
      result.parts.push_back(std::move(part));

      if (is_last) break;

      // Advance past the boundary
      pos = next + delim.size();
      pos = skip_crlf(pos, end);
    }

    return result;
  }

  std::string
  mtom_content_type(const std::string& boundary,
                    const std::string& start_content_id) {
    return "multipart/related; "
           "type=\"application/xop+xml\"; "
           "boundary=\"" +
           boundary +
           "\"; "
           "start=\"<" +
           start_content_id + ">\"";
  }

  std::string
  extract_boundary(const std::string& content_type) {
    auto pos = content_type.find("boundary=");
    if (pos == std::string::npos) return {};

    pos += 9; // length of "boundary="
    if (pos < content_type.size() && content_type[pos] == '"') {
      // Quoted boundary
      ++pos;
      auto end = content_type.find('"', pos);
      if (end == std::string::npos) return {};
      return content_type.substr(pos, end - pos);
    }

    // Unquoted boundary
    auto end = content_type.find_first_of("; \t", pos);
    if (end == std::string::npos) return content_type.substr(pos);
    return content_type.substr(pos, end - pos);
  }

} // namespace xb::mime
