#pragma once

#include <compare>

namespace xb::wire {

  // --- Selector specificity ------------------------------------------------
  //
  // Determines which encoding rule wins when multiple selectors match the
  // same XSD component.  Modeled after CSS specificity:
  //
  //   qname > path > type > namespace > wildcard (empty)
  //
  // Within the same tier, having an attribute selector adds weight.
  // Multiple tiers combine additively (e.g., path + type > path alone).
  //
  // The specificity value is a comparable integer — higher is more specific.
  //
  // This header is parameterized on the selector type so it works with the
  // generated xb::bes::selector_type without creating a header dependency
  // on the generated code.

  struct selector_specificity {
    // Weighted score: each tier contributes a power-of-16 weight so
    // tiers never bleed into each other.
    //
    //   qname:     0x1000
    //   path:      0x0100
    //   type:      0x0010
    //   namespace: 0x0001
    //   attribute: adds 0x0008 (sub-tier boost)

    int value = 0;

    auto
    operator<=>(const selector_specificity&) const = default;
  };

  // Compute specificity of a selector.
  //
  // Works with any type that has optional<string> fields named
  // qname, path, type, namespace_, and attribute (i.e., the generated
  // xb::bes::selector_type).
  template <typename Selector>
  selector_specificity
  specificity(const Selector& s) {
    int v = 0;

    if (s.qname.has_value()) v += 0x1000;
    if (s.path.has_value()) v += 0x0100;
    if (s.type.has_value()) v += 0x0010;
    if (s.namespace_.has_value()) v += 0x0001;
    if (s.attribute.has_value()) v += 0x0008;

    return {v};
  }

  // Returns the selector with higher specificity.  If equal, returns a.
  template <typename Selector>
  const Selector&
  more_specific(const Selector& a, const Selector& b) {
    return specificity(b) > specificity(a) ? b : a;
  }

  // Two selectors conflict when they have equal specificity but differ
  // in content (i.e., they match different things with the same priority).
  template <typename Selector>
  bool
  selectors_conflict(const Selector& a, const Selector& b) {
    if (specificity(a) != specificity(b)) return false;

    // Same specificity — check if they are materially identical
    return a != b;
  }

} // namespace xb::wire
