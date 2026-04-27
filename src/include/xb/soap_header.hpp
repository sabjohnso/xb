#pragma once

#include <xb/soap_model.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace xb::soap {

  // Handler function: returns true if the header was understood
  using header_handler = std::function<bool(const header_block&)>;

  // Exception carrying a SOAP fault for unhandled mustUnderstand headers
  struct soap_fault_exception : std::runtime_error {
    fault fault_value;

    explicit soap_fault_exception(fault f)
        : std::runtime_error("SOAP mustUnderstand header not handled"),
          fault_value(std::move(f)) {}
  };

  class header_pipeline {
    std::unordered_map<qname, header_handler> handlers_;
    /// The role this node plays in the SOAP interaction.  An empty
    /// string corresponds to the SOAP 1.2 "next" / SOAP 1.1 default
    /// role (every header without an explicit role is for us).
    std::string current_role_;

  public:
    // Register a handler for headers with a given element qname
    void
    add_handler(const qname& name, header_handler handler) {
      handlers_[name] = std::move(handler);
    }

    /// Configure which SOAP role / actor this pipeline plays.  Headers
    /// whose @c role attribute does not match the configured role (or
    /// the empty / "next" role when @p role is empty) are skipped per
    /// SOAP 1.2 §2.7 — including their @c mustUnderstand setting.
    void
    set_current_role(std::string role) {
      current_role_ = std::move(role);
    }

    // Process all headers in an envelope.
    // Throws soap_fault_exception if a mustUnderstand header is not handled.
    void
    process(const envelope& env) const {
      for (const auto& hb : env.headers) {
        if (!header_targets_us(hb)) continue;

        const auto& name = hb.content.name();
        bool understood = false;

        auto it = handlers_.find(name);
        if (it != handlers_.end()) { understood = it->second(hb); }

        if (hb.must_understand && !understood) {
          throw soap_fault_exception(make_must_understand_fault(env.version));
        }
      }
    }

  private:
    /// Decide whether a header block is targeted at this node.
    ///
    /// Headers without an explicit role (empty @c role) are addressed
    /// to the ultimate receiver, which by default this node assumes
    /// itself to be — always our responsibility unless explicitly
    /// configured otherwise.
    ///
    /// The SOAP 1.2 "next" role
    /// (=http://www.w3.org/2003/05/soap-envelope/role/next=) and the
    /// SOAP 1.1 "next" actor
    /// (=http://schemas.xmlsoap.org/soap/actor/next=) target every
    /// node in the message path — always our responsibility.
    ///
    /// The SOAP 1.2 "none" role tells every node to ignore the header
    /// — never our responsibility.
    ///
    /// Any other explicit role only matches this node when the caller
    /// has configured @c current_role_ accordingly via
    /// @ref set_current_role.
    bool
    header_targets_us(const header_block& hb) const {
      static constexpr auto soap12_next =
          "http://www.w3.org/2003/05/soap-envelope/role/next";
      static constexpr auto soap12_none =
          "http://www.w3.org/2003/05/soap-envelope/role/none";
      static constexpr auto soap12_ultimate_receiver =
          "http://www.w3.org/2003/05/soap-envelope/role/ultimateReceiver";
      static constexpr auto soap11_next =
          "http://schemas.xmlsoap.org/soap/actor/next";

      if (hb.role.empty()) return true;
      if (hb.role == soap12_next) return true;
      if (hb.role == soap11_next) return true;
      if (hb.role == soap12_none) return false;
      if (hb.role == soap12_ultimate_receiver) return current_role_.empty();
      return hb.role == current_role_;
    }

    static fault
    make_must_understand_fault(soap_version version) {
      if (version == soap_version::v1_1) {
        fault_1_1 f;
        f.fault_code = "soap:MustUnderstand";
        f.fault_string =
            "One or more mandatory SOAP header blocks not understood";
        return f;
      }
      fault_1_2 f;
      f.code.value = "soap:MustUnderstand";
      fault_reason_text rt;
      rt.lang = "en";
      rt.text = "One or more mandatory SOAP header blocks not understood";
      f.reason.push_back(std::move(rt));
      return f;
    }
  };

} // namespace xb::soap
