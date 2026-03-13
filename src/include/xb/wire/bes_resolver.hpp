#pragma once

#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace xb::wire {

  // -------------------------------------------------------------------------
  // BES import/cascade resolver
  //
  // Resolves <import href="..."> references in a BES encoding_type,
  // producing a flattened document with all imports merged.
  //
  // Cascade rules:
  //   - Local definitions override imported definitions (by name)
  //   - Later imports override earlier imports
  //   - Defaults merge field-by-field (local wins per-field)
  //   - Transitive imports are resolved recursively
  //   - Circular imports are detected and throw std::runtime_error
  //
  // The resolver is parameterized on the encoding type to avoid a
  // header dependency on the generated BES types.
  // -------------------------------------------------------------------------

  // Merge two defaults: overlay values win when present, otherwise
  // base values are kept.
  template <typename Defaults>
  Defaults
  merge_defaults(const Defaults& base, const Defaults& overlay) {
    Defaults result;
    result.byte_order =
        overlay.byte_order ? overlay.byte_order : base.byte_order;
    result.bit_order = overlay.bit_order ? overlay.bit_order : base.bit_order;
    result.alignment = overlay.alignment ? overlay.alignment : base.alignment;
    result.string_padding =
        overlay.string_padding ? overlay.string_padding : base.string_padding;
    result.string_encoding = overlay.string_encoding ? overlay.string_encoding
                                                     : base.string_encoding;
    return result;
  }

  namespace detail {

    // Extract the name from a choice variant element.
    // Works with framing_type, frame_stack_type, and message_type — all
    // have a `name` field.
    template <typename Variant>
    std::string
    choice_name(const Variant& v) {
      return std::visit([](const auto& item) { return item.name; }, v);
    }

    // Merge imported choice elements into an accumulator.
    // For each imported element, add it only if no same-name element
    // exists in the accumulator.  This implements "overlay wins": the
    // caller adds the overlay first, then merges the base underneath.
    template <typename ChoiceVec>
    void
    merge_choice_underneath(ChoiceVec& accum, ChoiceVec&& imported) {
      std::set<std::string> existing;
      for (const auto& c : accum)
        existing.insert(choice_name(c));

      for (auto& c : imported) {
        if (existing.count(choice_name(c)) == 0) {
          existing.insert(choice_name(c));
          accum.push_back(std::move(c));
        }
      }
    }

    // Recursive resolution implementation with cycle detection.
    template <typename Encoding, typename Loader>
    Encoding
    resolve_impl(Encoding doc, const Loader& loader,
                 std::set<std::string>& import_stack) {
      if (doc.import.empty()) return doc;

      // Resolve each import in declaration order.
      // Build an accumulator starting empty, then layer imports on top
      // of each other (later imports override earlier).
      // Finally, the local document overrides everything.

      // Accumulator for merged imports
      Encoding merged;

      for (auto& imp : doc.import) {
        if (import_stack.count(imp.href))
          throw std::runtime_error("circular BES import: " + imp.href);

        import_stack.insert(imp.href);
        auto imported = resolve_impl(loader(imp.href), loader, import_stack);
        import_stack.erase(imp.href);

        // Merge defaults: later import overrides earlier
        if (imported.defaults.has_value()) {
          if (merged.defaults.has_value())
            merged.defaults =
                merge_defaults(*merged.defaults, *imported.defaults);
          else
            merged.defaults = std::move(imported.defaults);
        }

        // Merge choice: later import overrides earlier (by name)
        // We add the new import's elements, overriding any same-name
        // elements from earlier imports.
        // Strategy: imported elements override existing merged elements.
        // To do this efficiently, we rebuild: take imported as overlay.
        merge_choice_underneath(imported.choice, std::move(merged.choice));
        merged.choice = std::move(imported.choice);
      }

      // Now overlay the local document on top of merged imports

      // Defaults: local overrides merged
      if (doc.defaults.has_value()) {
        if (merged.defaults.has_value())
          merged.defaults = merge_defaults(*merged.defaults, *doc.defaults);
        else
          merged.defaults = std::move(doc.defaults);
      }

      // Choice: local overrides merged (by name)
      merge_choice_underneath(doc.choice, std::move(merged.choice));

      // Build result
      Encoding result;
      result.target_namespace = std::move(doc.target_namespace);
      result.target_schema = std::move(doc.target_schema);
      result.defaults = std::move(merged.defaults);
      result.choice = std::move(doc.choice);
      // imports are consumed — leave empty
      return result;
    }

  } // namespace detail

  // Resolve all imports in a BES encoding document, producing a
  // flattened document with cascade rules applied.
  //
  // The loader function takes an href string and returns a parsed
  // encoding document.  The caller is responsible for path resolution
  // and caching.
  template <typename Encoding, typename Loader>
  Encoding
  resolve_imports(Encoding root, const Loader& loader) {
    std::set<std::string> import_stack;
    return detail::resolve_impl(std::move(root), loader, import_stack);
  }

} // namespace xb::wire
