#include <xb/http_transport.hpp>

#include <xb/expat_reader.hpp>
#include <xb/ostream_writer.hpp>
#include <xb/soap_envelope.hpp>

#include <curl/curl.h>

#include <sstream>

namespace xb::service {

  namespace {

    std::string
    soap_content_type(soap::soap_version version,
                      const std::string& soap_action) {
      if (version == soap::soap_version::v1_2) {
        std::string ct = "application/soap+xml; charset=utf-8";
        if (!soap_action.empty()) { ct += "; action=\"" + soap_action + "\""; }
        return ct;
      }
      return "text/xml; charset=utf-8";
    }

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
      reader.read(); // advance to first element
      return soap::read_envelope(reader);
    }

    size_t
    write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
      auto* response_body = static_cast<std::string*>(userdata);
      response_body->append(ptr, size * nmemb);
      return size * nmemb;
    }

  } // namespace

  struct http_transport::impl {
    http_options opts;
    CURL* curl = nullptr;

    explicit impl(http_options o) : opts(std::move(o)) {
      curl = curl_easy_init();
      if (!curl) { throw transport_error("failed to initialize libcurl"); }
    }

    ~impl() {
      if (curl) { curl_easy_cleanup(curl); }
    }

    impl(const impl&) = delete;
    impl&
    operator=(const impl&) = delete;

    http_response
    perform(const std::string& endpoint, const std::string& content_type,
            const std::string& soap_action, const std::string& body) {
      curl_easy_reset(curl);

      curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                       static_cast<long>(body.size()));

      struct curl_slist* headers = nullptr;
      std::string ct_header = "Content-Type: " + content_type;
      headers = curl_slist_append(headers, ct_header.c_str());
      if (!soap_action.empty()) {
        std::string sa_header = "SOAPAction: \"" + soap_action + "\"";
        headers = curl_slist_append(headers, sa_header.c_str());
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                       static_cast<long>(opts.connect_timeout.count()));
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                       static_cast<long>(opts.request_timeout.count()));
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                       opts.follow_redirects ? 1L : 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,
                       opts.verify_peer ? 1L : 0L);

      if (!opts.ca_bundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, opts.ca_bundle.c_str());
      }
      if (!opts.client_cert.empty()) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, opts.client_cert.c_str());
      }
      if (!opts.client_key.empty()) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, opts.client_key.c_str());
      }

      std::string response_body;
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

      CURLcode res = curl_easy_perform(curl);
      curl_slist_free_all(headers);

      if (res != CURLE_OK) {
        throw transport_error(std::string("HTTP request failed: ") +
                              curl_easy_strerror(res));
      }

      long status_code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

      char* ct_ptr = nullptr;
      curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct_ptr);

      http_response resp;
      resp.status_code = static_cast<int>(status_code);
      resp.content_type = ct_ptr ? ct_ptr : "";
      resp.body = std::move(response_body);
      return resp;
    }
  };

  http_transport::http_transport(http_options opts)
      : impl_(std::make_unique<impl>(std::move(opts))) {}

  http_transport::~http_transport() = default;

  http_transport::http_transport(http_transport&&) noexcept = default;

  http_transport&
  http_transport::operator=(http_transport&&) noexcept = default;

  soap::envelope
  http_transport::call(const std::string& endpoint,
                       const std::string& soap_action,
                       const soap::envelope& request) {
    auto body = serialize_envelope(request);
    auto content_type = soap_content_type(request.version, soap_action);

    // SOAP 1.1: SOAPAction header separate from Content-Type
    // SOAP 1.2: action parameter in Content-Type, no separate header
    std::string sa_header =
        (request.version == soap::soap_version::v1_1) ? soap_action : "";

    auto resp = impl_->perform(endpoint, content_type, sa_header, body);

    if (resp.status_code < 200 || resp.status_code >= 300) {
      // SOAP faults may come as 500 with a valid SOAP envelope
      if (resp.status_code == 500 && !resp.body.empty()) {
        try {
          return parse_envelope(resp.body);
        } catch (...) {
          // Not valid SOAP — fall through to throw
        }
      }
      throw transport_error("HTTP " + std::to_string(resp.status_code) + ": " +
                            resp.body);
    }

    if (resp.body.empty()) {
      // One-way operations may return empty body
      soap::envelope empty;
      empty.version = request.version;
      return empty;
    }

    return parse_envelope(resp.body);
  }

} // namespace xb::service
