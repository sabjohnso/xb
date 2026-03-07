#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/schema_to_dtd.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";

    struct translator {
      const schema_set& schemas;
      std::vector<dtd::element_decl> elements;
      std::vector<dtd::attlist_decl> attlists;
      std::unordered_set<std::string> emitted;

      explicit translator(const schema_set& ss) : schemas(ss) {}

      // Map XSD type to DTD attribute type
      dtd::attribute_type
      xsd_to_dtd_attr_type(const qname& type_name) {
        if (type_name.namespace_uri() != xs_ns)
          return dtd::attribute_type::cdata;
        auto& ln = type_name.local_name();
        if (ln == "ID") return dtd::attribute_type::id;
        if (ln == "IDREF") return dtd::attribute_type::idref;
        if (ln == "IDREFS") return dtd::attribute_type::idrefs;
        if (ln == "NMTOKEN") return dtd::attribute_type::nmtoken;
        if (ln == "NMTOKENS") return dtd::attribute_type::nmtokens;
        if (ln == "ENTITY") return dtd::attribute_type::entity;
        if (ln == "ENTITIES") return dtd::attribute_type::entities;
        if (ln == "NOTATION") return dtd::attribute_type::notation;
        return dtd::attribute_type::cdata;
      }

      // Check if a simple type is an enumeration and extract its values
      bool
      get_enumeration(const qname& type_name,
                      std::vector<std::string>& values) {
        auto* st = schemas.find_simple_type(type_name);
        if (!st) return false;
        if (st->facets().enumeration.empty()) return false;
        values = st->facets().enumeration;
        return true;
      }

      // Build DTD attribute_def from XSD attribute_use
      dtd::attribute_def
      translate_attribute(const attribute_use& au) {
        dtd::attribute_def ad;
        ad.name = au.name.local_name();

        // Check for enumeration type
        std::vector<std::string> enum_vals;
        if (get_enumeration(au.type_name, enum_vals)) {
          ad.type = dtd::attribute_type::enumeration;
          ad.enum_values = std::move(enum_vals);
        } else {
          ad.type = xsd_to_dtd_attr_type(au.type_name);
        }

        if (au.fixed_value) {
          ad.dflt = dtd::default_kind::fixed;
          ad.default_value = *au.fixed_value;
        } else if (au.required) {
          ad.dflt = dtd::default_kind::required;
        } else if (au.default_value) {
          ad.dflt = dtd::default_kind::value;
          ad.default_value = *au.default_value;
        } else {
          ad.dflt = dtd::default_kind::implied;
        }
        return ad;
      }

      // Map XSD occurrence to DTD quantifier
      dtd::quantifier
      occ_to_quantifier(const occurrence& occ) {
        if (occ.min_occurs == 0 && occ.is_unbounded())
          return dtd::quantifier::zero_or_more;
        if (occ.min_occurs == 1 && occ.is_unbounded())
          return dtd::quantifier::one_or_more;
        if (occ.min_occurs == 0 && occ.max_occurs == 1)
          return dtd::quantifier::optional;
        return dtd::quantifier::one;
      }

      // Translate an XSD particle to a DTD content_particle
      dtd::content_particle
      translate_particle(const particle& p) {
        dtd::content_particle cp;
        cp.quant = occ_to_quantifier(p.occurs);

        std::visit(
            [&](const auto& term) {
              using T = std::decay_t<decltype(term)>;
              if constexpr (std::is_same_v<T, element_decl>) {
                cp.kind = dtd::particle_kind::name;
                cp.name = term.name().local_name();
              } else if constexpr (std::is_same_v<T, element_ref>) {
                cp.kind = dtd::particle_kind::name;
                cp.name = term.ref.local_name();
              } else if constexpr (std::is_same_v<T, group_ref>) {
                // Inline the group's particles
                auto* mgd = schemas.find_model_group_def(term.ref);
                if (mgd) {
                  cp = translate_model_group(mgd->group());
                  cp.quant = occ_to_quantifier(p.occurs);
                }
              } else if constexpr (std::is_same_v<
                                       T, std::unique_ptr<model_group>>) {
                if (term) {
                  cp = translate_model_group(*term);
                  cp.quant = occ_to_quantifier(p.occurs);
                }
              } else if constexpr (std::is_same_v<T, wildcard>) {
                // Wildcards have no DTD equivalent — skip
                cp.kind = dtd::particle_kind::name;
                cp.name = "ANY";
              }
            },
            p.term);
        return cp;
      }

      dtd::content_particle
      translate_model_group(const model_group& mg) {
        dtd::content_particle cp;
        cp.kind = (mg.compositor() == compositor_kind::choice)
                      ? dtd::particle_kind::choice
                      : dtd::particle_kind::sequence;

        for (const auto& p : mg.particles()) {
          cp.children.push_back(translate_particle(p));
        }
        return cp;
      }

      void
      emit_element(const element_decl& e) {
        auto name = e.name().local_name();
        if (emitted.count(name)) return;
        emitted.insert(name);

        dtd::element_decl ded;
        ded.name = name;

        // Find the complex type for this element
        auto* ct = schemas.find_complex_type(e.type_name());
        if (!ct) {
          // Check if it's a simple built-in type
          if (e.type_name().namespace_uri() == xs_ns) {
            ded.content.kind = dtd::content_kind::mixed;
          } else {
            // Check for named simple type
            auto* st = schemas.find_simple_type(e.type_name());
            if (st) {
              ded.content.kind = dtd::content_kind::mixed;
            } else {
              ded.content.kind = dtd::content_kind::mixed;
            }
          }
          elements.push_back(std::move(ded));
          return;
        }

        // Translate content model
        auto& content = ct->content();
        if (content.kind == content_kind::empty) {
          ded.content.kind = dtd::content_kind::empty;
        } else if (content.kind == content_kind::simple) {
          ded.content.kind = dtd::content_kind::mixed;
        } else {
          // element_only or mixed
          bool is_mixed = ct->mixed();
          auto* cc = std::get_if<complex_content>(&content.detail);
          if (cc && cc->content_model) {
            if (is_mixed) {
              // Mixed content with children
              ded.content.kind = dtd::content_kind::mixed;
              for (const auto& p : cc->content_model->particles()) {
                auto* ed = std::get_if<element_decl>(&p.term);
                if (ed) {
                  ded.content.mixed_names.push_back(ed->name().local_name());
                }
                auto* er = std::get_if<element_ref>(&p.term);
                if (er) {
                  ded.content.mixed_names.push_back(er->ref.local_name());
                }
              }
            } else {
              ded.content.kind = dtd::content_kind::children;
              ded.content.particle = translate_model_group(*cc->content_model);
            }
          } else {
            ded.content.kind =
                is_mixed ? dtd::content_kind::mixed : dtd::content_kind::empty;
          }
        }

        elements.push_back(std::move(ded));

        // Translate attributes
        if (!ct->attributes().empty()) {
          dtd::attlist_decl al;
          al.element_name = name;
          for (const auto& au : ct->attributes()) {
            al.attributes.push_back(translate_attribute(au));
          }
          attlists.push_back(std::move(al));
        }
      }

      dtd::document
      translate() {
        for (const auto& s : schemas.schemas()) {
          for (const auto& e : s.elements()) {
            emit_element(e);
          }
        }
        return dtd::document{std::move(elements), std::move(attlists), {}};
      }
    };

  } // namespace

  dtd::document
  schema_to_dtd(const schema_set& schemas) {
    translator tr(schemas);
    return tr.translate();
  }

} // namespace xb
