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

  /// Options applied when extracting WS-Addressing headers from an
  /// incoming envelope.  An attacker who can place arbitrary URIs in
  /// @c ReplyTo / @c FaultTo / @c From can otherwise turn any service
  /// that honours those headers into an SSRF gadget.
  struct endpoint_validation_options {
    /// Endpoint addresses (URIs) the receiver is willing to route
    /// asynchronous responses or faults to.  When empty, no validation
    /// is performed (legacy permissive mode).  When non-empty, an
    /// extracted @c ReplyTo / @c FaultTo / @c From address must either
    /// equal one of these strings or be the WS-Addressing
    /// @c anonymous URI (which means "respond on the request
    /// connection" and is always benign).
    std::vector<std::string> address_allowlist;
  };

} // namespace xb::wsa
