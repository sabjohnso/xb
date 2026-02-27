#include <xb/rng_simplify.hpp>

#include <algorithm>
#include <set>
#include <unordered_map>

namespace xb {

  namespace {

    using namespace rng;

    // Forward declaration
    std::unique_ptr<pattern>
    simplify_impl(std::unique_ptr<pattern> p,
                  const rng_file_resolver& resolver);

    // Helper: move a unique_ptr's content out, simplify, wrap back
    std::unique_ptr<pattern>
    simplify_child(std::unique_ptr<pattern> p,
                   const rng_file_resolver& resolver) {
      if (!p) return nullptr;
      return simplify_impl(std::move(p), resolver);
    }

    // Check if a pattern is notAllowed
    bool
    is_not_allowed(const pattern& p) {
      return p.holds<not_allowed_pattern>();
    }

    // 4.17: Merge defines with the same name using combine method
    void
    merge_combines(std::vector<define>& defines) {
      std::unordered_map<std::string, std::size_t> first_index;
      std::vector<define> merged;

      for (auto& d : defines) {
        auto it = first_index.find(d.name);
        if (it == first_index.end()) {
          first_index[d.name] = merged.size();
          merged.push_back(std::move(d));
        } else {
          auto& existing = merged[it->second];
          // Determine combine method (at most one may omit combine)
          combine_method cm = d.combine;
          if (cm == combine_method::none) cm = existing.combine;
          if (cm == combine_method::none) {
            // Both omit combine — error per spec, but be lenient
            cm = combine_method::choice;
          }

          if (cm == combine_method::choice) {
            existing.body = make_pattern(
                choice_pattern{std::move(existing.body), std::move(d.body)});
          } else {
            existing.body = make_pattern(interleave_pattern{
                std::move(existing.body), std::move(d.body)});
          }
          existing.combine = cm;
        }
      }

      defines = std::move(merged);
    }

    // 4.19: Collect reachable definition names
    void
    collect_refs(const pattern& p, std::set<std::string>& refs) {
      std::visit(
          [&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ref_pattern>) {
              refs.insert(node.name);
            } else if constexpr (std::is_same_v<T, element_pattern> ||
                                 std::is_same_v<T, attribute_pattern>) {
              if (node.content) collect_refs(*node.content, refs);
            } else if constexpr (std::is_same_v<T, group_pattern> ||
                                 std::is_same_v<T, interleave_pattern> ||
                                 std::is_same_v<T, choice_pattern>) {
              if (node.left) collect_refs(*node.left, refs);
              if (node.right) collect_refs(*node.right, refs);
            } else if constexpr (std::is_same_v<T, one_or_more_pattern> ||
                                 std::is_same_v<T, list_pattern>) {
              if (node.content) collect_refs(*node.content, refs);
            } else if constexpr (std::is_same_v<T, data_pattern>) {
              if (node.except) collect_refs(*node.except, refs);
            } else if constexpr (std::is_same_v<T, grammar_pattern>) {
              if (node.start) collect_refs(*node.start, refs);
              for (const auto& d : node.defines) {
                if (d.body) collect_refs(*d.body, refs);
              }
            }
          },
          p.data());
    }

    // 4.19: Remove unreachable definitions
    void
    remove_unreachable(grammar_pattern& g) {
      // Iteratively find reachable names (fixed-point)
      std::set<std::string> reachable;
      if (g.start) collect_refs(*g.start, reachable);

      bool changed = true;
      while (changed) {
        changed = false;
        for (const auto& d : g.defines) {
          if (reachable.count(d.name) && d.body) {
            std::set<std::string> new_refs;
            collect_refs(*d.body, new_refs);
            for (const auto& r : new_refs) {
              if (reachable.insert(r).second) changed = true;
            }
          }
        }
      }

      // Remove unreachable
      g.defines.erase(std::remove_if(g.defines.begin(), g.defines.end(),
                                     [&](const define& d) {
                                       return reachable.count(d.name) == 0;
                                     }),
                      g.defines.end());
    }

