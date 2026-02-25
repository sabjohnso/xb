#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/doc_generator.hpp>

#include <xb/complex_type.hpp>
#include <xb/simple_type.hpp>

#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace xb {

  namespace {

    const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema";

    std::string
    default_value_for_builtin(const std::string& local_name) {
      if (local_name == "string" || local_name == "normalizedString" ||
          local_name == "token" || local_name == "Name" ||
          local_name == "NCName" || local_name == "NMTOKEN" ||
          local_name == "QName")
        return "string";
      if (local_name == "boolean") return "true";
      if (local_name == "int" || local_name == "integer" ||
          local_name == "long" || local_name == "short" ||
          local_name == "byte" || local_name == "unsignedInt" ||
          local_name == "unsignedLong" || local_name == "unsignedShort" ||
          local_name == "unsignedByte" || local_name == "nonNegativeInteger" ||
          local_name == "nonPositiveInteger")
        return "0";
      if (local_name == "positiveInteger") return "1";
      if (local_name == "negativeInteger") return "-1";
      if (local_name == "decimal" || local_name == "float" ||
          local_name == "double")
        return "0.0";
      if (local_name == "date") return "2000-01-01";
      if (local_name == "dateTime") return "2000-01-01T00:00:00";
      if (local_name == "time") return "00:00:00";
      if (local_name == "duration") return "PT0S";
      if (local_name == "base64Binary" || local_name == "hexBinary") return "";
      if (local_name == "anyURI") return "http://example.com";
      if (local_name == "language") return "en";
      if (local_name == "ID") return "id1";
      if (local_name == "IDREF") return "id1";
      if (local_name == "gYear") return "2000";
      if (local_name == "gYearMonth") return "2000-01";
      if (local_name == "gMonth") return "--01";
      if (local_name == "gMonthDay") return "--01-01";
      if (local_name == "gDay") return "---01";
      if (local_name == "anySimpleType" || local_name == "anyType")
        return "string";
      return "0";
    }

    bool
    is_xsd_builtin(const qname& type_name) {
      return type_name.namespace_uri() == xsd_ns;
    }

    class generator_impl {
      const schema_set& schemas_;
      doc_generator_options options_;
      xml_writer* writer_ = nullptr;

      // Namespace prefix management
      mutable std::unordered_map<std::string, std::string> ns_prefixes_;
      mutable std::size_t prefix_counter_ = 0;

      // Recursion tracking
      mutable std::size_t depth_ = 0;
      mutable std::set<qname> type_stack_;

    public:
      generator_impl(const schema_set& schemas, doc_generator_options options)
          : schemas_(schemas), options_(options) {}

      void
      generate(const qname& element_name, xml_writer& writer) {
        writer_ = &writer;
        ns_prefixes_.clear();
        prefix_counter_ = 0;
        depth_ = 0;
        type_stack_.clear();

        const auto* elem = schemas_.find_element(element_name);
        if (!elem)
          throw std::runtime_error("element not found: " +
                                   element_name.local_name());

        write_element(*elem);
      }

    private:
      void
      ensure_namespace(const qname& name) {
        const auto& uri = name.namespace_uri();
        if (uri.empty()) return;
        if (ns_prefixes_.count(uri)) return;

        // First namespace uses default (empty prefix), subsequent get ns0,
        // ns1, ...
        std::string prefix;
        if (ns_prefixes_.empty()) {
          prefix = "";
        } else {
          prefix = "ns" + std::to_string(prefix_counter_++);
        }
        ns_prefixes_[uri] = prefix;
        writer_->namespace_declaration(prefix, uri);
      }

      void
      write_element(const element_decl& elem) {
        // Handle abstract elements: find first concrete substitution member
        if (elem.abstract()) {
          const auto* concrete = find_concrete_substitution(elem.name());
          if (concrete) {
            write_element(*concrete);
            return;
          }
          // No concrete substitution found; emit empty element
        }

        writer_->start_element(elem.name());
        ensure_namespace(elem.name());

        if (elem.fixed_value()) {
          writer_->characters(*elem.fixed_value());
        } else if (elem.default_value()) {
          writer_->characters(*elem.default_value());
        } else {
          write_type_content(elem.type_name());
        }

        writer_->end_element();
      }

      const element_decl*
      find_concrete_substitution(const qname& abstract_name) const {
        for (const auto& s : schemas_.schemas()) {
          for (const auto& e : s.elements()) {
            if (e.substitution_group() &&
                *e.substitution_group() == abstract_name && !e.abstract()) {
              return &e;
            }
          }
        }
        return nullptr;
      }

      void
      write_type_content(const qname& type_name) {
        if (is_xsd_builtin(type_name)) {
          writer_->characters(
              default_value_for_builtin(type_name.local_name()));
          return;
        }

        // Check for complex type
        const auto* ct = schemas_.find_complex_type(type_name);
        if (ct) {
          write_complex_type_content(*ct);
          return;
        }

        // Check for simple type
        const auto* st = schemas_.find_simple_type(type_name);
        if (st) {
          write_simple_type_value(*st);
          return;
        }

        // Unknown type: emit empty
      }

      void
      write_complex_type_content(const complex_type& ct) {
        // Depth/recursion check
        if (depth_ >= options_.max_depth || type_stack_.count(ct.name()))
          return;

        depth_++;
        type_stack_.insert(ct.name());

        // Emit attributes
        write_attributes(ct);

        // Content dispatch
        switch (ct.content().kind) {
          case content_kind::empty:
            break;

          case content_kind::simple: {
            const auto* sc = std::get_if<simple_content>(&ct.content().detail);
            if (sc) write_simple_content_value(*sc);
            break;
          }

          case content_kind::element_only:
          case content_kind::mixed: {
            const auto* cc = std::get_if<complex_content>(&ct.content().detail);
            if (cc) {
              // If extension, emit base type's content model first
              if (cc->derivation == derivation_method::extension) {
                write_base_type_content(cc->base_type_name);
              }
              // Emit own content model
              if (cc->content_model) write_model_group(*cc->content_model);
            }
            break;
          }
        }

        type_stack_.erase(ct.name());
        depth_--;
      }

      void
      write_base_type_content(const qname& base_name) {
        if (is_xsd_builtin(base_name)) return;
        const auto* base_ct = schemas_.find_complex_type(base_name);
        if (!base_ct) return;

        // Emit base type's content (without incrementing depth for the
        // extension chain, but we need its content model)
        const auto* cc =
            std::get_if<complex_content>(&base_ct->content().detail);
        if (cc) {
          if (cc->derivation == derivation_method::extension)
            write_base_type_content(cc->base_type_name);
          if (cc->content_model) write_model_group(*cc->content_model);
        }
      }

      void
      write_attributes(const complex_type& ct) {
        // Direct attributes
        for (const auto& attr : ct.attributes()) {
          if (!attr.required && !options_.populate_optional) continue;

          qname attr_name = attr.name;
          std::string value;
          if (attr.fixed_value)
            value = *attr.fixed_value;
          else if (attr.default_value)
            value = *attr.default_value;
          else
            value = default_value_for_type(attr.type_name);

          writer_->attribute(attr_name, value);
        }

        // Attribute group refs
        for (const auto& agr : ct.attribute_group_refs()) {
          write_attribute_group(agr.ref);
        }
      }

      void
      write_attribute_group(const qname& group_name) {
        const auto* agd = schemas_.find_attribute_group_def(group_name);
        if (!agd) return;

        for (const auto& attr : agd->attributes()) {
          if (!attr.required && !options_.populate_optional) continue;

          std::string value;
          if (attr.fixed_value)
            value = *attr.fixed_value;
          else if (attr.default_value)
            value = *attr.default_value;
          else
            value = default_value_for_type(attr.type_name);

          writer_->attribute(attr.name, value);
        }

        // Recursive attribute group refs
        for (const auto& agr : agd->attribute_group_refs())
          write_attribute_group(agr.ref);
      }

      std::string
      default_value_for_type(const qname& type_name) const {
        if (is_xsd_builtin(type_name))
          return default_value_for_builtin(type_name.local_name());

        const auto* st = schemas_.find_simple_type(type_name);
        if (st) return resolve_simple_type_value(*st);

        return "string";
      }

      void
      write_simple_content_value(const simple_content& sc) {
        // Walk the base type chain to find a built-in
        qname current = sc.base_type_name;
        while (!is_xsd_builtin(current)) {
          const auto* st = schemas_.find_simple_type(current);
          if (!st) break;
          // Check facets on the simple type
          if (!st->facets().enumeration.empty()) {
            writer_->characters(st->facets().enumeration.front());
            return;
          }
          current = st->base_type_name();
        }
        if (is_xsd_builtin(current))
          writer_->characters(default_value_for_builtin(current.local_name()));
      }

      void
      write_simple_type_value(const simple_type& st) {
        writer_->characters(resolve_simple_type_value(st));
      }

      std::string
      resolve_simple_type_value(const simple_type& st) const {
        // Check facets: enumeration first
        if (!st.facets().enumeration.empty())
          return st.facets().enumeration.front();

        // min_inclusive
        if (st.facets().min_inclusive) return *st.facets().min_inclusive;

        // length / min_length
        if (st.facets().length) return std::string(*st.facets().length, 'a');
        if (st.facets().min_length)
          return std::string(*st.facets().min_length, 'a');

        // Walk base type chain
        qname current = st.base_type_name();
        while (!is_xsd_builtin(current)) {
          const auto* base = schemas_.find_simple_type(current);
          if (!base) break;
          if (!base->facets().enumeration.empty())
            return base->facets().enumeration.front();
          if (base->facets().min_inclusive)
            return *base->facets().min_inclusive;
          current = base->base_type_name();
        }
        if (is_xsd_builtin(current))
          return default_value_for_builtin(current.local_name());
        return "string";
      }

      void
      write_model_group(const model_group& group) {
        switch (group.compositor()) {
          case compositor_kind::sequence:
          case compositor_kind::all:
            for (const auto& p : group.particles())
              write_particle(p);
            break;

          case compositor_kind::choice:
            if (!group.particles().empty())
              write_particle(group.particles().front());
            break;
        }
      }

      void
      write_particle(const particle& p) {
        std::size_t count = p.occurs.min_occurs;
        if (count == 0 && options_.populate_optional) count = 1;
        if (count == 0) return;

        for (std::size_t i = 0; i < count; ++i) {
          std::visit(
              [this](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, element_decl>) {
                  write_element(term);
                } else if constexpr (std::is_same_v<T, element_ref>) {
                  const auto* resolved = schemas_.find_element(term.ref);
                  if (resolved) write_element(*resolved);
                } else if constexpr (std::is_same_v<T, group_ref>) {
                  const auto* g = schemas_.find_model_group_def(term.ref);
                  if (g) write_model_group(g->group());
                } else if constexpr (std::is_same_v<
                                         T, std::unique_ptr<model_group>>) {
                  if (term) write_model_group(*term);
                }
                // wildcard: skip
              },
              p.term);
        }
      }
    };

  } // namespace

  doc_generator::doc_generator(const schema_set& schemas,
                               doc_generator_options options)
      : schemas_(schemas), options_(options) {}

  void
  doc_generator::generate(const qname& element_name, xml_writer& writer) const {
    generator_impl impl(schemas_, options_);
    impl.generate(element_name, writer);
  }

} // namespace xb
