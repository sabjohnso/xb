#include <xb/schematron_overlay.hpp>

#include <unordered_map>

namespace xb {

  namespace {

    // Resolve a context expression to a namespace URI + local name pair.
    // Supports:
    //   "elementName"     -> {"", elementName}
    //   "prefix:element"  -> {resolved_uri, element}
    struct resolved_context {
      std::string ns;
      std::string local_name;
      bool valid = false;
    };

    resolved_context
    resolve_context(
        const std::string& context,
        const std::unordered_map<std::string, std::string>& ns_map) {
      // Skip complex contexts (paths with /, predicates with [, axes with ::)
      if (context.find('/') != std::string::npos ||
          context.find('[') != std::string::npos ||
          context.find("::") != std::string::npos) {
        return {"", "", false};
      }

      // Check for namespace prefix
      auto colon = context.find(':');
      if (colon != std::string::npos) {
        auto prefix = context.substr(0, colon);
        auto local = context.substr(colon + 1);
        auto it = ns_map.find(prefix);
        if (it != ns_map.end()) { return {it->second, local, true}; }
        return {"", "", false}; // unknown prefix
      }

      // Simple element name — no namespace
      return {"", context, true};
    }

    // Find the complex_type associated with an element in the schema_set
    complex_type*
    find_type_for_element(schema_set& schemas, const std::string& ns,
                          const std::string& local_name) {
      for (auto& s : schemas.schemas()) {
        // Find the element declaration
        for (const auto& e : s.elements()) {
          if (e.name().local_name() == local_name &&
              e.name().namespace_uri() == ns) {
            // Find the associated complex type
            for (auto& ct : s.complex_types()) {
              if (ct.name() == e.type_name()) { return &ct; }
            }
          }
        }
      }
      return nullptr;
    }

  } // namespace

  overlay_result
  schematron_overlay(schema_set& schemas, const schematron::schema& sch) {
    overlay_result result;

    // Build namespace prefix map from <sch:ns> declarations
    std::unordered_map<std::string, std::string> ns_map;
    for (const auto& ns : sch.namespaces) {
      ns_map[ns.prefix] = ns.uri;
    }

    // Process all patterns and rules
    for (const auto& pattern : sch.patterns) {
      for (const auto& rule : pattern.rules) {
        auto ctx = resolve_context(rule.context, ns_map);
        if (!ctx.valid) {
          ++result.rules_unmatched;
          result.warnings.push_back("Unsupported context expression: '" +
                                    rule.context + "'");
          continue;
        }

        auto* ct = find_type_for_element(schemas, ctx.ns, ctx.local_name);
        if (!ct) {
          ++result.rules_unmatched;
          result.warnings.push_back("No matching element for context: '" +
                                    rule.context + "'");
          continue;
        }

        ++result.rules_matched;

        for (const auto& check : rule.checks) {
          assertion a;
          if (check.is_assert) {
            a.test = check.test;
          } else {
            // Report: fires when condition is true, so negate for validation
            a.test = "not(" + check.test + ")";
          }
          ct->add_assertion(std::move(a));
        }
      }
    }

    return result;
  }

} // namespace xb
