#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xb::wsa {

  inline constexpr auto wsa_ns = "http://www.w3.org/2005/08/addressing";
  inline constexpr auto anonymous_uri =
      "http://www.w3.org/2005/08/addressing/anonymous";
  inline constexpr auto none_uri = "http://www.w3.org/2005/08/addressing/none";
  inline constexpr auto reply_relationship =
      "http://www.w3.org/2005/08/addressing/reply";

  struct endpoint_reference {
    std::string address;

    bool
    operator==(const endpoint_reference&) const = default;
  };

  struct relates_to {
    std::string uri;
    std::string relationship_type = std::string(reply_relationship);

    bool
    operator==(const relates_to&) const = default;
  };

  struct addressing_headers {
    std::optional<std::string> to;
    std::optional<std::string> action;
    std::optional<std::string> message_id;
    std::optional<endpoint_reference> reply_to;
    std::optional<endpoint_reference> fault_to;
    std::optional<endpoint_reference> from;
    std::vector<relates_to> relates_to_list;

    bool
    operator==(const addressing_headers&) const = default;
  };

} // namespace xb::wsa
