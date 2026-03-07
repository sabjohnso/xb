#include <xb/rng_writer.hpp>

#include <xb/ostream_writer.hpp>

#include <sstream>
#include <vector>

namespace xb {

  namespace {

    using namespace rng;

    const std::string rng_ns_uri = "http://relaxng.org/ns/structure/1.0";

    // Unfold a right-folded binary tree into a flat list of children.
    template <typename BinaryT>
    void
    collect_flat(const pattern& p, std::vector<const pattern*>& out) {
      if (p.holds<BinaryT>()) {
        auto& bin = p.get<BinaryT>();
        if (bin.left) collect_flat<BinaryT>(*bin.left, out);
        if (bin.right) collect_flat<BinaryT>(*bin.right, out);
      } else {
        out.push_back(&p);
      }
    }

    // Writer state — tracks whether the RNG namespace has been declared.
    struct write_ctx {
      xml_writer& w;
      bool ns_declared = false;

      void
      start_rng(const std::string& local) {
        w.start_element(qname(rng_ns_uri, local));
        if (!ns_declared) {
          w.namespace_declaration("", rng_ns_uri);
          ns_declared = true;
        }
      }

      void
      end() {
        w.end_element();
      }

      void
      attr(const std::string& name, const std::string& value) {
        w.attribute(qname("", name), value);
      }
    };

    // Forward declarations
    void
    write_pattern(const pattern& p, write_ctx& ctx);
    void
    write_name_class(const name_class& nc, write_ctx& ctx);

    void
    write_name_class(const name_class& nc, write_ctx& ctx) {
      std::visit(
          [&](const auto& n) {
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T, specific_name>) {
              ctx.start_rng("name");
              if (!n.ns.empty()) { ctx.attr("ns", n.ns); }
              ctx.w.characters(n.local_name);
              ctx.end();
            } else if constexpr (std::is_same_v<T, any_name_nc>) {
              ctx.start_rng("anyName");
              if (n.except) {
                ctx.start_rng("except");
                write_name_class(*n.except, ctx);
                ctx.end();
              }
              ctx.end();
            } else if constexpr (std::is_same_v<T, ns_name_nc>) {
              ctx.start_rng("nsName");
              if (!n.ns.empty()) { ctx.attr("ns", n.ns); }
              if (n.except) {
                ctx.start_rng("except");
                write_name_class(*n.except, ctx);
                ctx.end();
              }
              ctx.end();
            } else if constexpr (std::is_same_v<T, choice_name_class>) {
              // Unfold choice name class tree into flat children
              std::vector<const name_class*> children;
              std::function<void(const name_class&)> collect =
                  [&](const name_class& nc) {
                    if (nc.holds<choice_name_class>()) {
                      auto& c = nc.get<choice_name_class>();
                      if (c.left) collect(*c.left);
                      if (c.right) collect(*c.right);
                    } else {
                      children.push_back(&nc);
                    }
                  };
              collect(nc);

              ctx.start_rng("choice");
              for (const auto* child : children) {
                write_name_class(*child, ctx);
              }
              ctx.end();
            }
          },
          nc.data());
    }

    // Write element or attribute pattern — uses short form (name attribute)
    // for specific_name, child element form for complex name classes.
    void
    write_element_or_attribute(const std::string& tag, const name_class& nc,
                               const pattern* content, write_ctx& ctx) {
      ctx.start_rng(tag);

      if (nc.holds<specific_name>()) {
        auto& sn = nc.get<specific_name>();
        ctx.attr("name", sn.local_name);
        if (!sn.ns.empty()) { ctx.attr("ns", sn.ns); }
      }

      // Write complex name class as child element
      if (!nc.holds<specific_name>()) { write_name_class(nc, ctx); }

      // Write content
      if (content) { write_pattern(*content, ctx); }

      ctx.end();
    }

    // Write N-ary combinator using tree unfolding
    template <typename BinaryT>
    void
    write_combinator(const std::string& tag, const pattern& p, write_ctx& ctx) {
      std::vector<const pattern*> children;
      collect_flat<BinaryT>(p, children);

      ctx.start_rng(tag);
      for (const auto* child : children) {
        write_pattern(*child, ctx);
      }
      ctx.end();
    }

