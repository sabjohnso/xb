#include <xb/mtom_transport.hpp>

#include <xb/expat_reader.hpp>
#include <xb/mime_multipart.hpp>
#include <xb/soap_envelope.hpp>
#include <xb/xop.hpp>

#include <curl/curl.h>

namespace xb::service {

  namespace {

    soap::envelope
    parse_envelope(const std::string& xml) {
      expat_reader reader(xml);
      reader.read();
      return soap::read_envelope(reader);
    }

    size_t
    write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
      auto* response_body = static_cast<std::string*>(userdata);
      response_body->append(ptr, size * nmemb);
      return size * nmemb;
    }

    bool
    is_multipart(const std::string& content_type) {
      return content_type.find("multipart/related") != std::string::npos;
    }

  } // namespace

  struct mtom_transport::impl {
    http_options http_opts;
    mtom_options mtom_opts;
    CURL* curl = nullptr;

    impl(http_options ho, mtom_options mo)
        : http_opts(std::move(ho)), mtom_opts(std::move(mo)) {
      curl = curl_easy_init();
      if (!curl) { throw transport_error("failed to initialize libcurl"); }
    }

    ~impl() {
      if (curl) { curl_easy_cleanup(curl); }
    }

    impl(const impl&) = delete;
    impl&
    operator=(const impl&) = delete;

    struct raw_response {
      int status_code = 0;
      std::string content_type;
      std::string body;
    };

    raw_response
    perform(const std::string& endpoint, const std::string& content_type,
            const std::string& soap_action,
            const std::vector<std::byte>& body) {
      curl_easy_reset(curl);

      curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
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
                       static_cast<long>(http_opts.connect_timeout.count()));
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                       static_cast<long>(http_opts.request_timeout.count()));
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                       http_opts.follow_redirects ? 1L : 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,
                       http_opts.verify_peer ? 1L : 0L);

      if (!http_opts.ca_bundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, http_opts.ca_bundle.c_str());
      }
      if (!http_opts.client_cert.empty()) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, http_opts.client_cert.c_str());
      }
      if (!http_opts.client_key.empty()) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, http_opts.client_key.c_str());
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

      raw_response resp;
      resp.status_code = static_cast<int>(status_code);
      resp.content_type = ct_ptr ? ct_ptr : "";
      resp.body = std::move(response_body);
      return resp;
    }
  };

  mtom_transport::mtom_transport(http_options http_opts, mtom_options mtom_opts)
      : impl_(std::make_unique<impl>(std::move(http_opts),
                                     std::move(mtom_opts))) {}

  mtom_transport::~mtom_transport() = default;
  mtom_transport::mtom_transport(mtom_transport&&) noexcept = default;
  mtom_transport&
  mtom_transport::operator=(mtom_transport&&) noexcept = default;

  soap::envelope
  mtom_transport::call(const std::string& endpoint,
                       const std::string& soap_action,
                       const soap::envelope& request) {
    // Optimize the request
    auto mtom_msg =
        xop::optimize(request, impl_->mtom_opts.optimization_threshold);

    // Convert to MIME multipart
    auto mp = xop::to_multipart(mtom_msg);
    auto body = mime::serialize_multipart(mp);

    // Build MTOM Content-Type
    auto content_type =
        mime::mtom_content_type(mp.boundary, "root@xb.generated");

    // Send the request
    auto resp = impl_->perform(endpoint, content_type, soap_action, body);

    if (resp.status_code < 200 || resp.status_code >= 300) {
      if (resp.status_code == 500 && !resp.body.empty()) {
        try {
          return parse_envelope(resp.body);
        } catch (...) {
          // Not valid SOAP -- fall through to throw
        }
      }
      throw transport_error("HTTP " + std::to_string(resp.status_code) + ": " +
                            resp.body);
    }

    if (resp.body.empty()) {
      soap::envelope empty;
      empty.version = request.version;
      return empty;
    }

    // Check if response is MTOM multipart
    if (is_multipart(resp.content_type)) {
      auto boundary = mime::extract_boundary(resp.content_type);
      if (!boundary.empty()) {
        std::vector<std::byte> resp_bytes;
        for (char c : resp.body) {
          resp_bytes.push_back(static_cast<std::byte>(c));
        }
        auto resp_mp = mime::parse_multipart(resp_bytes, boundary);
        auto resp_mtom = xop::from_multipart(resp_mp);
        return xop::deoptimize(resp_mtom);
      }
    }

    // Plain XML response
    return parse_envelope(resp.body);
  }

} // namespace xb::service
