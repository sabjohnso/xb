#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xb::mime {

  struct mime_part {
    std::string content_type;
    std::string content_id;
    std::string content_transfer_encoding;
    std::vector<std::byte> body;

    bool
    operator==(const mime_part&) const = default;
  };

  struct multipart_message {
    std::string boundary;
    std::vector<mime_part> parts;

    bool
    operator==(const multipart_message&) const = default;
  };

  std::string
  generate_boundary();

  std::vector<std::byte>
  serialize_multipart(const multipart_message& msg);

  multipart_message
  parse_multipart(const std::vector<std::byte>& data,
                  const std::string& boundary);

  std::string
  mtom_content_type(const std::string& boundary,
                    const std::string& start_content_id);

  std::string
  extract_boundary(const std::string& content_type);

} // namespace xb::mime
