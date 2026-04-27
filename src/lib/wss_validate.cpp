#include <xb/wss_validate.hpp>

#ifdef XB_HAS_OPENSSL
#include <xb/wss_crypto.hpp>
#endif

#include <charconv>
#include <optional>
#include <string_view>

namespace xb::wss {

  namespace {

    /// Parse a small unsigned integer from @p s starting at @p pos.
    /// Advances @p pos past the digits.  Returns @c std::nullopt on
    /// missing/invalid digits.
    std::optional<int>
    parse_uint_field(std::string_view s, std::size_t& pos,
                     std::size_t expected_digits) {
      if (pos + expected_digits > s.size()) return std::nullopt;
      int value = 0;
      auto begin = s.data() + pos;
      auto end = begin + expected_digits;
      auto [ptr, ec] = std::from_chars(begin, end, value);
      if (ec != std::errc{} || ptr != end) return std::nullopt;
      pos += expected_digits;
      return value;
    }

    /// Parse an xsd:dateTime string suitable for WS-Security timestamps.
    /// Accepts:
    ///   YYYY-MM-DDTHH:MM:SS[.fraction]Z
    ///   YYYY-MM-DDTHH:MM:SS[.fraction]±HH:MM
    /// Returns @c std::nullopt on any deviation. Truncates fractional
    /// seconds (xb's internal clock is seconds-precision).
    std::optional<std::chrono::sys_seconds>
    parse_iso8601(std::string_view s) {
      using namespace std::chrono;
      std::size_t pos = 0;
      auto y = parse_uint_field(s, pos, 4);
      if (!y || pos >= s.size() || s[pos++] != '-') return std::nullopt;
      auto mo = parse_uint_field(s, pos, 2);
      if (!mo || pos >= s.size() || s[pos++] != '-') return std::nullopt;
      auto d = parse_uint_field(s, pos, 2);
      if (!d || pos >= s.size() || s[pos++] != 'T') return std::nullopt;
      auto h = parse_uint_field(s, pos, 2);
      if (!h || pos >= s.size() || s[pos++] != ':') return std::nullopt;
      auto mi = parse_uint_field(s, pos, 2);
      if (!mi || pos >= s.size() || s[pos++] != ':') return std::nullopt;
      auto sec = parse_uint_field(s, pos, 2);
      if (!sec) return std::nullopt;

      // Optional fractional seconds: ".ddd..." — skip but require digits.
      if (pos < s.size() && s[pos] == '.') {
        ++pos;
        std::size_t frac_start = pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
          ++pos;
        if (pos == frac_start) return std::nullopt;
      }

      // Timezone offset: 'Z' or ±HH:MM.
      int tz_offset_minutes = 0;
      if (pos >= s.size()) return std::nullopt;
      if (s[pos] == 'Z') {
        ++pos;
      } else if (s[pos] == '+' || s[pos] == '-') {
        const int sign = (s[pos] == '+') ? +1 : -1;
        ++pos;
        auto tz_h = parse_uint_field(s, pos, 2);
        if (!tz_h || pos >= s.size() || s[pos++] != ':') return std::nullopt;
        auto tz_m = parse_uint_field(s, pos, 2);
        if (!tz_m) return std::nullopt;
        tz_offset_minutes = sign * (*tz_h * 60 + *tz_m);
      } else {
        return std::nullopt;
      }
      if (pos != s.size()) return std::nullopt;

      year_month_day ymd{std::chrono::year{*y},
                         std::chrono::month{static_cast<unsigned>(*mo)},
                         std::chrono::day{static_cast<unsigned>(*d)}};
      if (!ymd.ok()) return std::nullopt;
      if (*h > 24 || *mi > 59 || *sec > 60) return std::nullopt;

      sys_days days_val = sys_days{ymd};
      sys_seconds tp = days_val + hours{*h} + minutes{*mi} + seconds{*sec};
      // Subtract the offset so the result is in UTC.
      tp -= minutes{tz_offset_minutes};
      return tp;
    }

  } // namespace

  validation_status
  validate_timestamp(const timestamp& ts, validate_timestamp_options opts,
                     std::chrono::sys_seconds now) {
    auto created = parse_iso8601(ts.created);
    if (!created) return validation_status::malformed;

    if (*created > now + opts.max_clock_skew) {
      return validation_status::future;
    }

    if (ts.expires) {
      auto expires = parse_iso8601(*ts.expires);
      if (!expires) return validation_status::malformed;
      if (*expires < now - opts.max_clock_skew) {
        return validation_status::expired;
      }
    } else {
      // No Expires — bound Created against the skew window so a stale
      // token cannot replay forever.
      if (*created < now - opts.max_clock_skew) {
        return validation_status::expired;
      }
    }

    return validation_status::ok;
  }

#ifdef XB_HAS_OPENSSL

  namespace {

    /// Translate a parse-failure outcome into the username-token
    /// authentication enum.  Created plays the same role as the
    /// timestamp's Created here.
    authentication_status
    check_created_window(const std::string& created,
                         validate_timestamp_options opts,
                         std::chrono::sys_seconds now) {
      // Reuse the timestamp validator: a UsernameToken with @c created
      // and no @c expires is essentially a one-shot timestamp.
      timestamp ts;
      ts.created = created;
      switch (validate_timestamp(ts, opts, now)) {
        case validation_status::ok:
          return authentication_status::ok;
        case validation_status::expired:
          return authentication_status::expired;
        case validation_status::future:
          return authentication_status::future;
        case validation_status::malformed:
          return authentication_status::malformed;
      }
      return authentication_status::malformed;
    }

  } // namespace

  authentication_status
  verify_username_token(const username_token& ut,
                        const password_lookup_fn& lookup,
                        validate_timestamp_options opts,
                        std::chrono::sys_seconds now) {
    // Digest mode is the only supported path.  Plaintext callers
    // perform their own comparison.
    if (ut.password_type != password_digest_type) {
      return authentication_status::missing_credentials;
    }
    if (!ut.nonce || !ut.created || ut.password.empty()) {
      return authentication_status::missing_credentials;
    }

    auto window = check_created_window(*ut.created, opts, now);
    if (window != authentication_status::ok) return window;

    auto password = lookup(ut.username);
    if (!password) return authentication_status::unknown_user;

    auto expected = crypto::compute_password_digest(
        *ut.nonce, *ut.created, *password, ut.digest_algorithm);
    if (!crypto::constant_time_equal(expected, ut.password)) {
      return authentication_status::wrong_password;
    }
    return authentication_status::ok;
  }

#endif // XB_HAS_OPENSSL

} // namespace xb::wss
