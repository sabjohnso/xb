#include <xb/xsd_writer.hpp>

#include <xb/ostream_writer.hpp>

#include <sstream>
#include <string>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    struct write_ctx {
      xml_writer& w;
      const schema& s;
      bool ns_declared = false;

      void
      start_xs(const std::string& local) {
        w.start_element(qname(xs_ns, local));
        if (!ns_declared) {
          w.namespace_declaration("xs", xs_ns);
          if (!s.target_namespace().empty()) {
            w.namespace_declaration("tns", s.target_namespace());
          }
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

      // Format a qname as a prefixed string for attribute values
      std::string
      type_ref(const qname& name) {
        if (name.namespace_uri() == xs_ns) { return "xs:" + name.local_name(); }
        if (!s.target_namespace().empty() &&
            name.namespace_uri() == s.target_namespace()) {
          return "tns:" + name.local_name();
        }
        // Unprefixed (empty namespace or same-namespace without tns)
        return name.local_name();
      }
    };

    void
    write_model_group(const model_group& mg, write_ctx& ctx);
    void
    write_particle(const particle& p, write_ctx& ctx);

    void
    write_occurrence(const occurrence& occ, write_ctx& ctx) {
      if (occ.min_occurs != 1) {
        ctx.attr("minOccurs", std::to_string(occ.min_occurs));
      }
      if (occ.is_unbounded()) {
        ctx.attr("maxOccurs", "unbounded");
      } else if (occ.max_occurs != 1) {
        ctx.attr("maxOccurs", std::to_string(occ.max_occurs));
      }
    }

    void
    write_wildcard_ns(const wildcard& wc, write_ctx& ctx) {
      switch (wc.ns_constraint) {
        case wildcard_ns_constraint::any:
          ctx.attr("namespace", "##any");
          break;
        case wildcard_ns_constraint::other:
          ctx.attr("namespace", "##other");
          break;
        case wildcard_ns_constraint::enumerated: {
          std::string ns_list;
          for (const auto& ns : wc.namespaces) {
            if (!ns_list.empty()) ns_list += ' ';
            ns_list += ns;
          }
          if (!ns_list.empty()) ctx.attr("namespace", ns_list);
          break;
        }
      }
    }

    void
    write_process_contents(const wildcard& wc, write_ctx& ctx) {
      switch (wc.process) {
        case process_contents::strict:
          break; // default, omit
        case process_contents::lax:
          ctx.attr("processContents", "lax");
          break;
        case process_contents::skip:
          ctx.attr("processContents", "skip");
          break;
      }
    }

    void
    write_facets(const facet_set& facets, write_ctx& ctx) {
      for (const auto& e : facets.enumeration) {
        ctx.start_xs("enumeration");
        ctx.attr("value", e);
        ctx.end();
      }
      if (facets.pattern) {
        ctx.start_xs("pattern");
        ctx.attr("value", *facets.pattern);
        ctx.end();
      }
      if (facets.min_inclusive) {
        ctx.start_xs("minInclusive");
        ctx.attr("value", *facets.min_inclusive);
        ctx.end();
      }
      if (facets.max_inclusive) {
        ctx.start_xs("maxInclusive");
        ctx.attr("value", *facets.max_inclusive);
        ctx.end();
      }
      if (facets.min_exclusive) {
        ctx.start_xs("minExclusive");
        ctx.attr("value", *facets.min_exclusive);
        ctx.end();
      }
      if (facets.max_exclusive) {
        ctx.start_xs("maxExclusive");
        ctx.attr("value", *facets.max_exclusive);
        ctx.end();
      }
      if (facets.length) {
        ctx.start_xs("length");
        ctx.attr("value", std::to_string(*facets.length));
        ctx.end();
      }
      if (facets.min_length) {
        ctx.start_xs("minLength");
        ctx.attr("value", std::to_string(*facets.min_length));
        ctx.end();
      }
      if (facets.max_length) {
        ctx.start_xs("maxLength");
        ctx.attr("value", std::to_string(*facets.max_length));
        ctx.end();
      }
      if (facets.total_digits) {
        ctx.start_xs("totalDigits");
        ctx.attr("value", std::to_string(*facets.total_digits));
        ctx.end();
      }
      if (facets.fraction_digits) {
        ctx.start_xs("fractionDigits");
        ctx.attr("value", std::to_string(*facets.fraction_digits));
        ctx.end();
      }
    }

    void
    write_attribute_use(const attribute_use& au, write_ctx& ctx) {
      ctx.start_xs("attribute");
      ctx.attr("name", au.name.local_name());
      if (!au.type_name.local_name().empty()) {
        ctx.attr("type", ctx.type_ref(au.type_name));
      }
      if (au.required) { ctx.attr("use", "required"); }
      if (au.default_value) { ctx.attr("default", *au.default_value); }
      if (au.fixed_value) { ctx.attr("fixed", *au.fixed_value); }
      ctx.end();
    }

    void
    write_attributes(const complex_type& ct, write_ctx& ctx) {
      for (const auto& au : ct.attributes()) {
        write_attribute_use(au, ctx);
      }
      for (const auto& agr : ct.attribute_group_refs()) {
        ctx.start_xs("attributeGroup");
        ctx.attr("ref", ctx.type_ref(agr.ref));
        ctx.end();
      }
      if (ct.attribute_wildcard()) {
        const auto& aw = *ct.attribute_wildcard();
        ctx.start_xs("anyAttribute");
        write_wildcard_ns(aw, ctx);
        write_process_contents(aw, ctx);
        ctx.end();
      }
    }

    void
    write_particle(const particle& p, write_ctx& ctx) {
      std::visit(
          [&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, element_decl>) {
              ctx.start_xs("element");
              ctx.attr("name", term.name().local_name());
              if (!term.type_name().local_name().empty()) {
                ctx.attr("type", ctx.type_ref(term.type_name()));
              }
              write_occurrence(p.occurs, ctx);
              if (term.nillable()) ctx.attr("nillable", "true");
              ctx.end();
            } else if constexpr (std::is_same_v<T, element_ref>) {
              ctx.start_xs("element");
              ctx.attr("ref", ctx.type_ref(term.ref));
              write_occurrence(p.occurs, ctx);
              ctx.end();
            } else if constexpr (std::is_same_v<T, group_ref>) {
              ctx.start_xs("group");
              ctx.attr("ref", ctx.type_ref(term.ref));
              write_occurrence(p.occurs, ctx);
              ctx.end();
            } else if constexpr (std::is_same_v<T,
                                                std::unique_ptr<model_group>>) {
              if (term) { write_model_group(*term, ctx); }
            } else if constexpr (std::is_same_v<T, wildcard>) {
              ctx.start_xs("any");
              write_wildcard_ns(term, ctx);
              write_process_contents(term, ctx);
              write_occurrence(p.occurs, ctx);
              ctx.end();
            }
          },
          p.term);
    }

    void
    write_model_group(const model_group& mg, write_ctx& ctx) {
      const char* elem = "sequence";
      switch (mg.compositor()) {
        case compositor_kind::sequence:
          elem = "sequence";
          break;
        case compositor_kind::choice:
          elem = "choice";
          break;
        case compositor_kind::all:
        case compositor_kind::interleave:
          elem = "all";
          break;
      }
      ctx.start_xs(elem);
      for (const auto& p : mg.particles()) {
        write_particle(p, ctx);
      }
      ctx.end();
    }

    void
    write_simple_type(const simple_type& st, write_ctx& ctx) {
      ctx.start_xs("simpleType");
      if (!st.name().local_name().empty()) {
        ctx.attr("name", st.name().local_name());
      }

      switch (st.variety()) {
        case simple_type_variety::atomic: {
          ctx.start_xs("restriction");
          ctx.attr("base", ctx.type_ref(st.base_type_name()));
          write_facets(st.facets(), ctx);
          ctx.end();
          break;
        }
        case simple_type_variety::list: {
          ctx.start_xs("list");
          if (st.item_type_name()) {
            ctx.attr("itemType", ctx.type_ref(*st.item_type_name()));
          }
          ctx.end();
          break;
        }
        case simple_type_variety::union_type: {
          ctx.start_xs("union");
          if (!st.member_type_names().empty()) {
            std::string members;
            for (const auto& m : st.member_type_names()) {
              if (!members.empty()) members += ' ';
              members += ctx.type_ref(m);
            }
            ctx.attr("memberTypes", members);
          }
          ctx.end();
          break;
        }
      }

      ctx.end(); // simpleType
    }

    void
    write_complex_type(const complex_type& ct, write_ctx& ctx) {
      ctx.start_xs("complexType");
      if (!ct.name().local_name().empty()) {
        ctx.attr("name", ct.name().local_name());
      }
      if (ct.abstract()) ctx.attr("abstract", "true");
      if (ct.mixed()) ctx.attr("mixed", "true");

      const auto& content = ct.content();

      if (content.kind == content_kind::simple) {
        // Simple content
        auto* sc = std::get_if<simple_content>(&content.detail);
        if (sc) {
          ctx.start_xs("simpleContent");
          const char* deriv = (sc->derivation == derivation_method::extension)
                                  ? "extension"
                                  : "restriction";
          ctx.start_xs(deriv);
          ctx.attr("base", ctx.type_ref(sc->base_type_name));
          write_facets(sc->facets, ctx);
          // Attributes go inside the derivation element for simpleContent
          write_attributes(ct, ctx);
          ctx.end(); // extension/restriction
          ctx.end(); // simpleContent
        }
      } else if (content.kind == content_kind::element_only ||
                 content.kind == content_kind::mixed) {
        auto* cc = std::get_if<complex_content>(&content.detail);
        if (cc) {
          bool has_base = !cc->base_type_name.local_name().empty();
          if (has_base) {
            // Explicit complexContent with derivation
            ctx.start_xs("complexContent");
            const char* deriv = (cc->derivation == derivation_method::extension)
                                    ? "extension"
                                    : "restriction";
            ctx.start_xs(deriv);
            ctx.attr("base", ctx.type_ref(cc->base_type_name));
            if (cc->content_model) {
              write_model_group(*cc->content_model, ctx);
            }
            write_attributes(ct, ctx);
            ctx.end(); // extension/restriction
            ctx.end(); // complexContent
          } else {
            // Inline content model (no derivation)
            if (cc->content_model) {
              write_model_group(*cc->content_model, ctx);
            }
            write_attributes(ct, ctx);
          }
        }
      } else if (content.kind == content_kind::empty) {
        // Empty content — just write attributes if any
        write_attributes(ct, ctx);
      }

      ctx.end(); // complexType
    }

    void
    write_element_decl(const element_decl& e, write_ctx& ctx) {
      ctx.start_xs("element");
      ctx.attr("name", e.name().local_name());
      if (!e.type_name().local_name().empty()) {
        ctx.attr("type", ctx.type_ref(e.type_name()));
      }
      if (e.nillable()) ctx.attr("nillable", "true");
      if (e.abstract()) ctx.attr("abstract", "true");
      if (e.default_value()) ctx.attr("default", *e.default_value());
      if (e.fixed_value()) ctx.attr("fixed", *e.fixed_value());
      if (e.substitution_group()) {
        ctx.attr("substitutionGroup", ctx.type_ref(*e.substitution_group()));
      }
      ctx.end();
    }

    void
    write_model_group_def(const model_group_def& mgd, write_ctx& ctx) {
      ctx.start_xs("group");
      ctx.attr("name", mgd.name().local_name());
      write_model_group(mgd.group(), ctx);
      ctx.end();
    }

    void
    write_attribute_group_def(const attribute_group_def& agd, write_ctx& ctx) {
      ctx.start_xs("attributeGroup");
      ctx.attr("name", agd.name().local_name());
      for (const auto& au : agd.attributes()) {
        write_attribute_use(au, ctx);
      }
      for (const auto& agr : agd.attribute_group_refs()) {
        ctx.start_xs("attributeGroup");
        ctx.attr("ref", ctx.type_ref(agr.ref));
        ctx.end();
      }
      if (agd.attribute_wildcard()) {
        const auto& aw = *agd.attribute_wildcard();
        ctx.start_xs("anyAttribute");
        write_wildcard_ns(aw, ctx);
        write_process_contents(aw, ctx);
        ctx.end();
      }
      ctx.end();
    }

  } // namespace

  void
  xsd_write(const schema& s, xml_writer& writer) {
    write_ctx ctx{writer, s};

    ctx.start_xs("schema");
    if (!s.target_namespace().empty()) {
      ctx.attr("targetNamespace", s.target_namespace());
    }

    for (const auto& imp : s.imports()) {
      ctx.start_xs("import");
      if (!imp.namespace_uri.empty()) {
        ctx.attr("namespace", imp.namespace_uri);
      }
      if (!imp.schema_location.empty()) {
        ctx.attr("schemaLocation", imp.schema_location);
      }
      ctx.end();
    }

    for (const auto& inc : s.includes()) {
      ctx.start_xs("include");
      ctx.attr("schemaLocation", inc.schema_location);
      ctx.end();
    }

    for (const auto& st : s.simple_types()) {
      write_simple_type(st, ctx);
    }

    for (const auto& ct : s.complex_types()) {
      write_complex_type(ct, ctx);
    }

    for (const auto& mgd : s.model_group_defs()) {
      write_model_group_def(mgd, ctx);
    }

    for (const auto& agd : s.attribute_group_defs()) {
      write_attribute_group_def(agd, ctx);
    }

    for (const auto& e : s.elements()) {
      write_element_decl(e, ctx);
    }

    ctx.end(); // schema
  }

  std::string
  xsd_write_string(const schema& s) {
    std::ostringstream os;
    ostream_writer writer(os);
    writer.xml_declaration();
    xsd_write(s, writer);
    return os.str();
  }

  std::string
  xsd_write_string(const schema& s, int indent) {
    std::ostringstream os;
    ostream_writer writer(os, indent);
    writer.xml_declaration();
    xsd_write(s, writer);
    return os.str();
  }

} // namespace xb
