#include <xb/rng_compact_writer.hpp>

#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb {

  namespace {

    using namespace rng;

    const std::string xsd_dt_uri = "http://www.w3.org/2001/XMLSchema-datatypes";

    // RNC keywords that need backslash escaping when used as identifiers
    const std::unordered_set<std::string> rnc_keywords = {
        "attribute", "default",  "datatypes", "div",        "element",
        "empty",     "external", "grammar",   "include",    "inherit",
        "list",      "mixed",    "namespace", "notAllowed", "parent",
        "start",     "string",   "token",     "text",
    };

    // -----------------------------------------------------------------------
    // Precedence levels for parenthesization
    // -----------------------------------------------------------------------

    enum class prec { none, choice, interleave, group, postfix, primary };

    // -----------------------------------------------------------------------
    // Tree unfolding
    // -----------------------------------------------------------------------

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

    // -----------------------------------------------------------------------
    // Namespace / datatype library collector
    // -----------------------------------------------------------------------

    struct ns_context {
      std::string default_ns;
      std::unordered_map<std::string, std::string> ns_prefixes; // uri -> prefix
      std::unordered_map<std::string, std::string> dt_prefixes; // uri -> prefix
      int ns_counter = 0;

      std::string
      get_ns_prefix(const std::string& uri) {
        if (uri.empty()) return "";
        if (uri == default_ns) return "";
        auto it = ns_prefixes.find(uri);
        if (it != ns_prefixes.end()) return it->second;
        auto prefix = "ns" + std::to_string(++ns_counter);
        ns_prefixes[uri] = prefix;
        return prefix;
      }

      std::string
      get_dt_prefix(const std::string& uri) {
        if (uri.empty()) return "";
        auto it = dt_prefixes.find(uri);
        if (it != dt_prefixes.end()) return it->second;
        std::string prefix;
        if (uri == xsd_dt_uri) {
          prefix = "xsd";
        } else {
          prefix = "dt" + std::to_string(dt_prefixes.size() + 1);
        }
        dt_prefixes[uri] = prefix;
        return prefix;
      }
    };

    // Walk the tree collecting all namespace URIs and datatype libraries
    void
    collect_namespaces(const pattern& p, std::set<std::string>& ns_uris,
                       std::set<std::string>& dt_uris);

    void
    collect_nc_namespaces(const name_class& nc,
                          std::set<std::string>& ns_uris) {
      std::visit(
          [&](const auto& n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, specific_name>) {
              if (!n.ns.empty()) ns_uris.insert(n.ns);
            } else if constexpr (std::is_same_v<T, ns_name_nc>) {
              if (!n.ns.empty()) ns_uris.insert(n.ns);
              if (n.except) collect_nc_namespaces(*n.except, ns_uris);
            } else if constexpr (std::is_same_v<T, any_name_nc>) {
              if (n.except) collect_nc_namespaces(*n.except, ns_uris);
            } else if constexpr (std::is_same_v<T, choice_name_class>) {
              if (n.left) collect_nc_namespaces(*n.left, ns_uris);
              if (n.right) collect_nc_namespaces(*n.right, ns_uris);
            }
          },
          nc.data());
    }

    void
    collect_namespaces(const pattern& p, std::set<std::string>& ns_uris,
                       std::set<std::string>& dt_uris) {
      std::visit(
          [&](const auto& node) {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, element_pattern> ||
                          std::is_same_v<T, attribute_pattern>) {
              collect_nc_namespaces(node.name, ns_uris);
              if (node.content)
                collect_namespaces(*node.content, ns_uris, dt_uris);
            } else if constexpr (std::is_same_v<T, group_pattern> ||
                                 std::is_same_v<T, interleave_pattern> ||
                                 std::is_same_v<T, choice_pattern>) {
              if (node.left) collect_namespaces(*node.left, ns_uris, dt_uris);
              if (node.right) collect_namespaces(*node.right, ns_uris, dt_uris);
            } else if constexpr (std::is_same_v<T, one_or_more_pattern> ||
                                 std::is_same_v<T, zero_or_more_pattern> ||
                                 std::is_same_v<T, optional_pattern> ||
                                 std::is_same_v<T, mixed_pattern> ||
                                 std::is_same_v<T, list_pattern>) {
              if (node.content)
                collect_namespaces(*node.content, ns_uris, dt_uris);
            } else if constexpr (std::is_same_v<T, data_pattern>) {
              if (!node.datatype_library.empty())
                dt_uris.insert(node.datatype_library);
              if (node.except)
                collect_namespaces(*node.except, ns_uris, dt_uris);
            } else if constexpr (std::is_same_v<T, value_pattern>) {
              if (!node.datatype_library.empty())
                dt_uris.insert(node.datatype_library);
            } else if constexpr (std::is_same_v<T, grammar_pattern>) {
              if (node.start) collect_namespaces(*node.start, ns_uris, dt_uris);
              for (const auto& d : node.defines) {
                if (d.body) collect_namespaces(*d.body, ns_uris, dt_uris);
              }
              for (const auto& inc : node.includes) {
                if (inc.start_override)
                  collect_namespaces(*inc.start_override, ns_uris, dt_uris);
                for (const auto& ov : inc.overrides) {
                  if (ov.body) collect_namespaces(*ov.body, ns_uris, dt_uris);
                }
              }
            }
          },
          p.data());
    }

    // -----------------------------------------------------------------------
    // RNC writer
    // -----------------------------------------------------------------------

    std::string
    escape_name(const std::string& name) {
      if (rnc_keywords.count(name)) return "\\" + name;
      return name;
    }

    // Forward declarations
    void
    write_rnc(const pattern& p, std::ostream& os, ns_context& ctx,
              prec parent_prec, int indent, int depth);
    void
    write_nc(const name_class& nc, std::ostream& os, ns_context& ctx);

    // Determine whether a pattern renders as a single inline token
    // (no braces, no line breaks needed).
    bool
    is_inline(const pattern& p) {
      return std::visit(
          [](const auto& node) -> bool {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, empty_pattern> ||
                          std::is_same_v<T, text_pattern> ||
                          std::is_same_v<T, not_allowed_pattern> ||
                          std::is_same_v<T, ref_pattern> ||
                          std::is_same_v<T, parent_ref_pattern> ||
                          std::is_same_v<T, external_ref_pattern> ||
                          std::is_same_v<T, value_pattern>) {
              return true;
            } else if constexpr (std::is_same_v<T, data_pattern>) {
              return node.params.empty() && !node.except;
            } else if constexpr (std::is_same_v<T, one_or_more_pattern> ||
                                 std::is_same_v<T, zero_or_more_pattern> ||
                                 std::is_same_v<T, optional_pattern>) {
              return node.content && is_inline(*node.content);
            } else {
              return false;
            }
          },
          p.data());
    }

    void
    write_indent(std::ostream& os, int indent, int depth) {
      for (int i = 0; i < indent * depth; ++i) {
        os << ' ';
      }
    }

    void
    write_nc(const name_class& nc, std::ostream& os, ns_context& ctx) {
      std::visit(
          [&](const auto& n) {
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T, specific_name>) {
              auto prefix = ctx.get_ns_prefix(n.ns);
              if (!prefix.empty()) { os << prefix << ":"; }
              os << n.local_name;
            } else if constexpr (std::is_same_v<T, any_name_nc>) {
              os << "*";
              if (n.except) {
                os << " - ";
                write_nc(*n.except, os, ctx);
              }
            } else if constexpr (std::is_same_v<T, ns_name_nc>) {
              auto prefix = ctx.get_ns_prefix(n.ns);
              if (!prefix.empty()) {
                os << prefix << ":*";
              } else {
                os << "*";
              }
              if (n.except) {
                os << " - ";
                write_nc(*n.except, os, ctx);
              }
            } else if constexpr (std::is_same_v<T, choice_name_class>) {
              // Flatten the choice tree
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

              os << "(";
              for (std::size_t i = 0; i < children.size(); ++i) {
                if (i > 0) os << " | ";
                write_nc(*children[i], os, ctx);
              }
              os << ")";
            }
          },
          nc.data());
    }

    // Write the name class for an element or attribute pattern.
    // For element, unqualified names use default namespace.
    // For attribute, unqualified names have empty namespace.
    void
    write_element_nc(const name_class& nc, std::ostream& os, ns_context& ctx) {
      write_nc(nc, os, ctx);
    }

    bool
    is_builtin_datatype(const std::string& dt_lib, const std::string& type) {
      return dt_lib.empty() && (type == "string" || type == "token");
    }

    template <typename BinaryT>
    void
    write_binary_chain(const pattern& p, const std::string& op, prec my_prec,
                       std::ostream& os, ns_context& ctx, prec parent_prec,
                       int indent, int depth) {
      std::vector<const pattern*> children;
      collect_flat<BinaryT>(p, children);

      bool need_parens = parent_prec > my_prec;
      if (need_parens) os << "(";

      // Choice with indent: leading-pipe format
      //     first
      //   | second
      //   | third
      if (indent > 0 && !need_parens && op == "|") {
        write_indent(os, indent, 1);
        write_rnc(*children[0], os, ctx, my_prec, indent, depth);
        for (std::size_t i = 1; i < children.size(); ++i) {
          os << "\n";
          write_indent(os, indent, depth);
          os << "| ";
          write_rnc(*children[i], os, ctx, my_prec, indent, depth);
        }
      } else {
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (i > 0) {
            if (indent > 0 && !need_parens) {
              os << ",\n";
              write_indent(os, indent, depth);
            } else {
              if (op == ",")
                os << ", ";
              else
                os << " " << op << " ";
            }
          }
          write_rnc(*children[i], os, ctx, my_prec, indent, depth);
        }
      }

      if (need_parens) os << ")";
    }

    // Write a braced block: inline if content is simple, multi-line otherwise.
    void
    write_braced(const pattern* content, std::ostream& os, ns_context& ctx,
                 int indent, int depth) {
      if (indent > 0 && content && !is_inline(*content)) {
        os << " {\n";
        write_indent(os, indent, depth + 1);
        write_rnc(*content, os, ctx, prec::none, indent, depth + 1);
        os << "\n";
        write_indent(os, indent, depth);
        os << "}";
      } else {
        os << " { ";
        if (content) write_rnc(*content, os, ctx, prec::none, indent, depth);
        os << " }";
      }
    }

    void
    write_rnc(const pattern& p, std::ostream& os, ns_context& ctx,
              prec parent_prec, int indent, int depth) {
      std::visit(
          [&](const auto& node) {
            using T = std::decay_t<decltype(node)>;

            // Leaf patterns
            if constexpr (std::is_same_v<T, empty_pattern>) {
              os << "empty";
            } else if constexpr (std::is_same_v<T, text_pattern>) {
              os << "text";
            } else if constexpr (std::is_same_v<T, not_allowed_pattern>) {
              os << "notAllowed";
            }

            // Element / attribute
            else if constexpr (std::is_same_v<T, element_pattern>) {
              os << "element ";
              write_element_nc(node.name, os, ctx);
              write_braced(node.content.get(), os, ctx, indent, depth);
            } else if constexpr (std::is_same_v<T, attribute_pattern>) {
              os << "attribute ";
              write_element_nc(node.name, os, ctx);
              write_braced(node.content.get(), os, ctx, indent, depth);
            }

            // Binary combinators
            else if constexpr (std::is_same_v<T, group_pattern>) {
              write_binary_chain<group_pattern>(p, ",", prec::group, os, ctx,
                                                parent_prec, indent, depth);
            } else if constexpr (std::is_same_v<T, interleave_pattern>) {
              write_binary_chain<interleave_pattern>(p, "&", prec::interleave,
                                                     os, ctx, parent_prec,
                                                     indent, depth);
            } else if constexpr (std::is_same_v<T, choice_pattern>) {
              write_binary_chain<choice_pattern>(p, "|", prec::choice, os, ctx,
                                                 parent_prec, indent, depth);
            }

            // Postfix operators
            else if constexpr (std::is_same_v<T, one_or_more_pattern>) {
              if (node.content)
                write_rnc(*node.content, os, ctx, prec::postfix, indent, depth);
              os << "+";
            } else if constexpr (std::is_same_v<T, zero_or_more_pattern>) {
              if (node.content)
                write_rnc(*node.content, os, ctx, prec::postfix, indent, depth);
              os << "*";
            } else if constexpr (std::is_same_v<T, optional_pattern>) {
              if (node.content)
                write_rnc(*node.content, os, ctx, prec::postfix, indent, depth);
              os << "?";
            } else if constexpr (std::is_same_v<T, mixed_pattern>) {
              os << "mixed";
              write_braced(node.content.get(), os, ctx, indent, depth);
            }

            // References
            else if constexpr (std::is_same_v<T, ref_pattern>) {
              os << escape_name(node.name);
            } else if constexpr (std::is_same_v<T, parent_ref_pattern>) {
              os << "parent " << escape_name(node.name);
            } else if constexpr (std::is_same_v<T, external_ref_pattern>) {
              os << "external \"" << node.href << "\"";
            }

            // Data types
            else if constexpr (std::is_same_v<T, data_pattern>) {
              if (is_builtin_datatype(node.datatype_library, node.type)) {
                os << node.type;
              } else {
                auto prefix = ctx.get_dt_prefix(node.datatype_library);
                os << prefix << ":" << node.type;
              }

              if (!node.params.empty()) {
                os << " {";
                for (const auto& param : node.params) {
                  os << " " << param.name << " = \"" << param.value << "\"";
                }
                os << " }";
              }

              if (node.except) {
                os << " - ";
                write_rnc(*node.except, os, ctx, prec::postfix, indent, depth);
              }
            } else if constexpr (std::is_same_v<T, value_pattern>) {
              if (!node.datatype_library.empty()) {
                auto prefix = ctx.get_dt_prefix(node.datatype_library);
                os << prefix << ":" << node.type << " ";
              }
              os << "\"" << node.value << "\"";
            } else if constexpr (std::is_same_v<T, list_pattern>) {
              os << "list";
              write_braced(node.content.get(), os, ctx, indent, depth);
            }

            // Grammar
            else if constexpr (std::is_same_v<T, grammar_pattern>) {
              if (node.start) {
                if (indent > 0 && !is_inline(*node.start)) {
                  os << "start =\n";
                  write_indent(os, indent, 1);
                  write_rnc(*node.start, os, ctx, prec::none, indent, 1);
                } else {
                  os << "start = ";
                  write_rnc(*node.start, os, ctx, prec::none, indent, 0);
                }
                os << "\n";
              }

              for (const auto& d : node.defines) {
                os << escape_name(d.name);
                std::string op = " = ";
                if (d.combine == combine_method::choice)
                  op = " |= ";
                else if (d.combine == combine_method::interleave)
                  op = " &= ";

                if (d.body) {
                  bool breaks = indent > 0 && !is_inline(*d.body);
                  if (breaks) {
                    // Trim trailing space from operator
                    os << op.substr(0, op.size() - 1) << "\n";
                    write_indent(os, indent, 1);
                    write_rnc(*d.body, os, ctx, prec::none, indent, 1);
                  } else {
                    os << op;
                    write_rnc(*d.body, os, ctx, prec::none, indent, 0);
                  }
                } else {
                  os << op;
                }
                os << "\n";
              }

              for (const auto& inc : node.includes) {
                os << "include \"" << inc.href << "\"";
                if (!inc.overrides.empty() || inc.start_override) {
                  os << " {\n";
                  if (inc.start_override) {
                    write_indent(os, indent > 0 ? indent : 2, 1);
                    os << "start = ";
                    write_rnc(*inc.start_override, os, ctx, prec::none, indent,
                              1);
                    os << "\n";
                  }
                  for (const auto& ov : inc.overrides) {
                    write_indent(os, indent > 0 ? indent : 2, 1);
                    os << escape_name(ov.name);
                    if (ov.combine == combine_method::choice) {
                      os << " |= ";
                    } else if (ov.combine == combine_method::interleave) {
                      os << " &= ";
                    } else {
                      os << " = ";
                    }
                    if (ov.body)
                      write_rnc(*ov.body, os, ctx, prec::none, indent, 1);
                    os << "\n";
                  }
                  os << "}";
                }
                os << "\n";
              }
            }
          },
          p.data());
    }

    std::string
    rng_compact_write_impl(const rng::pattern& p, int indent) {
      // Pre-pass: collect namespaces and datatype libraries
      std::set<std::string> ns_uris;
      std::set<std::string> dt_uris;
      collect_namespaces(p, ns_uris, dt_uris);

      ns_context ctx;

      // Determine default namespace (most common element namespace)
      // For simplicity, pick the first non-empty one
      for (const auto& uri : ns_uris) {
        if (!uri.empty()) {
          ctx.default_ns = uri;
          break;
        }
      }

      // Pre-register datatype prefixes
      for (const auto& uri : dt_uris) {
        ctx.get_dt_prefix(uri);
      }

      // Pre-register namespace prefixes (except default)
      for (const auto& uri : ns_uris) {
        if (!uri.empty() && uri != ctx.default_ns) { ctx.get_ns_prefix(uri); }
      }

      // Build preamble
      std::ostringstream preamble;
      if (!ctx.default_ns.empty()) {
        preamble << "default namespace = \"" << ctx.default_ns << "\"\n";
      }
      for (const auto& [uri, prefix] : ctx.ns_prefixes) {
        preamble << "namespace " << prefix << " = \"" << uri << "\"\n";
      }
      for (const auto& [uri, prefix] : ctx.dt_prefixes) {
        preamble << "datatypes " << prefix << " = \"" << uri << "\"\n";
      }

      std::string preamble_str = preamble.str();

      // Build body
      std::ostringstream body;
      write_rnc(p, body, ctx, prec::none, indent, 0);

      // For non-grammar patterns, add trailing newline
      if (!p.holds<rng::grammar_pattern>()) { body << "\n"; }

      // Combine with blank line separator if preamble is non-empty
      if (!preamble_str.empty()) { return preamble_str + "\n" + body.str(); }
      return body.str();
    }

  } // namespace

  std::string
  rng_compact_write(const rng::pattern& p) {
    return rng_compact_write_impl(p, 0);
  }

  std::string
  rng_compact_write(const rng::pattern& p, int indent) {
    return rng_compact_write_impl(p, indent);
  }

} // namespace xb
