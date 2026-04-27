#pragma once

/// @file
/// Shared libcurl hardening helpers used by both @c http_transport and
/// @c mtom_transport.  Centralising the calls means a defence added in
/// one place automatically applies to every libcurl-backed transport.

#include <xb/http_transport.hpp>
#include <xb/wsdl_transport.hpp>

#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace xb::service::detail {

  /// Test whether an IPv4 address (host byte order) lies in any of the
  /// ranges xb refuses to connect to by default: loopback (127/8),
  /// RFC 1918 private (10/8, 172.16/12, 192.168/16), link-local
  /// (169.254/16), CGNAT (100.64/10), broadcast (255.255.255.255), or
  /// the AWS / Azure / GCP cloud-metadata addresses.
  inline bool
  is_blocked_ipv4(std::uint32_t addr) {
    auto in_range = [&](std::uint32_t base, int prefix) {
      std::uint32_t mask =
          prefix == 0 ? 0 : (~std::uint32_t{0}) << (32 - prefix);
      return (addr & mask) == (base & mask);
    };
    if (in_range(0x7F000000, 8)) return true;  // 127.0.0.0/8
    if (in_range(0x0A000000, 8)) return true;  // 10.0.0.0/8
    if (in_range(0xAC100000, 12)) return true; // 172.16.0.0/12
    if (in_range(0xC0A80000, 16)) return true; // 192.168.0.0/16
    if (in_range(0xA9FE0000, 16)) return true; // 169.254.0.0/16
    if (in_range(0x64400000, 10)) return true; // 100.64.0.0/10
    if (addr == 0xFFFFFFFFu) return true;      // 255.255.255.255
    if (addr == 0xA8FE7E10u) return true;      // 168.63.129.16 (Azure)
    return false;
  }

  /// Test whether an IPv6 address lies in any blocked range: loopback
  /// (::1/128), unspecified (::/128), link-local (fe80::/10),
  /// unique-local (fc00::/7), or IPv4-mapped where the embedded IPv4
  /// is itself blocked.
  inline bool
  is_blocked_ipv6(const std::uint8_t addr[16]) {
    static const std::uint8_t loopback[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 0, 0, 1};
    static const std::uint8_t unspec[16] = {0};
    if (std::memcmp(addr, loopback, 16) == 0) return true;
    if (std::memcmp(addr, unspec, 16) == 0) return true;
    if (addr[0] == 0xFE && (addr[1] & 0xC0) == 0x80) return true; // fe80::/10
    if ((addr[0] & 0xFE) == 0xFC) return true;                    // fc00::/7
    // IPv4-mapped: ::ffff:a.b.c.d
    static const std::uint8_t v4mapped_prefix[12] = {0, 0, 0, 0, 0,    0,
                                                     0, 0, 0, 0, 0xFF, 0xFF};
    if (std::memcmp(addr, v4mapped_prefix, 12) == 0) {
      std::uint32_t v4 =
          (std::uint32_t(addr[12]) << 24) | (std::uint32_t(addr[13]) << 16) |
          (std::uint32_t(addr[14]) << 8) | std::uint32_t(addr[15]);
      return is_blocked_ipv4(v4);
    }
    return false;
  }

  /// libcurl @c CURLOPT_PREREQFUNCTION callback (libcurl ≥ 7.80). Fires
  /// after DNS resolution and before the TCP connect, including after
  /// each redirect, so it sees the true destination IP regardless of
  /// any DNS-rebinding tricks.
  inline int
  prereq_block_private(void* /*clientp*/, char* conn_primary_ip,
                       char* /*conn_local_ip*/, int /*conn_primary_port*/,
                       int /*conn_local_port*/) {
    if (conn_primary_ip == nullptr) return 0;
    in6_addr addr6;
    in_addr addr4;
    if (::inet_pton(AF_INET, conn_primary_ip, &addr4) == 1) {
      std::uint32_t host = ntohl(addr4.s_addr);
      if (is_blocked_ipv4(host)) return 1; // CURL_PREREQFUNC_ABORT
    } else if (::inet_pton(AF_INET6, conn_primary_ip, &addr6) == 1) {
      if (is_blocked_ipv6(addr6.s6_addr)) return 1;
    }
    return 0; // CURL_PREREQFUNC_OK
  }

  /// Sink used as @c CURLOPT_WRITEDATA. @ref write_callback returns
  /// short of the requested length once @ref max_bytes would be
  /// exceeded, causing libcurl to abort the transfer.
  struct write_sink {
    std::string body;
    std::size_t max_bytes = 0;
  };

  inline size_t
  write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* sink = static_cast<write_sink*>(userdata);
    const std::size_t incoming = size * nmemb;
    if (sink->body.size() + incoming > sink->max_bytes) { return 0; }
    sink->body.append(ptr, incoming);
    return incoming;
  }

  inline bool
  file_is_readable(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode) && ::access(path.c_str(), R_OK) == 0;
  }

  /// Apply the security-relevant subset of @p opts to @p curl: TLS
  /// hostname verification, bounded redirects, HTTPS-only redirect
  /// protocols, response size cap, and CA bundle validation.  The
  /// caller is responsible for freeing @p headers if this function
  /// throws.  Throws @c transport_error on misconfiguration (e.g.
  /// unreadable CA bundle).
  inline void
  apply_security_options(CURL* curl, const http_options& opts) {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                     opts.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, opts.max_redirects);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, opts.verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, opts.verify_peer ? 2L : 0L);

    if (!opts.ca_bundle.empty()) {
      if (!file_is_readable(opts.ca_bundle)) {
        throw transport_error("ca_bundle is not a readable file: " +
                              opts.ca_bundle);
      }
      curl_easy_setopt(curl, CURLOPT_CAINFO, opts.ca_bundle.c_str());
    }
    if (!opts.client_cert.empty()) {
      curl_easy_setopt(curl, CURLOPT_SSLCERT, opts.client_cert.c_str());
    }
    if (!opts.client_key.empty()) {
      curl_easy_setopt(curl, CURLOPT_SSLKEY, opts.client_key.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                     static_cast<curl_off_t>(opts.max_response_bytes));

    if (opts.block_private_destinations) {
      curl_easy_setopt(curl, CURLOPT_PREREQFUNCTION, prereq_block_private);
    }
  }

} // namespace xb::service::detail
