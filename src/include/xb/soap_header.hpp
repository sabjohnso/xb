#pragma once

#include <xb/soap_model.hpp>

#include <functional>
#include <stdexcept>
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

  public:
    // Register a handler for headers with a given element qname
    void
    add_handler(const qname& name, header_handler handler) {
      handlers_[name] = std::move(handler);
    }

    // Process all headers in an envelope.
    // Throws soap_fault_exception if a mustUnderstand header is not handled.
    void
    process(const envelope& env) const {
      for (const auto& hb : env.headers) {
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
