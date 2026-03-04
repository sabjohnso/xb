#include <xb/wss_headers.hpp>

#include <xb/any_element.hpp>

namespace xb::wss {

  namespace {

    std::string
    text_content(const any_element& elem) {
      std::string result;
      for (const auto& child : elem.children()) {
        if (auto* text = std::get_if<std::string>(&child)) { result += *text; }
      }
      return result;
    }

    any_element
    make_text_element(const std::string& ns, const std::string& local_name,
                      const std::string& text) {
      return any_element(qname(ns, local_name), {}, {std::string(text)});
    }

    any_element
    make_text_element(const std::string& ns, const std::string& local_name,
                      std::vector<any_attribute> attrs,
                      const std::string& text) {
      return any_element(qname(ns, local_name), std::move(attrs),
                         {std::string(text)});
    }

    any_element
    build_username_token_element(const username_token& ut) {
      std::vector<any_element::child> children;

      children.push_back(make_text_element(wsse_ns, "Username", ut.username));

      std::vector<any_attribute> pw_attrs;
      if (!ut.password_type.empty()) {
        pw_attrs.emplace_back(qname("", "Type"), ut.password_type);
      }
      children.push_back(make_text_element(wsse_ns, "Password",
                                           std::move(pw_attrs), ut.password));

      if (ut.nonce) {
        std::vector<any_attribute> nonce_attrs;
        nonce_attrs.emplace_back(qname("", "EncodingType"),
                                 std::string(base64_encoding_type));
        children.push_back(make_text_element(
            wsse_ns, "Nonce", std::move(nonce_attrs), *ut.nonce));
      }

      if (ut.created) {
        children.push_back(make_text_element(wsu_ns, "Created", *ut.created));
      }

      return any_element(qname(wsse_ns, "UsernameToken"), {},
                         std::move(children));
    }

    any_element
    build_timestamp_element(const timestamp& ts) {
      std::vector<any_element::child> children;

      children.push_back(make_text_element(wsu_ns, "Created", ts.created));

      if (ts.expires) {
        children.push_back(make_text_element(wsu_ns, "Expires", *ts.expires));
      }

      return any_element(qname(wsu_ns, "Timestamp"), {}, std::move(children));
    }

    any_element
    build_binary_security_token_element(const binary_security_token& bst) {
      std::vector<any_attribute> attrs;
      attrs.emplace_back(qname("", "ValueType"), bst.value_type);
      attrs.emplace_back(qname("", "EncodingType"), bst.encoding_type);
      if (bst.wsu_id) { attrs.emplace_back(qname(wsu_ns, "Id"), *bst.wsu_id); }

      return any_element(qname(wsse_ns, "BinarySecurityToken"),
                         std::move(attrs), {std::string(bst.value)});
    }

    username_token
    extract_username_token(const any_element& elem) {
      username_token ut;

      for (const auto& child : elem.children()) {
        auto* el = std::get_if<any_element>(&child);
        if (!el) continue;

        const auto& name = el->name();
        const auto& local = name.local_name();

        if (name.namespace_uri() == wsse_ns && local == "Username") {
          ut.username = text_content(*el);
        } else if (name.namespace_uri() == wsse_ns && local == "Password") {
          ut.password = text_content(*el);
          for (const auto& attr : el->attributes()) {
            if (attr.name().local_name() == "Type") {
              ut.password_type = std::string(attr.value());
            }
          }
        } else if (name.namespace_uri() == wsse_ns && local == "Nonce") {
          ut.nonce = text_content(*el);
        } else if (name.namespace_uri() == wsu_ns && local == "Created") {
          ut.created = text_content(*el);
        }
      }

      return ut;
    }

    timestamp
    extract_timestamp(const any_element& elem) {
      timestamp ts;

      for (const auto& child : elem.children()) {
        auto* el = std::get_if<any_element>(&child);
        if (!el) continue;

        const auto& name = el->name();
        if (name.namespace_uri() != wsu_ns) continue;

        if (name.local_name() == "Created") {
          ts.created = text_content(*el);
        } else if (name.local_name() == "Expires") {
          ts.expires = text_content(*el);
        }
      }

      return ts;
    }

    binary_security_token
    extract_binary_security_token(const any_element& elem) {
      binary_security_token bst;
      bst.value = text_content(elem);

      for (const auto& attr : elem.attributes()) {
        const auto& local = attr.name().local_name();
        if (local == "ValueType") {
          bst.value_type = std::string(attr.value());
        } else if (local == "EncodingType") {
          bst.encoding_type = std::string(attr.value());
        } else if (attr.name().namespace_uri() == wsu_ns && local == "Id") {
          bst.wsu_id = std::string(attr.value());
        }
      }

      return bst;
    }

  } // namespace

  void
  add_security_header(soap::envelope& env, const security_header& h) {
    if (!h.username && !h.ts && !h.binary_token) return;

    std::vector<any_element::child> security_children;

    if (h.username) {
      security_children.push_back(build_username_token_element(*h.username));
    }
    if (h.ts) { security_children.push_back(build_timestamp_element(*h.ts)); }
    if (h.binary_token) {
      security_children.push_back(
          build_binary_security_token_element(*h.binary_token));
    }

    soap::header_block hb;
    hb.must_understand = true;
    hb.content = any_element(qname(wsse_ns, "Security"), {},
                             std::move(security_children));
    env.headers.push_back(std::move(hb));
  }

  security_header
  extract_security_header(const soap::envelope& env) {
    security_header result;

    for (const auto& hb : env.headers) {
      const auto& name = hb.content.name();
      if (name.namespace_uri() != wsse_ns || name.local_name() != "Security")
        continue;

      for (const auto& child : hb.content.children()) {
        auto* el = std::get_if<any_element>(&child);
        if (!el) continue;

        const auto& cname = el->name();
        const auto& local = cname.local_name();
        const auto& ns = cname.namespace_uri();

        if (ns == wsse_ns && local == "UsernameToken") {
          result.username = extract_username_token(*el);
        } else if (ns == wsu_ns && local == "Timestamp") {
          result.ts = extract_timestamp(*el);
        } else if (ns == wsse_ns && local == "BinarySecurityToken") {
          result.binary_token = extract_binary_security_token(*el);
        }
      }
    }

    return result;
  }

} // namespace xb::wss
