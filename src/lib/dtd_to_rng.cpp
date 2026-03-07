#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/dtd_to_rng.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb {

  namespace {

    const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

    struct translator {
      const dtd::document& doc;
      std::vector<rng::define> defines;
      std::unordered_set<std::string> emitted;

      // Merged attributes: element_name -> vector of attribute_def*
      std::unordered_map<std::string, std::vector<const dtd::attribute_def*>>
          attr_map;

      explicit translator(const dtd::document& d) : doc(d) {
        for (const auto& al : doc.attlists) {
          for (const auto& ad : al.attributes) {
            attr_map[al.element_name].push_back(&ad);
          }
        }
      }

      // Map DTD attribute type to an RNG content pattern
      std::unique_ptr<rng::pattern>
      attr_type_content(const dtd::attribute_def& ad) {
        if (ad.type == dtd::attribute_type::enumeration) {
          std::vector<std::unique_ptr<rng::pattern>> vals;
          for (const auto& v : ad.enum_values) {
            vals.push_back(
                rng::make_pattern(rng::value_pattern{"", "token", v, ""}));
          }
          return right_fold<rng::choice_pattern>(vals);
        }
        if (ad.type == dtd::attribute_type::cdata) {
          return rng::make_pattern(rng::text_pattern{});
        }
        // ID, IDREF, IDREFS, NMTOKEN, etc. → data_pattern
        std::string type;
        switch (ad.type) {
          case dtd::attribute_type::id:
            type = "ID";
            break;
          case dtd::attribute_type::idref:
            type = "IDREF";
            break;
          case dtd::attribute_type::idrefs:
            type = "IDREFS";
            break;
          case dtd::attribute_type::nmtoken:
            type = "NMTOKEN";
            break;
          case dtd::attribute_type::nmtokens:
            type = "NMTOKENS";
            break;
          case dtd::attribute_type::entity:
            type = "ENTITY";
            break;
          case dtd::attribute_type::entities:
            type = "ENTITIES";
            break;
          case dtd::attribute_type::notation:
            type = "NOTATION";
            break;
          default:
            type = "string";
            break;
        }
        return rng::make_pattern(rng::data_pattern{xsd_dt, type, {}, nullptr});
      }

      // Build RNG pattern for an element's attributes
      std::unique_ptr<rng::pattern>
      translate_attributes(const std::string& element_name) {
        auto it = attr_map.find(element_name);
        if (it == attr_map.end()) return nullptr;

        std::vector<std::unique_ptr<rng::pattern>> parts;
        for (const auto* ad : it->second) {
          std::unique_ptr<rng::pattern> content;
          if (ad->dflt == dtd::default_kind::fixed) {
            content = rng::make_pattern(
                rng::value_pattern{"", "token", ad->default_value, ""});
          } else {
            content = attr_type_content(*ad);
          }

          auto attr = rng::make_pattern(rng::attribute_pattern{
              rng::name_class(rng::specific_name{"", ad->name}),
              std::move(content)});

          // #REQUIRED → bare; everything else → optional
          if (ad->dflt != dtd::default_kind::required) {
            attr = rng::make_pattern(rng::optional_pattern{std::move(attr)});
          }
          parts.push_back(std::move(attr));
        }

        if (parts.empty()) return nullptr;
        if (parts.size() == 1) return std::move(parts[0]);
        return right_fold<rng::group_pattern>(parts);
      }

      // Right-fold a list of patterns using a binary pattern constructor
      template <typename BinaryPattern>
      std::unique_ptr<rng::pattern>
      right_fold(std::vector<std::unique_ptr<rng::pattern>>& parts) {
        if (parts.empty()) { return rng::make_pattern(rng::empty_pattern{}); }
        if (parts.size() == 1) return std::move(parts[0]);
        auto result = std::move(parts.back());
        for (int i = static_cast<int>(parts.size()) - 2; i >= 0; --i) {
          result = rng::make_pattern(
              BinaryPattern{std::move(parts[static_cast<std::size_t>(i)]),
                            std::move(result)});
        }
        return result;
      }

      // Wrap a pattern with the appropriate quantifier
      std::unique_ptr<rng::pattern>
      wrap_quantifier(std::unique_ptr<rng::pattern> inner, dtd::quantifier q) {
        switch (q) {
          case dtd::quantifier::one:
            return inner;
          case dtd::quantifier::optional:
            return rng::make_pattern(rng::optional_pattern{std::move(inner)});
          case dtd::quantifier::zero_or_more:
            return rng::make_pattern(
                rng::zero_or_more_pattern{std::move(inner)});
          case dtd::quantifier::one_or_more:
            return rng::make_pattern(
                rng::one_or_more_pattern{std::move(inner)});
        }
        return inner;
      }

      // Translate a DTD content_particle to an RNG pattern
      std::unique_ptr<rng::pattern>
      translate_particle(const dtd::content_particle& cp) {
        if (cp.kind == dtd::particle_kind::name) {
          emit_element_by_name(cp.name);
          auto inner = rng::make_pattern(rng::ref_pattern{cp.name});
          return wrap_quantifier(std::move(inner), cp.quant);
        }
        // sequence or choice
        std::vector<std::unique_ptr<rng::pattern>> parts;
        for (const auto& child : cp.children) {
          parts.push_back(translate_particle(child));
        }
        std::unique_ptr<rng::pattern> inner;
        if (cp.kind == dtd::particle_kind::sequence) {
          inner = right_fold<rng::group_pattern>(parts);
        } else {
          inner = right_fold<rng::choice_pattern>(parts);
        }
        return wrap_quantifier(std::move(inner), cp.quant);
      }

      // Translate the content of an element to an RNG pattern
      std::unique_ptr<rng::pattern>
      translate_content(const dtd::element_decl& ed) {
        switch (ed.content.kind) {
          case dtd::content_kind::empty:
            return rng::make_pattern(rng::empty_pattern{});

          case dtd::content_kind::any: {
            // ANY → mixed { zeroOrMore { element { anyName } { text } } }
            auto any_elem = rng::make_pattern(
                rng::element_pattern{rng::name_class(rng::any_name_nc{nullptr}),
                                     rng::make_pattern(rng::text_pattern{})});
            auto zm = rng::make_pattern(
                rng::zero_or_more_pattern{std::move(any_elem)});
            return rng::make_pattern(rng::mixed_pattern{std::move(zm)});
          }

          case dtd::content_kind::mixed: {
            if (ed.content.mixed_names.empty()) {
              // Pure (#PCDATA) → text
              return rng::make_pattern(rng::text_pattern{});
            }
            // (#PCDATA | a | b)* → mixed { zeroOrMore { choice { ref ... } } }
            std::vector<std::unique_ptr<rng::pattern>> refs;
            for (const auto& name : ed.content.mixed_names) {
              emit_element_by_name(name);
              refs.push_back(rng::make_pattern(rng::ref_pattern{name}));
            }
            auto ch = right_fold<rng::choice_pattern>(refs);
            auto zm =
                rng::make_pattern(rng::zero_or_more_pattern{std::move(ch)});
            return rng::make_pattern(rng::mixed_pattern{std::move(zm)});
          }

          case dtd::content_kind::children: {
            if (!ed.content.particle) {
              return rng::make_pattern(rng::empty_pattern{});
            }
            auto& root_cp = *ed.content.particle;
            // Top-level particle: translate its children directly
            std::vector<std::unique_ptr<rng::pattern>> parts;
            for (const auto& child : root_cp.children) {
              parts.push_back(translate_particle(child));
            }
            std::unique_ptr<rng::pattern> body;
            if (root_cp.kind == dtd::particle_kind::choice) {
              body = right_fold<rng::choice_pattern>(parts);
            } else {
              body = right_fold<rng::group_pattern>(parts);
            }
            return wrap_quantifier(std::move(body), root_cp.quant);
          }
        }
        return rng::make_pattern(rng::empty_pattern{});
      }

      // Look up an element by name and emit it
      void
      emit_element_by_name(const std::string& name) {
        for (const auto& ed : doc.elements) {
          if (ed.name == name) {
            emit_element(ed);
            return;
          }
        }
      }

      void
      emit_element(const dtd::element_decl& ed) {
        if (emitted.count(ed.name)) return;
        emitted.insert(ed.name);

        auto content = translate_content(ed);
        auto attrs = translate_attributes(ed.name);

        // Combine content and attributes
        std::unique_ptr<rng::pattern> body;
        if (attrs) {
          if (content->holds<rng::empty_pattern>()) {
            body = std::move(attrs);
          } else {
            body = rng::make_pattern(
                rng::group_pattern{std::move(content), std::move(attrs)});
          }
        } else {
          body = std::move(content);
        }

        auto elem = rng::make_pattern(rng::element_pattern{
            rng::name_class(rng::specific_name{"", ed.name}), std::move(body)});

        defines.push_back(
            rng::define{ed.name, rng::combine_method::none, std::move(elem)});
      }

      rng::pattern
      translate() {
        for (const auto& ed : doc.elements) {
          emit_element(ed);
        }

        // Build start pattern
        std::vector<std::unique_ptr<rng::pattern>> refs;
        for (const auto& ed : doc.elements) {
          refs.push_back(rng::make_pattern(rng::ref_pattern{ed.name}));
        }

        std::unique_ptr<rng::pattern> start;
        if (refs.empty()) {
          start = rng::make_pattern(rng::empty_pattern{});
        } else if (refs.size() == 1) {
          start = std::move(refs[0]);
        } else {
          start = right_fold<rng::choice_pattern>(refs);
        }

        return rng::pattern(
            rng::grammar_pattern{std::move(start), std::move(defines), {}});
      }
    };

  } // namespace

  rng::pattern
  dtd_to_rng(const dtd::document& doc) {
    translator tr(doc);
    return tr.translate();
  }

} // namespace xb