    std::unique_ptr<pattern>
    simplify_impl(std::unique_ptr<pattern> p,
                  const rng_file_resolver& resolver) {
      if (!p) return nullptr;

      return std::visit(
          [&](auto& node) -> std::unique_ptr<pattern> {
            using T = std::decay_t<decltype(node)>;

            // Leaf patterns pass through
            if constexpr (std::is_same_v<T, empty_pattern> ||
                          std::is_same_v<T, text_pattern> ||
                          std::is_same_v<T, not_allowed_pattern> ||
                          std::is_same_v<T, ref_pattern> ||
                          std::is_same_v<T, parent_ref_pattern> ||
                          std::is_same_v<T, value_pattern>) {
              return make_pattern(std::move(node));
            }

            // 4.13: mixed → interleave(content, text)
            if constexpr (std::is_same_v<T, mixed_pattern>) {
              auto content = simplify_child(std::move(node.content), resolver);
              return make_pattern(interleave_pattern{
                  std::move(content), make_pattern(text_pattern{})});
            }

            // 4.14: optional → choice(content, empty)
            if constexpr (std::is_same_v<T, optional_pattern>) {
              auto content = simplify_child(std::move(node.content), resolver);
              return make_pattern(choice_pattern{
                  std::move(content), make_pattern(empty_pattern{})});
            }

            // 4.15: zeroOrMore → choice(oneOrMore(content), empty)
            if constexpr (std::is_same_v<T, zero_or_more_pattern>) {
              auto content = simplify_child(std::move(node.content), resolver);
              return make_pattern(choice_pattern{
                  make_pattern(one_or_more_pattern{std::move(content)}),
                  make_pattern(empty_pattern{})});
            }

            // Recurse into element / attribute
            if constexpr (std::is_same_v<T, element_pattern>) {
              node.content = simplify_child(std::move(node.content), resolver);
              return make_pattern(std::move(node));
            }
            if constexpr (std::is_same_v<T, attribute_pattern>) {
              node.content = simplify_child(std::move(node.content), resolver);
              // 4.20: attribute(nc, notAllowed) → notAllowed
              if (node.content && is_not_allowed(*node.content)) {
                return make_pattern(not_allowed_pattern{});
              }
              return make_pattern(std::move(node));
            }

            // Binary combinators: group, interleave, choice
            if constexpr (std::is_same_v<T, group_pattern>) {
              node.left = simplify_child(std::move(node.left), resolver);
              node.right = simplify_child(std::move(node.right), resolver);
              // 4.20: group(notAllowed, _) or group(_, notAllowed)
              if ((node.left && is_not_allowed(*node.left)) ||
                  (node.right && is_not_allowed(*node.right))) {
                return make_pattern(not_allowed_pattern{});
              }
              return make_pattern(std::move(node));
            }
            if constexpr (std::is_same_v<T, interleave_pattern>) {
              node.left = simplify_child(std::move(node.left), resolver);
              node.right = simplify_child(std::move(node.right), resolver);
              // 4.20: interleave(notAllowed, _) or interleave(_, notAllowed)
              if ((node.left && is_not_allowed(*node.left)) ||
                  (node.right && is_not_allowed(*node.right))) {
                return make_pattern(not_allowed_pattern{});
              }
              return make_pattern(std::move(node));
            }
            if constexpr (std::is_same_v<T, choice_pattern>) {
              node.left = simplify_child(std::move(node.left), resolver);
              node.right = simplify_child(std::move(node.right), resolver);
              // 4.20: choice(notAllowed, p) → p
              if (node.left && is_not_allowed(*node.left)) {
                return std::move(node.right);
              }
              // 4.20: choice(p, notAllowed) → p
              if (node.right && is_not_allowed(*node.right)) {
                return std::move(node.left);
              }
              return make_pattern(std::move(node));
            }

            // oneOrMore
            if constexpr (std::is_same_v<T, one_or_more_pattern>) {
              node.content = simplify_child(std::move(node.content), resolver);
              // 4.20: oneOrMore(notAllowed) → notAllowed
              if (node.content && is_not_allowed(*node.content)) {
                return make_pattern(not_allowed_pattern{});
              }
              return make_pattern(std::move(node));
            }

            // list
            if constexpr (std::is_same_v<T, list_pattern>) {
              node.content = simplify_child(std::move(node.content), resolver);
              // 4.20: list(notAllowed) → notAllowed
              if (node.content && is_not_allowed(*node.content)) {
                return make_pattern(not_allowed_pattern{});
              }
              return make_pattern(std::move(node));
            }

            // data (simplify except pattern if present)
            if constexpr (std::is_same_v<T, data_pattern>) {
              node.except = simplify_child(std::move(node.except), resolver);
              return make_pattern(std::move(node));
            }

            // external_ref: expand using resolver
            if constexpr (std::is_same_v<T, external_ref_pattern>) {
              if (resolver) {
                auto content = resolver(node.href);
                // Parse the content as a pattern
                // For now, return notAllowed if resolver not impl'd
                // In production, this would parse the external file
              }
              // Without resolver, leave as-is
              return make_pattern(std::move(node));
            }

            // grammar: merge combines, simplify bodies, remove unreachable
            if constexpr (std::is_same_v<T, grammar_pattern>) {
              // 4.17: merge combines
              merge_combines(node.defines);

              // Simplify all define bodies and start
              node.start = simplify_child(std::move(node.start), resolver);
              for (auto& d : node.defines) {
                d.body = simplify_child(std::move(d.body), resolver);
              }

              // 4.19: remove unreachable definitions
              remove_unreachable(node);

              return make_pattern(std::move(node));
            }

            // Fallback — should not be reached for valid patterns
            return make_pattern(std::move(node));
          },
          p->data());
    }

  } // namespace

  rng::pattern
  rng_simplify(rng::pattern input, const rng_file_resolver& resolver) {
    auto p = rng::make_pattern(std::move(input));
    auto result = simplify_impl(std::move(p), resolver);
    return std::move(*result);
  }

} // namespace xb
