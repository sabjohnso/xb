#include <xb/http_listener.hpp>

#include <xb/any_element.hpp>
#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/soap_model.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xb::service {

  namespace {

    std::string
    serialize_envelope(const soap::envelope& env) {
      std::ostringstream os;
      ostream_writer writer(os);
      soap::write_envelope(writer, env);
      return os.str();
    }

    soap::envelope
    parse_envelope(const std::string& xml) {
      expat_reader reader(xml);
      reader.read();
      return soap::read_envelope(reader);
    }

    // Build a SOAP fault envelope from an exception message.
    std::string
    make_fault_response(soap::soap_version version,
                        const std::string& message) {
      // Build fault as any_element tree to avoid the xml:lang
      // re-declaration issue with write_fault + any_element round-trip.
      const std::string soap_ns =
          (version == soap::soap_version::v1_2)
              ? "http://www.w3.org/2003/05/soap-envelope"
              : "http://schemas.xmlsoap.org/soap/envelope/";

      using child = any_element::child;
      using elem = any_element;

      soap::envelope env;
      env.version = version;

      if (version == soap::soap_version::v1_2) {
        auto value = elem(qname{soap_ns, "Value"}, {},
                          {child{std::string("soap:Receiver")}});
        auto code = elem(qname{soap_ns, "Code"}, {}, {child{value}});
        auto text = elem(qname{soap_ns, "Text"}, {}, {child{message}});
        auto reason = elem(qname{soap_ns, "Reason"}, {}, {child{text}});
        env.body.push_back(
            elem(qname{soap_ns, "Fault"}, {}, {child{code}, child{reason}}));
      } else {
        auto faultcode = elem(qname{"", "faultcode"}, {},
                              {child{std::string("soap:Server")}});
        auto faultstring = elem(qname{"", "faultstring"}, {}, {child{message}});
        env.body.push_back(elem(qname{soap_ns, "Fault"}, {},
                                {child{faultcode}, child{faultstring}}));
      }

      return serialize_envelope(env);
    }

    std::string
    soap_content_type(soap::soap_version version) {
      return (version == soap::soap_version::v1_2)
                 ? "application/soap+xml; charset=utf-8"
                 : "text/xml; charset=utf-8";
    }

    // Extract the SOAPAction from a Content-Type action parameter.
    // Content-Type: application/soap+xml; charset=utf-8; action="urn:test"
    std::string
    extract_action_from_content_type(const std::string& content_type) {
      auto pos = content_type.find("action=");
      if (pos == std::string::npos) return {};
      pos += 7; // skip "action="
      if (pos < content_type.size() && content_type[pos] == '"') {
        ++pos;
        auto end = content_type.find('"', pos);
        if (end != std::string::npos)
          return content_type.substr(pos, end - pos);
      }
      // Unquoted
      auto end = content_type.find(';', pos);
      if (end == std::string::npos) end = content_type.size();
      auto result = content_type.substr(pos, end - pos);
      // Trim trailing whitespace
      while (!result.empty() && result.back() == ' ')
        result.pop_back();
      return result;
    }

    // Detect SOAP version from Content-Type.
    soap::soap_version
    detect_soap_version(const std::string& content_type) {
      if (content_type.find("application/soap+xml") != std::string::npos)
        return soap::soap_version::v1_2;
      return soap::soap_version::v1_1;
    }

    // Simple HTTP header parser.
    struct http_request {
      std::string method;
      std::string path;
      std::string content_type;
      std::string soap_action; // from SOAPAction header
      std::string body;
    };

    /// Outcome of @ref read_http_request. Distinguishes "ok" from each
    /// rejection reason so the caller can map to the correct HTTP status.
    enum class read_status {
      ok,
      malformed,         // 400
      payload_too_large, // 413
      not_implemented,   // 501 (e.g. Transfer-Encoding)
    };

    /// Read and parse an HTTP request from @p fd into @p req. Honours the
    /// @p max_body_bytes cap. Returns a status indicating whether the
    /// request is well-formed and whether the body fits.
    read_status
    read_http_request(int fd, std::size_t max_body_bytes, http_request& req) {
      // Read until we have the header/body separator.
      std::string raw;
      char buf[4096];
      std::string::size_type header_end = std::string::npos;

      while (header_end == std::string::npos) {
        auto n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return read_status::malformed;
        raw.append(buf, static_cast<std::size_t>(n));
        header_end = raw.find("\r\n\r\n");
        // Cap header section so a malicious peer cannot make us
        // accumulate gigabytes before the separator arrives.
        if (raw.size() > 64UL * 1024UL && header_end == std::string::npos) {
          return read_status::malformed;
        }
      }

      // Parse request line
      auto first_line_end = raw.find("\r\n");
      if (first_line_end == std::string::npos) return read_status::malformed;
      auto line = raw.substr(0, first_line_end);
      auto sp1 = line.find(' ');
      auto sp2 = line.find(' ', sp1 + 1);
      if (sp1 == std::string::npos || sp2 == std::string::npos)
        return read_status::malformed;
      req.method = line.substr(0, sp1);
      req.path = line.substr(sp1 + 1, sp2 - sp1 - 1);

      // Parse headers
      std::size_t content_length = 0;
      bool has_content_length = false;
      bool has_transfer_encoding = false;
      auto headers_str =
          raw.substr(first_line_end + 2, header_end - first_line_end - 2);
      std::istringstream hdr_stream(headers_str);
      std::string hdr_line;
      while (std::getline(hdr_stream, hdr_line)) {
        if (!hdr_line.empty() && hdr_line.back() == '\r') hdr_line.pop_back();
        if (hdr_line.empty()) break;
        auto colon = hdr_line.find(':');
        if (colon == std::string::npos) continue;
        auto name = hdr_line.substr(0, colon);
        auto value = hdr_line.substr(colon + 1);
        // Trim leading whitespace from value
        while (!value.empty() && value.front() == ' ')
          value.erase(0, 1);

        // Case-insensitive header comparison
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_name == "content-length") {
          try {
            std::size_t consumed = 0;
            content_length = std::stoull(value, &consumed);
            // Reject any trailing garbage after the number.
            while (consumed < value.size() && value[consumed] == ' ')
              ++consumed;
            if (consumed != value.size()) return read_status::malformed;
          } catch (const std::exception&) { return read_status::malformed; }
          has_content_length = true;
        } else if (lower_name == "transfer-encoding") {
          has_transfer_encoding = true;
        } else if (lower_name == "content-type") {
          req.content_type = value;
        } else if (lower_name == "soapaction") {
          // Strip quotes
          req.soap_action = value;
          if (req.soap_action.size() >= 2 && req.soap_action.front() == '"' &&
              req.soap_action.back() == '"') {
            req.soap_action =
                req.soap_action.substr(1, req.soap_action.size() - 2);
          }
        }
      }

      // RFC 7230 §3.3.1: a request that includes Transfer-Encoding is
      // not supported here (chunked decoding is unimplemented), and a
      // request with both is a smuggling shape. Either way: refuse.
      if (has_transfer_encoding) return read_status::not_implemented;

      if (has_content_length && content_length > max_body_bytes) {
        return read_status::payload_too_large;
      }

      // Read body
      auto body_start = header_end + 4;
      req.body = raw.substr(body_start);
      // Defensive: even if the already-buffered prefix exceeded the cap,
      // refuse rather than truncate.
      if (req.body.size() > max_body_bytes) {
        return read_status::payload_too_large;
      }
      while (req.body.size() < content_length) {
        auto n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.body.append(buf, static_cast<std::size_t>(n));
        if (req.body.size() > max_body_bytes) {
          return read_status::payload_too_large;
        }
      }

      return read_status::ok;
    }

    void
    send_http_response(int fd, int status_code, const std::string& status_text,
                       const std::string& content_type,
                       const std::string& body) {
      std::ostringstream resp;
      resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
           << "Content-Type: " << content_type << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << body;
      auto data = resp.str();
      auto remaining = data.size();
      auto ptr = data.data();
      while (remaining > 0) {
        auto n = ::send(fd, ptr, remaining, MSG_NOSIGNAL);
        if (n <= 0) break;
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
      }
    }

  } // namespace

  // -- impl -------------------------------------------------------------------

  struct http_listener::impl {
    http_listener_options opts;
    int server_fd = -1;
    int stop_pipe[2] = {-1, -1};
    std::atomic<bool> running{false};

    explicit impl(http_listener_options o) : opts(std::move(o)) {
      // Create the stop pipe
      if (::pipe(stop_pipe) < 0)
        throw std::runtime_error("http_listener: pipe() failed");

      // Create server socket
      server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (server_fd < 0) {
        ::close(stop_pipe[0]);
        ::close(stop_pipe[1]);
        throw std::runtime_error("http_listener: socket() failed");
      }

      int opt_val = 1;
      ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val,
                   sizeof(opt_val));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(opts.port);
      if (inet_pton(AF_INET, opts.bind_address.c_str(), &addr.sin_addr) <= 0) {
        cleanup();
        throw std::runtime_error("http_listener: invalid bind address: " +
                                 opts.bind_address);
      }

      if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
          0) {
        cleanup();
        throw std::runtime_error(std::string("http_listener: bind() failed: ") +
                                 std::strerror(errno));
      }

      if (::listen(server_fd, opts.backlog) < 0) {
        cleanup();
        throw std::runtime_error("http_listener: listen() failed");
      }

      // Read back the assigned port (for port=0 case)
      sockaddr_in bound{};
      socklen_t len = sizeof(bound);
      if (::getsockname(server_fd, reinterpret_cast<sockaddr*>(&bound), &len) ==
          0) {
        opts.port = ntohs(bound.sin_port);
      }
    }

    ~impl() { cleanup(); }

    void
    cleanup() {
      if (server_fd >= 0) {
        ::close(server_fd);
        server_fd = -1;
      }
      if (stop_pipe[0] >= 0) {
        ::close(stop_pipe[0]);
        stop_pipe[0] = -1;
      }
      if (stop_pipe[1] >= 0) {
        ::close(stop_pipe[1]);
        stop_pipe[1] = -1;
      }
    }

    void
    serve(soap_handler handler) {
      running = true;

      pollfd fds[2];
      fds[0].fd = server_fd;
      fds[0].events = POLLIN;
      fds[1].fd = stop_pipe[0];
      fds[1].events = POLLIN;

      while (running) {
        int rc = ::poll(fds, 2, -1);
        if (rc < 0) {
          if (errno == EINTR) continue;
          break;
        }

        // Check stop signal
        if (fds[1].revents & POLLIN) break;

        // Accept a connection
        if (fds[0].revents & POLLIN) {
          int client_fd = ::accept(server_fd, nullptr, nullptr);
          if (client_fd < 0) continue;
          handle_connection(client_fd, handler);
          ::close(client_fd);
        }
      }

      running = false;
    }

    void
    handle_connection(int client_fd, const soap_handler& handler) {
      http_request req;
      switch (read_http_request(client_fd, opts.max_request_bytes, req)) {
        case read_status::ok:
          break;
        case read_status::payload_too_large:
          send_http_response(client_fd, 413, "Payload Too Large", "text/plain",
                             "Request body exceeds the configured limit");
          return;
        case read_status::not_implemented:
          send_http_response(
              client_fd, 501, "Not Implemented", "text/plain",
              "Transfer-Encoding requests are not supported by this listener");
          return;
        case read_status::malformed:
          send_http_response(client_fd, 400, "Bad Request", "text/plain",
                             "Malformed HTTP request");
          return;
      }

      if (req.method != "POST") {
        send_http_response(client_fd, 405, "Method Not Allowed", "text/plain",
                           "Only POST is supported");
        return;
      }

      // Determine SOAP version and action
      auto version = detect_soap_version(req.content_type);
      std::string action = req.soap_action;
      if (action.empty())
        action = extract_action_from_content_type(req.content_type);

      try {
        auto request_env = parse_envelope(req.body);
        // Ensure the parsed envelope has the detected version
        request_env.version = version;

        auto response_env = handler(action, request_env);

        auto response_xml = serialize_envelope(response_env);
        send_http_response(client_fd, 200, "OK",
                           soap_content_type(response_env.version),
                           response_xml);
      } catch (const std::exception& e) {
        auto fault_xml = make_fault_response(version, e.what());
        send_http_response(client_fd, 500, "Internal Server Error",
                           soap_content_type(version), fault_xml);
      }
    }

    void
    signal_stop() {
      running = false;
      char c = 0;
      auto _ = ::write(stop_pipe[1], &c, 1);
      (void)_;
    }
  };

  // -- http_listener public interface -----------------------------------------

  http_listener::http_listener(http_listener_options opts)
      : impl_(std::make_unique<impl>(std::move(opts))) {}

  http_listener::~http_listener() = default;
  http_listener::http_listener(http_listener&&) noexcept = default;
  http_listener&
  http_listener::operator=(http_listener&&) noexcept = default;

  void
  http_listener::serve(soap_handler handler) {
    impl_->serve(std::move(handler));
  }

  void
  http_listener::stop() {
    impl_->signal_stop();
  }

  std::uint16_t
  http_listener::listening_port() const {
    return impl_->opts.port;
  }

} // namespace xb::service