    void
    write_pattern(const pattern& p, write_ctx& ctx) {
      std::visit(
          [&](const auto& node) {
            using T = std::decay_t<decltype(node)>;

            // Leaf patterns
            if constexpr (std::is_same_v<T, empty_pattern>) {
              ctx.start_rng("empty");
              ctx.end();
            } else if constexpr (std::is_same_v<T, text_pattern>) {
              ctx.start_rng("text");
              ctx.end();
            } else if constexpr (std::is_same_v<T, not_allowed_pattern>) {
              ctx.start_rng("notAllowed");
              ctx.end();
            }

            // Element / attribute
            else if constexpr (std::is_same_v<T, element_pattern>) {
              write_element_or_attribute("element", node.name,
                                         node.content.get(), ctx);
            } else if constexpr (std::is_same_v<T, attribute_pattern>) {
              write_element_or_attribute("attribute", node.name,
                                         node.content.get(), ctx);
            }

            // Binary combinators — unfold
            else if constexpr (std::is_same_v<T, group_pattern>) {
              write_combinator<group_pattern>("group", p, ctx);
            } else if constexpr (std::is_same_v<T, interleave_pattern>) {
              write_combinator<interleave_pattern>("interleave", p, ctx);
            } else if constexpr (std::is_same_v<T, choice_pattern>) {
              write_combinator<choice_pattern>("choice", p, ctx);
            }

            // Occurrence
            else if constexpr (std::is_same_v<T, one_or_more_pattern>) {
              ctx.start_rng("oneOrMore");
              if (node.content) write_pattern(*node.content, ctx);
              ctx.end();
            } else if constexpr (std::is_same_v<T, zero_or_more_pattern>) {
              ctx.start_rng("zeroOrMore");
              if (node.content) write_pattern(*node.content, ctx);
              ctx.end();
            } else if constexpr (std::is_same_v<T, optional_pattern>) {
              ctx.start_rng("optional");
              if (node.content) write_pattern(*node.content, ctx);
              ctx.end();
            } else if constexpr (std::is_same_v<T, mixed_pattern>) {
              ctx.start_rng("mixed");
              if (node.content) write_pattern(*node.content, ctx);
              ctx.end();
            }

            // References
            else if constexpr (std::is_same_v<T, ref_pattern>) {
              ctx.start_rng("ref");
              ctx.attr("name", node.name);
              ctx.end();
            } else if constexpr (std::is_same_v<T, parent_ref_pattern>) {
              ctx.start_rng("parentRef");
              ctx.attr("name", node.name);
              ctx.end();
            } else if constexpr (std::is_same_v<T, external_ref_pattern>) {
              ctx.start_rng("externalRef");
              ctx.attr("href", node.href);
              if (!node.ns.empty()) { ctx.attr("ns", node.ns); }
              ctx.end();
            }

            // Data types
            else if constexpr (std::is_same_v<T, data_pattern>) {
              ctx.start_rng("data");
              if (!node.datatype_library.empty()) {
                ctx.attr("datatypeLibrary", node.datatype_library);
              }
              ctx.attr("type", node.type);

              for (const auto& param : node.params) {
                ctx.start_rng("param");
                ctx.attr("name", param.name);
                ctx.w.characters(param.value);
                ctx.end();
              }

              if (node.except) {
                ctx.start_rng("except");
                write_pattern(*node.except, ctx);
                ctx.end();
              }

              ctx.end();
            } else if constexpr (std::is_same_v<T, value_pattern>) {
              ctx.start_rng("value");
              if (!node.datatype_library.empty()) {
                ctx.attr("datatypeLibrary", node.datatype_library);
              }
              ctx.attr("type", node.type);
              if (!node.ns.empty()) { ctx.attr("ns", node.ns); }
              ctx.w.characters(node.value);
              ctx.end();
            } else if constexpr (std::is_same_v<T, list_pattern>) {
              ctx.start_rng("list");
              if (node.content) write_pattern(*node.content, ctx);
              ctx.end();
            }

            // Grammar
            else if constexpr (std::is_same_v<T, grammar_pattern>) {
              ctx.start_rng("grammar");

              if (node.start) {
                ctx.start_rng("start");
                write_pattern(*node.start, ctx);
                ctx.end();
              }

              for (const auto& d : node.defines) {
                ctx.start_rng("define");
                ctx.attr("name", d.name);
                if (d.combine == combine_method::choice) {
                  ctx.attr("combine", "choice");
                } else if (d.combine == combine_method::interleave) {
                  ctx.attr("combine", "interleave");
                }
                if (d.body) write_pattern(*d.body, ctx);
                ctx.end();
              }

              for (const auto& inc : node.includes) {
                ctx.start_rng("include");
                ctx.attr("href", inc.href);
                if (!inc.ns.empty()) { ctx.attr("ns", inc.ns); }

                if (inc.start_override) {
                  ctx.start_rng("start");
                  write_pattern(*inc.start_override, ctx);
                  ctx.end();
                }

                for (const auto& ov : inc.overrides) {
                  ctx.start_rng("define");
                  ctx.attr("name", ov.name);
                  if (ov.combine == combine_method::choice) {
                    ctx.attr("combine", "choice");
                  } else if (ov.combine == combine_method::interleave) {
                    ctx.attr("combine", "interleave");
                  }
                  if (ov.body) write_pattern(*ov.body, ctx);
                  ctx.end();
                }

                ctx.end();
              }

              ctx.end();
            }
          },
          p.data());
    }

  } // namespace

  void
  rng_write(const rng::pattern& p, xml_writer& writer) {
    write_ctx ctx{writer};
    write_pattern(p, ctx);
  }

  std::string
  rng_write_string(const rng::pattern& p) {
    std::ostringstream os;
    ostream_writer writer(os);
    writer.xml_declaration();
    rng_write(p, writer);
    return os.str();
  }

  std::string
  rng_write_string(const rng::pattern& p, int indent) {
    std::ostringstream os;
    ostream_writer writer(os, indent);
    writer.xml_declaration();
    rng_write(p, writer);
    return os.str();
  }

} // namespace xb
