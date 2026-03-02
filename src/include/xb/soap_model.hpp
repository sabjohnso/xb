#pragma once

#include <xb/any_element.hpp>

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace xb::soap {

  enum class soap_version { v1_1, v1_2 };

  struct header_block {
    any_element content;
    bool must_understand = false;
    std::string role; // 1.2 role / 1.1 actor

    bool
    operator==(const header_block&) const = default;
  };

  struct envelope {
    soap_version version = soap_version::v1_1;
    std::vector<header_block> headers;
    std::vector<any_element> body;

    bool
    operator==(const envelope&) const = default;
  };

  // --- SOAP 1.1 Fault ---

  struct fault_1_1 {
    std::string fault_code;
    std::string fault_string;
    std::string fault_actor;
    std::optional<any_element> detail;

    bool
    operator==(const fault_1_1&) const = default;
  };

  // --- SOAP 1.2 Fault ---

  struct fault_subcode {
    std::string value;
    std::unique_ptr<fault_subcode> subcode;

    fault_subcode() = default;
    fault_subcode(const fault_subcode&);
    fault_subcode(fault_subcode&&) = default;

    fault_subcode&
    operator=(const fault_subcode&);
    fault_subcode&
    operator=(fault_subcode&&) = default;

    ~fault_subcode() = default;

    bool
    operator==(const fault_subcode&) const;
  };

  struct fault_code {
    std::string value;
    std::optional<fault_subcode> subcode;

    bool
    operator==(const fault_code&) const = default;
  };

  struct fault_reason_text {
    std::string lang;
    std::string text;

    bool
    operator==(const fault_reason_text&) const = default;
  };

  struct fault_1_2 {
    fault_code code;
    std::vector<fault_reason_text> reason;
    std::string node;
    std::string role;
    std::optional<any_element> detail;

    bool
    operator==(const fault_1_2&) const = default;
  };

  using fault = std::variant<fault_1_1, fault_1_2>;

  // --- Out-of-line definitions for fault_subcode ---

  inline fault_subcode::fault_subcode(const fault_subcode& other)
      : value(other.value),
        subcode(other.subcode ? std::make_unique<fault_subcode>(*other.subcode)
                              : nullptr) {}

  inline fault_subcode&
  fault_subcode::operator=(const fault_subcode& other) {
    if (this != &other) {
      value = other.value;
      subcode = other.subcode ? std::make_unique<fault_subcode>(*other.subcode)
                              : nullptr;
    }
    return *this;
  }

  inline bool
  fault_subcode::operator==(const fault_subcode& other) const {
    if (value != other.value) return false;
    if (static_cast<bool>(subcode) != static_cast<bool>(other.subcode))
      return false;
    if (subcode && *subcode != *other.subcode) return false;
    return true;
  }

} // namespace xb::soap
