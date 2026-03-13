#pragma once

#include <xb/qname.hpp>
#include <xb/schema_set.hpp>
#include <xb/wire/selector_specificity.hpp>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace xb::wire {

  // -------------------------------------------------------------------------
  // Encoding plan: the result of binding BES selectors to XSD types
  //
  // Takes a resolved BES encoding_type (imports already flattened) and
  // an XSD schema_set.  Matches each BES message to its target XSD
  // complex type via selectors, type attributes, or namespace matching.
  //
  // Parameterized on the BES encoding types to avoid a compile-time
  // dependency on the generated code.
  // -------------------------------------------------------------------------

  // A message bound to its target XSD type.
  template <typename MessageType>
  struct bound_message {
    qname target_type;
    const MessageType* message;
  };

  // Parse Clark notation: "{namespace}localName" → qname
  inline std::optional<qname>
  parse_clark(const std::string& s) {
    if (s.empty()) return std::nullopt;
    if (s[0] == '{') {
      auto close = s.find('}');
      if (close == std::string::npos) return std::nullopt;
      return qname{s.substr(1, close - 1), s.substr(close + 1)};
    }
    return std::nullopt;
  }

  template <typename Encoding>
  class encoding_plan {
  public:
    using message_type = typename std::decay_t<decltype(std::get<2>(
        std::declval<typename Encoding::choice_element_type>()))>;
    using framing_type = typename std::decay_t<decltype(std::get<0>(
        std::declval<typename Encoding::choice_element_type>()))>;
    using frame_stack_type = typename std::decay_t<decltype(std::get<1>(
        std::declval<typename Encoding::choice_element_type>()))>;
    using defaults_type =
        typename std::decay_t<decltype(*std::declval<Encoding>().defaults)>;
    using bound_msg = bound_message<message_type>;

    encoding_plan(const Encoding& encoding, const schema_set& schemas) {
      // Store defaults
      if (encoding.defaults.has_value()) defaults_ = *encoding.defaults;

      // Index framings and frame-stacks by name
      for (const auto& choice : encoding.choice) {
        std::visit(
            [this](const auto& item) {
              using T = std::decay_t<decltype(item)>;
              if constexpr (std::is_same_v<T, framing_type>)
                framings_[item.name] = &item;
              else if constexpr (std::is_same_v<T, frame_stack_type>)
                frame_stacks_[item.name] = &item;
            },
            choice);
      }

      // Bind messages to XSD types
      bind_messages(encoding, schemas);
    }

    const std::vector<bound_msg>&
    messages() const {
      return messages_;
    }

    const bound_msg*
    find_message(const qname& xsd_type) const {
      auto it = type_index_.find(xsd_type);
      if (it == type_index_.end()) return nullptr;
      return &messages_[it->second];
    }

    const framing_type*
    find_framing(const std::string& name) const {
      auto it = framings_.find(name);
      return it != framings_.end() ? it->second : nullptr;
    }

    const frame_stack_type*
    find_frame_stack(const std::string& name) const {
      auto it = frame_stacks_.find(name);
      return it != frame_stacks_.end() ? it->second : nullptr;
    }

    const defaults_type&
    defaults() const {
      return defaults_;
    }

  private:
    defaults_type defaults_{};
    std::vector<bound_msg> messages_;
    std::map<qname, std::size_t> type_index_;
    std::map<std::string, const framing_type*> framings_;
    std::map<std::string, const frame_stack_type*> frame_stacks_;

    // Resolve a message's target type from its type attribute or selector.
    // Returns the set of matching XSD type qnames.
    std::vector<qname>
    resolve_targets(const message_type& msg, const Encoding& encoding,
                    const schema_set& schemas) const {
      // 1. Try type attribute (shorthand for qname)
      if (msg.type.has_value() && !msg.type->empty()) {
        auto clark = parse_clark(*msg.type);
        if (clark.has_value()) {
          // Clark notation: {namespace}localName
          if (schemas.find_complex_type(*clark)) return {*clark};
          throw std::runtime_error("BES message '" + msg.name +
                                   "': type not found in schema: " + *msg.type);
        }
        // Local name: resolve via target_namespace
        std::string ns;
        if (encoding.target_namespace.has_value())
          ns = *encoding.target_namespace;
        qname resolved{ns, *msg.type};
        if (schemas.find_complex_type(resolved)) return {resolved};
        throw std::runtime_error("BES message '" + msg.name +
                                 "': type not found in schema: " + *msg.type);
      }

      // 2. Try selector
      if (msg.selector.has_value()) {
        return resolve_selector(*msg.selector, schemas);
      }

      // 3. No type or selector — skip (message has no XSD binding)
      return {};
    }

    template <typename Selector>
    std::vector<qname>
    resolve_selector(const Selector& sel, const schema_set& schemas) const {
      std::vector<qname> matches;

      // qname selector: exact match
      if (sel.qname.has_value()) {
        auto clark = parse_clark(*sel.qname);
        if (clark.has_value() && schemas.find_complex_type(*clark))
          matches.push_back(*clark);
        return matches;
      }

      // namespace selector: match all complex types in namespace
      if (sel.namespace_.has_value()) {
        for (const auto& s : schemas.schemas()) {
          if (s.target_namespace() == *sel.namespace_) {
            for (const auto& ct : s.complex_types())
              matches.push_back(ct.name());
          }
        }
        return matches;
      }

      // type selector: match complex types by local name
      if (sel.type.has_value()) {
        for (const auto& s : schemas.schemas()) {
          for (const auto& ct : s.complex_types()) {
            if (ct.name().local_name() == *sel.type)
              matches.push_back(ct.name());
          }
        }
        return matches;
      }

      return matches;
    }

    void
    bind_messages(const Encoding& encoding, const schema_set& schemas) {
      // Collect all message bindings with their specificities.
      // When multiple messages match the same XSD type, the one with
      // highest specificity wins.

      struct candidate {
        const message_type* msg;
        selector_specificity spec;
      };

      std::map<qname, candidate> best;

      for (const auto& choice : encoding.choice) {
        const auto* msg = std::get_if<message_type>(&choice);
        if (!msg) continue;

        auto targets = resolve_targets(*msg, encoding, schemas);
        auto spec =
            msg->selector.has_value()
                ? specificity(*msg->selector)
                : selector_specificity{0x1000}; // type attr = qname specificity

        for (const auto& target : targets) {
          auto it = best.find(target);
          if (it == best.end() || spec > it->second.spec)
            best[target] = {msg, spec};
        }
      }

      // Build the result
      for (auto& [type, cand] : best) {
        auto idx = messages_.size();
        messages_.push_back({type, cand.msg});
        type_index_[type] = idx;
      }
    }
  };

} // namespace xb::wire
