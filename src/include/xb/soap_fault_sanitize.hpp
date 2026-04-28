#pragma once

/// @file
/// Helper that scrubs server-side leakage patterns out of SOAP
/// faults before they ship to a (potentially attacker-controlled)
/// peer.  Callers that intentionally publish detailed fault
/// information SHOULD attach it AFTER sanitisation.

#include <xb/soap_model.hpp>

#include <string>

namespace xb::soap {

  /// Knobs for @ref sanitize_fault.
  struct fault_sanitize_options {
    /// When @c true, every text field on the fault is replaced
    /// wholesale with @ref generic_message and the @c detail element
    /// is dropped.  When @c false (the default), the helper performs
    /// pattern-based redaction: filesystem paths, hex addresses, and
    /// SQL-error fragments are replaced with neutral placeholders;
    /// the @c detail element is still cleared because its application
    /// content is rarely audit-safe by default.
    bool replace_with_generic = false;

    /// String substituted when @ref replace_with_generic is @c true.
    std::string generic_message = "Internal server error";
  };

  /// Mutate @p f in place, removing common leakage patterns from
  /// every text field (and the @c detail element).
  ///
  /// The default behaviour is conservative: leave benign messages
  /// alone, redact only patterns that match recognised leakage
  /// shapes.  If your policy is "never leak any internal data,
  /// period", set @ref fault_sanitize_options::replace_with_generic
  /// to @c true.
  void
  sanitize_fault(fault& f, const fault_sanitize_options& opts = {});

} // namespace xb::soap
