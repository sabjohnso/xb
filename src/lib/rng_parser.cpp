#include <xb/rng_parser.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xb {

  namespace {

    const std::string rng_ns = "http://relaxng.org/ns/structure/1.0";

    bool
    is_whitespace_only(std::string_view sv) {
      return !sv.empty() &&
             sv.find_first_not_of(" \n\r\t") == std::string_view::npos;
    }

    bool
    read_skip_ws(xml_reader& reader) {
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::characters &&
            is_whitespace_only(reader.text()))
          continue;
        return true;
      }
      return false;
    }

    bool
    is_rng(const qname& name, const std::string& local) {
      return name.namespace_uri() == rng_ns && name.local_name() == local;
    }

    std::optional<std::string>
    opt_attr(xml_reader& reader, const std::string& local) {
      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        if (reader.attribute_name(i).local_name() == local &&
            reader.attribute_name(i).namespace_uri().empty()) {
          return std::string(reader.attribute_value(i));
        }
      }
      return std::nullopt;
    }

    std::string
    req_attr(xml_reader& reader, const std::string& local) {
      auto val = opt_attr(reader, local);
      if (!val.has_value()) {
        throw std::runtime_error("rng_parser: missing required attribute '" +
                                 local + "' on <" + reader.name().local_name() +
                                 ">");
      }
      return val.value();
    }

    void
    skip_element(xml_reader& reader) {
      std::size_t depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return;
        }
      }
    }

    // Read all text content of the current element (handles multiple text
    // nodes interleaved with whitespace).
    std::string
    read_text_content(xml_reader& reader) {
      std::string result;
      std::size_t depth = reader.depth();
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth) {
          return result;
        }
        if (reader.node_type() == xml_node_type::characters) {
          result += std::string(reader.text());
        }
      }
      return result;
    }

    // Forward declarations
    rng::pattern
    parse_pattern(xml_reader& reader, const std::string& datatype_library,
                  const std::string& ns);
    rng::name_class
    parse_name_class(xml_reader& reader, const std::string& ns);

    // Parse a name class from the current element position
    rng::name_class
    parse_name_class(xml_reader& reader, const std::string& ns) {
      const auto& name = reader.name();

      if (is_rng(name, "name")) {
        auto text = read_text_content(reader);
        // resolve ns from context
        return rng::name_class(rng::specific_name{ns, std::move(text)});
      }

      if (is_rng(name, "anyName")) {
        std::unique_ptr<rng::name_class> except;
        std::size_t depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == depth)
            break;
          if (reader.node_type() == xml_node_type::start_element) {
            if (is_rng(reader.name(), "except")) {
              // Parse the first name class inside except
              if (read_skip_ws(reader) &&
                  reader.node_type() == xml_node_type::start_element) {
                auto inner_ns = opt_attr(reader, "ns").value_or(ns);
                except =
                    rng::make_name_class(parse_name_class(reader, inner_ns));
              }
              // Skip to end of except
              skip_element(reader);
            } else {
              skip_element(reader);
            }
          }
        }
        return rng::name_class(rng::any_name_nc{std::move(except)});
      }

      if (is_rng(name, "nsName")) {
        auto ns_attr = opt_attr(reader, "ns").value_or(ns);
        std::unique_ptr<rng::name_class> except;
        std::size_t depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == depth)
            break;
          if (reader.node_type() == xml_node_type::start_element) {
            if (is_rng(reader.name(), "except")) {
              if (read_skip_ws(reader) &&
                  reader.node_type() == xml_node_type::start_element) {
                auto inner_ns = opt_attr(reader, "ns").value_or(ns);
                except =
                    rng::make_name_class(parse_name_class(reader, inner_ns));
              }
              skip_element(reader);
            } else {
              skip_element(reader);
            }
          }
        }
        return rng::name_class(
            rng::ns_name_nc{std::move(ns_attr), std::move(except)});
      }

      if (is_rng(name, "choice")) {
        std::vector<std::unique_ptr<rng::name_class>> children;
        std::size_t depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == depth)
            break;
          if (reader.node_type() == xml_node_type::start_element) {
            auto child_ns = opt_attr(reader, "ns").value_or(ns);
            children.push_back(
                rng::make_name_class(parse_name_class(reader, child_ns)));
          }
        }
        if (children.size() < 2) {
          throw std::runtime_error(
              "rng_parser: name class choice requires at least 2 children");
        }
        // Right-fold into binary tree
        auto result = std::move(children.back());
        for (int i = static_cast<int>(children.size()) - 2; i >= 0; --i) {
          result = rng::make_name_class(rng::choice_name_class{
              std::move(children[i]), std::move(result)});
        }
        return rng::name_class(std::move(*result));
      }

      throw std::runtime_error("rng_parser: unknown name class <" +
                               std::string(name.local_name()) + ">");
    }

    // Fold a vector of child patterns into a single binary pattern using the
    // given combinator constructor.
    template <typename MakeBinary>
    std::unique_ptr<rng::pattern>
    fold_children(std::vector<std::unique_ptr<rng::pattern>>& children,
                  MakeBinary make) {
      if (children.empty()) {
        throw std::runtime_error("rng_parser: combinator with no children");
      }
      if (children.size() == 1) return std::move(children[0]);

      auto result = std::move(children.back());
      for (int i = static_cast<int>(children.size()) - 2; i >= 0; --i) {
        result =
            rng::make_pattern(make(std::move(children[i]), std::move(result)));
      }
      return result;
    }

    // Parse children of a combinator element (group, interleave, choice, etc.)
    std::vector<std::unique_ptr<rng::pattern>>
    parse_children(xml_reader& reader, const std::string& dtlib,
                   const std::string& ns) {
      std::vector<std::unique_ptr<rng::pattern>> children;
      std::size_t depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          break;
        if (reader.node_type() == xml_node_type::start_element) {
          if (reader.name().namespace_uri() != rng_ns) {
            // Annotation — skip
            skip_element(reader);
            continue;
          }
          children.push_back(
              rng::make_pattern(parse_pattern(reader, dtlib, ns)));
        }
      }
      return children;
    }

    // Parse a single child pattern (for oneOrMore, zeroOrMore, etc.)
    std::unique_ptr<rng::pattern>
    parse_single_or_group(xml_reader& reader, const std::string& dtlib,
                          const std::string& ns) {
      auto children = parse_children(reader, dtlib, ns);
      if (children.empty()) return rng::make_pattern(rng::empty_pattern{});
      if (children.size() == 1) return std::move(children[0]);
      // Implicit group
      return fold_children(children, [](std::unique_ptr<rng::pattern> l,
                                        std::unique_ptr<rng::pattern> r) {
        return rng::group_pattern{std::move(l), std::move(r)};
      });
    }

    // Parse element or attribute pattern
    rng::pattern
    parse_element_or_attribute(xml_reader& reader, bool is_element,
                               const std::string& dtlib,
                               const std::string& ns) {
      auto name_attr = opt_attr(reader, "name");
      auto ns_attr = opt_attr(reader, "ns").value_or(ns);
      auto local_dtlib = opt_attr(reader, "datatypeLibrary").value_or(dtlib);

      std::unique_ptr<rng::name_class> nc_ptr;
      std::vector<std::unique_ptr<rng::pattern>> content_children;
      std::size_t depth = reader.depth();

      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          break;
        if (reader.node_type() == xml_node_type::start_element) {
          if (reader.name().namespace_uri() != rng_ns) {
            // Annotation — skip
            skip_element(reader);
            continue;
          }
          const auto& child_name = reader.name().local_name();
          // Name class children
          if (!name_attr.has_value() && !nc_ptr &&
              (child_name == "name" || child_name == "anyName" ||
               child_name == "nsName" || child_name == "choice")) {
            // Check if this is a name class or a pattern choice
            // If we haven't set name yet and it's a name class element
            if (child_name == "choice" && !name_attr.has_value() && !nc_ptr) {
              // Peek ahead — if children are name classes, it's a name
              // class choice. Otherwise it's a pattern choice. But we
              // can't really peek, so use a heuristic: if no name_attr
              // and first child element of element/attribute, treat as
              // name class.
              nc_ptr = rng::make_name_class(parse_name_class(reader, ns_attr));
            } else {
              nc_ptr = rng::make_name_class(parse_name_class(reader, ns_attr));
            }
          } else {
            content_children.push_back(
                rng::make_pattern(parse_pattern(reader, local_dtlib, ns_attr)));
          }
        }
      }

      // Build the name class
      rng::name_class nc = name_attr.has_value()
                               ? rng::name_class(rng::specific_name{
                                     ns_attr, std::move(name_attr.value())})
                               : (nc_ptr ? rng::name_class(std::move(*nc_ptr))
                                         : rng::name_class(rng::any_name_nc{}));

      // Build content
      std::unique_ptr<rng::pattern> content;
      if (content_children.empty()) {
        // Default: attribute defaults to text, element defaults to empty
        // Actually per RELAX NG spec, both default to <empty/> when no
        // content is specified. But attribute with no content means text.
        if (!is_element) {
          content = rng::make_pattern(rng::text_pattern{});
        } else {
          content = rng::make_pattern(rng::empty_pattern{});
        }
      } else if (content_children.size() == 1) {
        content = std::move(content_children[0]);
      } else {
        content = fold_children(
            content_children, [](std::unique_ptr<rng::pattern> l,
                                 std::unique_ptr<rng::pattern> r) {
              return rng::group_pattern{std::move(l), std::move(r)};
            });
      }

      if (is_element) {
        return rng::pattern(
            rng::element_pattern{std::move(nc), std::move(content)});
      }
      return rng::pattern(
          rng::attribute_pattern{std::move(nc), std::move(content)});
    }

    // Parse grammar components (start, define, include, div)
    void
    parse_grammar_content(xml_reader& reader, const std::string& dtlib,
                          const std::string& ns,
                          std::unique_ptr<rng::pattern>& start,
                          std::vector<rng::define>& defines,
                          std::vector<rng::include_directive>& includes) {
      std::size_t depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          break;
        if (reader.node_type() != xml_node_type::start_element) continue;

        if (reader.name().namespace_uri() != rng_ns) {
          skip_element(reader);
          continue;
        }

        const auto& child_name = reader.name().local_name();
        auto local_dtlib = opt_attr(reader, "datatypeLibrary").value_or(dtlib);
        auto local_ns = opt_attr(reader, "ns").value_or(ns);

        if (child_name == "start") {
          auto content = parse_single_or_group(reader, local_dtlib, local_ns);
          auto combine_attr = opt_attr(reader, "combine");
          // For simplicity, just store the start pattern (last one wins
          // for non-combine, or merge at simplification time)
          start = std::move(content);
        } else if (child_name == "define") {
          auto name = req_attr(reader, "name");
          auto combine_attr = opt_attr(reader, "combine");
          rng::combine_method cm = rng::combine_method::none;
          if (combine_attr.has_value()) {
            if (combine_attr.value() == "choice")
              cm = rng::combine_method::choice;
            else if (combine_attr.value() == "interleave")
              cm = rng::combine_method::interleave;
          }
          auto body = parse_single_or_group(reader, local_dtlib, local_ns);
          defines.push_back(rng::define{std::move(name), cm, std::move(body)});
        } else if (child_name == "include") {
          auto href = req_attr(reader, "href");
          auto inc_ns = opt_attr(reader, "ns").value_or(ns);

          // Parse override definitions inside include
          std::vector<rng::define> overrides;
          std::unique_ptr<rng::pattern> start_override;
          std::vector<rng::include_directive> nested_includes;
          parse_grammar_content(reader, local_dtlib, inc_ns, start_override,
                                overrides, nested_includes);

          includes.push_back(rng::include_directive{
              std::move(href), std::move(inc_ns), std::move(overrides),
              std::move(start_override)});
        } else if (child_name == "div") {
          // div is purely organizational — recurse to gather its children
          parse_grammar_content(reader, local_dtlib, local_ns, start, defines,
                                includes);
        } else {
          skip_element(reader);
        }
      }
    }

    rng::pattern
    parse_pattern(xml_reader& reader, const std::string& dtlib,
                  const std::string& ns) {
      const auto& name = reader.name();

      if (name.namespace_uri() != rng_ns) {
        throw std::runtime_error("rng_parser: unexpected element <" +
                                 std::string(name.local_name()) +
                                 "> in namespace " + name.namespace_uri());
      }

      const auto& local = name.local_name();
      auto local_dtlib = opt_attr(reader, "datatypeLibrary").value_or(dtlib);
      auto local_ns = opt_attr(reader, "ns").value_or(ns);

      // Leaf patterns
      if (local == "empty") {
        skip_element(reader);
        return rng::pattern(rng::empty_pattern{});
      }
      if (local == "text") {
        skip_element(reader);
        return rng::pattern(rng::text_pattern{});
      }
      if (local == "notAllowed") {
        skip_element(reader);
        return rng::pattern(rng::not_allowed_pattern{});
      }

      // References
      if (local == "ref") {
        auto ref_name = req_attr(reader, "name");
        skip_element(reader);
        return rng::pattern(rng::ref_pattern{std::move(ref_name)});
      }
      if (local == "parentRef") {
        auto ref_name = req_attr(reader, "name");
        skip_element(reader);
        return rng::pattern(rng::parent_ref_pattern{std::move(ref_name)});
      }

      // Element / attribute
      if (local == "element") {
        return parse_element_or_attribute(reader, true, local_dtlib, local_ns);
      }
      if (local == "attribute") {
        return parse_element_or_attribute(reader, false, local_dtlib, local_ns);
      }

      // Combinators
      if (local == "group") {
        auto children = parse_children(reader, local_dtlib, local_ns);
        return rng::pattern(std::move(
            *fold_children(children, [](std::unique_ptr<rng::pattern> l,
                                        std::unique_ptr<rng::pattern> r) {
              return rng::group_pattern{std::move(l), std::move(r)};
            })));
      }
      if (local == "interleave") {
        auto children = parse_children(reader, local_dtlib, local_ns);
        return rng::pattern(std::move(
            *fold_children(children, [](std::unique_ptr<rng::pattern> l,
                                        std::unique_ptr<rng::pattern> r) {
              return rng::interleave_pattern{std::move(l), std::move(r)};
            })));
      }
      if (local == "choice") {
        auto children = parse_children(reader, local_dtlib, local_ns);
        return rng::pattern(std::move(
            *fold_children(children, [](std::unique_ptr<rng::pattern> l,
                                        std::unique_ptr<rng::pattern> r) {
              return rng::choice_pattern{std::move(l), std::move(r)};
            })));
      }

      // Occurrence
      if (local == "oneOrMore") {
        auto content = parse_single_or_group(reader, local_dtlib, local_ns);
        return rng::pattern(rng::one_or_more_pattern{std::move(content)});
      }
      if (local == "zeroOrMore") {
        auto content = parse_single_or_group(reader, local_dtlib, local_ns);
        return rng::pattern(rng::zero_or_more_pattern{std::move(content)});
      }
      if (local == "optional") {
        auto content = parse_single_or_group(reader, local_dtlib, local_ns);
        return rng::pattern(rng::optional_pattern{std::move(content)});
      }
      if (local == "mixed") {
        auto content = parse_single_or_group(reader, local_dtlib, local_ns);
        return rng::pattern(rng::mixed_pattern{std::move(content)});
      }

      // Data / value / list
      if (local == "data") {
        auto type = req_attr(reader, "type");
        std::vector<rng::data_param> params;
        std::unique_ptr<rng::pattern> except;

        std::size_t depth = reader.depth();
        while (read_skip_ws(reader)) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == depth)
            break;
          if (reader.node_type() == xml_node_type::start_element) {
            if (is_rng(reader.name(), "param")) {
              auto pname = req_attr(reader, "name");
              auto pvalue = read_text_content(reader);
              params.push_back(
                  rng::data_param{std::move(pname), std::move(pvalue)});
            } else if (is_rng(reader.name(), "except")) {
              auto children = parse_children(reader, local_dtlib, local_ns);
              if (!children.empty()) { except = std::move(children[0]); }
            } else {
              skip_element(reader);
            }
          }
        }
        return rng::pattern(rng::data_pattern{local_dtlib, std::move(type),
                                              std::move(params),
                                              std::move(except)});
      }

      if (local == "value") {
        auto type = opt_attr(reader, "type").value_or("token");
        auto val = read_text_content(reader);
        return rng::pattern(rng::value_pattern{local_dtlib, std::move(type),
                                               std::move(val), local_ns});
      }

      if (local == "list") {
        auto content = parse_single_or_group(reader, local_dtlib, local_ns);
        return rng::pattern(rng::list_pattern{std::move(content)});
      }

      // External ref
      if (local == "externalRef") {
        auto href = req_attr(reader, "href");
        skip_element(reader);
        return rng::pattern(
            rng::external_ref_pattern{std::move(href), local_ns});
      }

      // Grammar
      if (local == "grammar") {
        std::unique_ptr<rng::pattern> start;
        std::vector<rng::define> defines;
        std::vector<rng::include_directive> includes;
        parse_grammar_content(reader, local_dtlib, local_ns, start, defines,
                              includes);
        return rng::pattern(rng::grammar_pattern{
            std::move(start), std::move(defines), std::move(includes)});
      }

      throw std::runtime_error("rng_parser: unknown element <" +
                               std::string(local) + ">");
    }

  } // namespace

  rng::pattern
  rng_xml_parser::parse(xml_reader& reader) {
    // Advance to the first element
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) {
        auto dtlib = opt_attr(reader, "datatypeLibrary").value_or("");
        auto ns = opt_attr(reader, "ns").value_or("");
        return parse_pattern(reader, dtlib, ns);
      }
    }
    throw std::runtime_error("rng_parser: no root element found");
  }

} // namespace xb
