#include <xb/schematron_parser.hpp>

#include <stdexcept>

namespace xb {

  namespace {

    const std::string sch_ns = "http://purl.oclc.org/dml/schematron";

    bool
    is_sch_element(const qname& name, const std::string& local) {
      return name.namespace_uri() == sch_ns && name.local_name() == local;
    }

    // Skip whitespace-only text nodes
    bool
    read_skip_ws(xml_reader& reader) {
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::characters) {
          // Check if all whitespace
          auto text = reader.text();
          bool all_ws = true;
          for (char c : text) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
              all_ws = false;
              break;
            }
          }
          if (all_ws) continue;
        }
        return true;
      }
      return false;
    }

    std::string
    opt_attr(xml_reader& reader, const std::string& name) {
      qname attr_name("", name);
      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        if (reader.attribute_name(i).local_name() == name) {
          return std::string(reader.attribute_value(i));
        }
      }
      return "";
    }

    // Read all text content under the current element
    std::string
    read_text_content(xml_reader& reader) {
      std::string result;
      auto start_depth = reader.depth();
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == start_depth) {
          return result;
        }
        if (reader.node_type() == xml_node_type::characters) {
          result += reader.text();
        }
      }
      return result;
    }

    // Skip an element and all its children
    void
    skip_element(xml_reader& reader) {
      auto depth = reader.depth();
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == depth)
          return;
      }
    }

    schematron::assert_or_report
    parse_assert_or_report(xml_reader& reader, bool is_assert) {
      schematron::assert_or_report ar;
      ar.is_assert = is_assert;
      ar.test = opt_attr(reader, "test");
      ar.diagnostics = opt_attr(reader, "diagnostics");
      ar.message = read_text_content(reader);
      return ar;
    }

    schematron::rule
    parse_rule(xml_reader& reader) {
      schematron::rule r;
      r.context = opt_attr(reader, "context");

      auto rule_depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == rule_depth) {
          return r;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_sch_element(reader.name(), "assert")) {
            r.checks.push_back(parse_assert_or_report(reader, true));
          } else if (is_sch_element(reader.name(), "report")) {
            r.checks.push_back(parse_assert_or_report(reader, false));
          } else {
            skip_element(reader);
          }
        }
      }
      return r;
    }

    schematron::pattern
    parse_pattern(xml_reader& reader) {
      schematron::pattern p;
      p.id = opt_attr(reader, "id");
      p.name = opt_attr(reader, "name");

      auto pat_depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == pat_depth) {
          return p;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_sch_element(reader.name(), "rule")) {
            p.rules.push_back(parse_rule(reader));
          } else {
            skip_element(reader);
          }
        }
      }
      return p;
    }

    schematron::phase
    parse_phase(xml_reader& reader) {
      schematron::phase ph;
      ph.id = opt_attr(reader, "id");

      auto phase_depth = reader.depth();
      while (read_skip_ws(reader)) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == phase_depth) {
          return ph;
        }
        if (reader.node_type() == xml_node_type::start_element) {
          if (is_sch_element(reader.name(), "active")) {
            ph.active_patterns.push_back(opt_attr(reader, "pattern"));
            skip_element(reader);
          } else {
            skip_element(reader);
          }
        }
      }
      return ph;
    }

  } // namespace

  schematron::schema
  schematron_parser::parse(xml_reader& reader) {
    schematron::schema result;

    // Advance to root element
    while (reader.read()) {
      if (reader.node_type() == xml_node_type::start_element) break;
    }

    if (!is_sch_element(reader.name(), "schema")) {
      throw std::runtime_error(
          "schematron_parser: expected <sch:schema> root element");
    }

    auto root_depth = reader.depth();
    while (read_skip_ws(reader)) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == root_depth) {
        return result;
      }
      if (reader.node_type() == xml_node_type::start_element) {
        if (is_sch_element(reader.name(), "title")) {
          result.title = read_text_content(reader);
        } else if (is_sch_element(reader.name(), "ns")) {
          schematron::namespace_binding ns;
          ns.prefix = opt_attr(reader, "prefix");
          ns.uri = opt_attr(reader, "uri");
          result.namespaces.push_back(std::move(ns));
          skip_element(reader);
        } else if (is_sch_element(reader.name(), "pattern")) {
          result.patterns.push_back(parse_pattern(reader));
        } else if (is_sch_element(reader.name(), "phase")) {
          result.phases.push_back(parse_phase(reader));
        } else if (is_sch_element(reader.name(), "diagnostics")) {
          skip_element(reader); // diagnostics stored but not parsed yet
        } else {
          skip_element(reader);
        }
      }
    }

    return result;
  }

} // namespace xb
