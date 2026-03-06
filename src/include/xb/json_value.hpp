#pragma once

#if __has_include(<nlohmann/json.hpp>)

#include <xb/any_attribute.hpp>
#include <xb/any_element.hpp>
#include <xb/date.hpp>
#include <xb/date_time.hpp>
#include <xb/day_time_duration.hpp>
#include <xb/decimal.hpp>
#include <xb/duration.hpp>
#include <xb/integer.hpp>
#include <xb/qname.hpp>
#include <xb/time.hpp>
#include <xb/xml_value.hpp>
#include <xb/year_month_duration.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace xb {

  // --- xb::integer ---
  // Small values: JSON integer. Large values: JSON string.
  inline void
  to_json(nlohmann::json& j, const integer& v) {
    auto s = v.to_string();
    try {
      auto i64 = static_cast<int64_t>(v);
      if (integer(i64) == v) {
        j = i64;
        return;
      }
    } catch (...) {}
    j = s;
  }

  inline void
  from_json(const nlohmann::json& j, integer& v) {
    if (j.is_number_integer()) {
      v = integer(j.get<int64_t>());
    } else {
      v = integer(std::string_view{j.get_ref<const std::string&>()});
    }
  }

  // --- xb::decimal ---
  // Always JSON string to preserve exact precision.
  inline void
  to_json(nlohmann::json& j, const decimal& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, decimal& v) {
    v = decimal(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::date ---
  inline void
  to_json(nlohmann::json& j, const date& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, date& v) {
    v = date(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::time ---
  inline void
  to_json(nlohmann::json& j, const time& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, time& v) {
    v = time(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::date_time ---
  inline void
  to_json(nlohmann::json& j, const date_time& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, date_time& v) {
    v = date_time(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::duration ---
  inline void
  to_json(nlohmann::json& j, const duration& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, duration& v) {
    v = duration(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::year_month_duration ---
  inline void
  to_json(nlohmann::json& j, const year_month_duration& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, year_month_duration& v) {
    v = year_month_duration(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::day_time_duration ---
  inline void
  to_json(nlohmann::json& j, const day_time_duration& v) {
    j = v.to_string();
  }

  inline void
  from_json(const nlohmann::json& j, day_time_duration& v) {
    v = day_time_duration(std::string_view{j.get_ref<const std::string&>()});
  }

  // --- xb::qname ---
  inline void
  to_json(nlohmann::json& j, const qname& v) {
    j = nlohmann::json{{"namespace", v.namespace_uri()},
                       {"local", v.local_name()}};
  }

  inline void
  from_json(const nlohmann::json& j, qname& v) {
    v = qname(j.at("namespace").get<std::string>(),
              j.at("local").get<std::string>());
  }

  // --- xb::any_attribute ---
  inline void
  to_json(nlohmann::json& j, const any_attribute& v) {
    j = nlohmann::json{{"name", v.name()}, {"value", v.value()}};
  }

  inline void
  from_json(const nlohmann::json& j, any_attribute& v) {
    qname name = j.at("name").get<qname>();
    v = any_attribute(std::move(name), j.at("value").get<std::string>());
  }

  // --- std::vector<std::byte> (binary data as base64) ---
  inline void
  to_json(nlohmann::json& j, const std::vector<std::byte>& v) {
    j = format_base64_binary(v);
  }

  inline void
  from_json(const nlohmann::json& j, std::vector<std::byte>& v) {
    v = parse_base64_binary(j.get_ref<const std::string&>());
  }

  // --- xb::any_element ---
  // Recursive: {tag, attributes, content} where content is an array
  // of strings (text nodes) and objects (child elements).
  inline void
  to_json(nlohmann::json& j, const any_element& v) {
    j["tag"] = v.name();
    j["attributes"] = v.attributes();

    nlohmann::json content = nlohmann::json::array();
    for (const auto& child : v.children()) {
      std::visit(
          [&content](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, std::string>) {
              content.push_back(c);
            } else {
              nlohmann::json child_j;
              to_json(child_j, c);
              content.push_back(std::move(child_j));
            }
          },
          child);
    }
    j["content"] = std::move(content);
  }

  inline void
  from_json(const nlohmann::json& j, any_element& v) {
    qname tag = j.at("tag").get<qname>();
    std::vector<any_attribute> attributes =
        j.at("attributes").get<std::vector<any_attribute>>();

    std::vector<any_element::child> children;
    for (const auto& item : j.at("content")) {
      if (item.is_string()) {
        children.emplace_back(item.get<std::string>());
      } else {
        any_element child_elem;
        from_json(item, child_elem);
        children.emplace_back(std::move(child_elem));
      }
    }

    v = any_element(std::move(tag), std::move(attributes), std::move(children));
  }

} // namespace xb

#endif // __has_include(<nlohmann/json.hpp>)
