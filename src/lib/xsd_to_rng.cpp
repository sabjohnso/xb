#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <xb/xsd_to_rng.hpp>

#include <string>
#include <unordered_set>
#include <vector>

namespace xb {

  namespace {

    const std::string xs_ns = "http://www.w3.org/2001/XMLSchema";
    const std::string xsd_dt = "http://www.w3.org/2001/XMLSchema-datatypes";

    struct translator {
      const schema_set& schemas;
      std::vector<rng::define> defines;
      std::unordered_set<std::string> emitted;
      std::unordered_set<std::string> element_names;

      explicit translator(const schema_set& ss) : schemas(ss) {}

      // Collect all element local names to detect collisions with type names
      void
      collect_element_names() {
        for (const auto& s : schemas.schemas()) {
          for (const auto& e : s.elements()) {
            element_names.insert(e.name().local_name());
          }
        }
      }

      std::string
      define_name_for_element(const element_decl& e) {
        return e.name().local_name();
      }

      std::string
      define_name_for_complex_type(const complex_type& ct) {
        auto name = ct.name().local_name();
        if (element_names.count(name)) { name += ".type"; }
        return name;
      }

      std::string
      define_name_for_simple_type(const simple_type& st) {
        auto name = st.name().local_name();
        if (element_names.count(name)) { name += ".type"; }
        return name;
      }

      std::string
      define_name_for_model_group(const model_group_def& mg) {
        return "group." + mg.name().local_name();
      }

      std::string
      define_name_for_attr_group(const attribute_group_def& ag) {
        return "attrgroup." + ag.name().local_name();
      }

      bool
      is_builtin_xsd_type(const qname& name) {
        return name.namespace_uri() == xs_ns;
      }

      std::unique_ptr<rng::pattern>
      translate_type_ref(const qname& type_name) {
        if (is_builtin_xsd_type(type_name)) {
          return rng::make_pattern(
              rng::data_pattern{xsd_dt, type_name.local_name(), {}, nullptr});
        }
        // Check for named simple type
        if (auto* st = schemas.find_simple_type(type_name)) {
          auto dname = define_name_for_simple_type(*st);
          emit_simple_type(*st);
          return rng::make_pattern(rng::ref_pattern{dname});
        }
        // Check for named complex type
        if (auto* ct = schemas.find_complex_type(type_name)) {
          auto dname = define_name_for_complex_type(*ct);
          emit_complex_type(*ct, type_name.namespace_uri());
          return rng::make_pattern(rng::ref_pattern{dname});
        }
        // Fallback: treat as data
        return rng::make_pattern(
            rng::data_pattern{xsd_dt, type_name.local_name(), {}, nullptr});
      }

      std::unique_ptr<rng::pattern>
      wrap_occurrence(std::unique_ptr<rng::pattern> inner,
                      const occurrence& occ) {
        if (occ.min_occurs == 1 && occ.max_occurs == 1) { return inner; }
        if (occ.min_occurs == 0 && occ.max_occurs == 1) {
          return rng::make_pattern(rng::optional_pattern{std::move(inner)});
        }
        if (occ.min_occurs == 0 && occ.is_unbounded()) {
          return rng::make_pattern(rng::zero_or_more_pattern{std::move(inner)});
        }
        if (occ.min_occurs == 1 && occ.is_unbounded()) {
          return rng::make_pattern(rng::one_or_more_pattern{std::move(inner)});
        }
        if (occ.min_occurs == 0) {
          return rng::make_pattern(rng::zero_or_more_pattern{std::move(inner)});
        }
        return rng::make_pattern(rng::one_or_more_pattern{std::move(inner)});
      }

      std::unique_ptr<rng::pattern>
      translate_particle(const particle& p, const std::string& ns) {
        auto inner = std::visit(
            [&](const auto& term) -> std::unique_ptr<rng::pattern> {
              using T = std::decay_t<decltype(term)>;
              if constexpr (std::is_same_v<T, element_decl>) {
                auto content = translate_type_ref(term.type_name());
                return rng::make_pattern(rng::element_pattern{
                    rng::name_class(rng::specific_name{
                        term.name().namespace_uri(), term.name().local_name()}),
                    std::move(content)});
              } else if constexpr (std::is_same_v<T, element_ref>) {
                auto def_name = term.ref.local_name();
                // Ensure the referenced element is emitted
                if (auto* e = schemas.find_element(term.ref)) {
                  emit_element(*e, ns);
                }
                return rng::make_pattern(rng::ref_pattern{def_name});
              } else if constexpr (std::is_same_v<T, group_ref>) {
                auto def_name = "group." + term.ref.local_name();
                if (auto* mg = schemas.find_model_group_def(term.ref)) {
                  emit_model_group_def(*mg, ns);
                }
                return rng::make_pattern(rng::ref_pattern{def_name});
              } else if constexpr (std::is_same_v<
                                       T, std::unique_ptr<model_group>>) {
                if (term) { return translate_model_group(*term, ns); }
                return rng::make_pattern(rng::empty_pattern{});
              } else if constexpr (std::is_same_v<T, wildcard>) {
                // xs:any → element with anyName + text content
                return rng::make_pattern(rng::element_pattern{
                    rng::name_class(rng::any_name_nc{nullptr}),
                    rng::make_pattern(rng::text_pattern{})});
              } else {
                return rng::make_pattern(rng::empty_pattern{});
              }
            },
            p.term);
        return wrap_occurrence(std::move(inner), p.occurs);
      }

      // Right-fold a list of patterns using a binary pattern constructor
      template <typename BinaryPattern>
      std::unique_ptr<rng::pattern>
      right_fold(std::vector<std::unique_ptr<rng::pattern>>& parts) {
        if (parts.empty()) { return rng::make_pattern(rng::empty_pattern{}); }
        if (parts.size() == 1) { return std::move(parts[0]); }
        auto result = std::move(parts.back());
        for (int i = static_cast<int>(parts.size()) - 2; i >= 0; --i) {
          result = rng::make_pattern(
              BinaryPattern{std::move(parts[static_cast<std::size_t>(i)]),
                            std::move(result)});
        }
        return result;
      }

      std::unique_ptr<rng::pattern>
      translate_model_group(const model_group& mg, const std::string& ns) {
        std::vector<std::unique_ptr<rng::pattern>> parts;
        for (const auto& p : mg.particles()) {
          parts.push_back(translate_particle(p, ns));
        }
        switch (mg.compositor()) {
          case compositor_kind::sequence:
            return right_fold<rng::group_pattern>(parts);
          case compositor_kind::choice:
            return right_fold<rng::choice_pattern>(parts);
          case compositor_kind::all:
          case compositor_kind::interleave:
            return right_fold<rng::interleave_pattern>(parts);
        }
        return right_fold<rng::group_pattern>(parts);
      }

      std::unique_ptr<rng::pattern>
      translate_attributes(const std::vector<attribute_use>& attrs,
                           const std::vector<attribute_group_ref>& ag_refs,
                           const std::optional<wildcard>& attr_wc,
                           const std::string& ns) {
        std::vector<std::unique_ptr<rng::pattern>> parts;
        for (const auto& a : attrs) {
          auto content = translate_type_ref(a.type_name);
          auto attr_pat = rng::make_pattern(rng::attribute_pattern{
              rng::name_class(rng::specific_name{a.name.namespace_uri(),
                                                 a.name.local_name()}),
              std::move(content)});
          if (a.fixed_value) {
            // Fixed attribute → value_pattern
            attr_pat = rng::make_pattern(rng::attribute_pattern{
                rng::name_class(rng::specific_name{a.name.namespace_uri(),
                                                   a.name.local_name()}),
                rng::make_pattern(rng::value_pattern{
                    xsd_dt, a.type_name.local_name(), *a.fixed_value, ""})});
          }
          if (!a.required) {
            attr_pat =
                rng::make_pattern(rng::optional_pattern{std::move(attr_pat)});
          }
          parts.push_back(std::move(attr_pat));
        }
        for (const auto& agr : ag_refs) {
          auto def_name = "attrgroup." + agr.ref.local_name();
          if (auto* ag = schemas.find_attribute_group_def(agr.ref)) {
            emit_attribute_group_def(*ag, ns);
          }
          parts.push_back(rng::make_pattern(rng::ref_pattern{def_name}));
        }
        if (attr_wc) {
          // xs:anyAttribute → attribute with anyName
          parts.push_back(rng::make_pattern(
              rng::attribute_pattern{rng::name_class(rng::any_name_nc{nullptr}),
                                     rng::make_pattern(rng::text_pattern{})}));
        }
        if (parts.empty()) { return nullptr; }
        if (parts.size() == 1) { return std::move(parts[0]); }
        // Attributes are combined with group (sequence)
        return right_fold<rng::group_pattern>(parts);
      }

      std::unique_ptr<rng::pattern>
      translate_content(const complex_type& ct, const std::string& ns) {
        auto& content = ct.content();
        std::unique_ptr<rng::pattern> body;

        if (content.kind == content_kind::empty) {
          body = rng::make_pattern(rng::empty_pattern{});
        } else if (content.kind == content_kind::simple) {
          if (auto* sc = std::get_if<simple_content>(&content.detail)) {
            body = translate_type_ref(sc->base_type_name);
          } else {
            body = rng::make_pattern(rng::text_pattern{});
          }
        } else {
          // element_only or mixed
          if (auto* cc = std::get_if<complex_content>(&content.detail)) {
            if (cc->content_model) {
              body = translate_model_group(*cc->content_model, ns);
              // Handle extension: prepend base type ref
              if (cc->derivation == derivation_method::extension &&
                  cc->base_type_name != qname()) {
                auto base_ref = translate_type_ref(cc->base_type_name);
                body = rng::make_pattern(
                    rng::group_pattern{std::move(base_ref), std::move(body)});
              }
            } else if (cc->base_type_name != qname() &&
                       cc->derivation == derivation_method::extension) {
              body = translate_type_ref(cc->base_type_name);
            } else {
              body = rng::make_pattern(rng::empty_pattern{});
            }
          } else {
            body = rng::make_pattern(rng::empty_pattern{});
          }
        }

        // Add attributes
        auto attrs =
            translate_attributes(ct.attributes(), ct.attribute_group_refs(),
                                 ct.attribute_wildcard(), ns);
        if (attrs) {
          if (body->holds<rng::empty_pattern>()) {
            body = std::move(attrs);
          } else {
            body = rng::make_pattern(
                rng::group_pattern{std::move(body), std::move(attrs)});
          }
        }

        // Wrap in mixed if needed
        if (ct.mixed()) {
          body = rng::make_pattern(rng::mixed_pattern{std::move(body)});
        }

        return body;
      }

      void
      emit_element(const element_decl& e, const std::string& /*ns*/) {
        auto dname = define_name_for_element(e);
        if (emitted.count(dname)) return;
        emitted.insert(dname);

        auto content = translate_type_ref(e.type_name());
        auto body = rng::make_pattern(rng::element_pattern{
            rng::name_class(rng::specific_name{e.name().namespace_uri(),
                                               e.name().local_name()}),
            std::move(content)});
        defines.push_back(
            rng::define{dname, rng::combine_method::none, std::move(body)});
      }

      void
      emit_complex_type(const complex_type& ct, const std::string& ns) {
        auto dname = define_name_for_complex_type(ct);
        if (emitted.count(dname)) return;
        emitted.insert(dname);

        auto body = translate_content(ct, ns);
        defines.push_back(
            rng::define{dname, rng::combine_method::none, std::move(body)});
      }

      void
      emit_simple_type(const simple_type& st) {
        auto dname = define_name_for_simple_type(st);
        if (emitted.count(dname)) return;
        emitted.insert(dname);

        std::unique_ptr<rng::pattern> body;

        // Check for enumeration facets first
        if (!st.facets().enumeration.empty()) {
          std::vector<std::unique_ptr<rng::pattern>> vals;
          for (const auto& v : st.facets().enumeration) {
            std::string dt_lib = xsd_dt;
            std::string type = st.base_type_name().local_name();
            if (is_builtin_xsd_type(st.base_type_name())) {
              type = st.base_type_name().local_name();
            }
            vals.push_back(
                rng::make_pattern(rng::value_pattern{dt_lib, type, v, ""}));
          }
          body = right_fold<rng::choice_pattern>(vals);
        } else if (st.variety() == simple_type_variety::list) {
          auto item_content = st.item_type_name()
                                  ? translate_type_ref(*st.item_type_name())
                                  : rng::make_pattern(rng::data_pattern{
                                        xsd_dt, "string", {}, nullptr});
          body = rng::make_pattern(rng::list_pattern{std::move(item_content)});
        } else if (st.variety() == simple_type_variety::union_type) {
          std::vector<std::unique_ptr<rng::pattern>> members;
          for (const auto& m : st.member_type_names()) {
            members.push_back(translate_type_ref(m));
          }
          body = right_fold<rng::choice_pattern>(members);
        } else {
          // Atomic type with possible facets
          std::vector<rng::data_param> params;
          auto& f = st.facets();
          if (f.pattern)
            params.push_back(rng::data_param{"pattern", *f.pattern});
          if (f.min_inclusive)
            params.push_back(rng::data_param{"minInclusive", *f.min_inclusive});
          if (f.max_inclusive)
            params.push_back(rng::data_param{"maxInclusive", *f.max_inclusive});
          if (f.min_exclusive)
            params.push_back(rng::data_param{"minExclusive", *f.min_exclusive});
          if (f.max_exclusive)
            params.push_back(rng::data_param{"maxExclusive", *f.max_exclusive});
          if (f.length)
            params.push_back(
                rng::data_param{"length", std::to_string(*f.length)});
          if (f.min_length)
            params.push_back(
                rng::data_param{"minLength", std::to_string(*f.min_length)});
          if (f.max_length)
            params.push_back(
                rng::data_param{"maxLength", std::to_string(*f.max_length)});
          if (f.total_digits)
            params.push_back(rng::data_param{"totalDigits",
                                             std::to_string(*f.total_digits)});
          if (f.fraction_digits)
            params.push_back(rng::data_param{
                "fractionDigits", std::to_string(*f.fraction_digits)});

          std::string base_type = is_builtin_xsd_type(st.base_type_name())
                                      ? st.base_type_name().local_name()
                                      : "string";
          body = rng::make_pattern(
              rng::data_pattern{xsd_dt, base_type, std::move(params), nullptr});
        }

        defines.push_back(
            rng::define{dname, rng::combine_method::none, std::move(body)});
      }

      void
      emit_model_group_def(const model_group_def& mg, const std::string& ns) {
        auto dname = define_name_for_model_group(mg);
        if (emitted.count(dname)) return;
        emitted.insert(dname);

        auto body = translate_model_group(mg.group(), ns);
        defines.push_back(
            rng::define{dname, rng::combine_method::none, std::move(body)});
      }

      void
      emit_attribute_group_def(const attribute_group_def& ag,
                               const std::string& ns) {
        auto dname = define_name_for_attr_group(ag);
        if (emitted.count(dname)) return;
        emitted.insert(dname);

        auto body =
            translate_attributes(ag.attributes(), ag.attribute_group_refs(),
                                 ag.attribute_wildcard(), ns);
        if (!body) { body = rng::make_pattern(rng::empty_pattern{}); }
        defines.push_back(
            rng::define{dname, rng::combine_method::none, std::move(body)});
      }

      rng::pattern
      translate() {
        collect_element_names();

        // Emit all global elements, complex types, simple types,
        // model group defs, attribute group defs
        for (const auto& s : schemas.schemas()) {
          auto ns = s.target_namespace();
          for (const auto& e : s.elements()) {
            emit_element(e, ns);
          }
          for (const auto& ct : s.complex_types()) {
            emit_complex_type(ct, ns);
          }
          for (const auto& st : s.simple_types()) {
            emit_simple_type(st);
          }
          for (const auto& mg : s.model_group_defs()) {
            emit_model_group_def(mg, ns);
          }
          for (const auto& ag : s.attribute_group_defs()) {
            emit_attribute_group_def(ag, ns);
          }
        }

        // Build start pattern
        std::unique_ptr<rng::pattern> start;
        // Collect all global element defines as refs for start
        std::vector<std::unique_ptr<rng::pattern>> elem_refs;
        for (const auto& s : schemas.schemas()) {
          for (const auto& e : s.elements()) {
            elem_refs.push_back(rng::make_pattern(
                rng::ref_pattern{define_name_for_element(e)}));
          }
        }
        if (elem_refs.empty()) {
          start = rng::make_pattern(rng::empty_pattern{});
        } else if (elem_refs.size() == 1) {
          start = std::move(elem_refs[0]);
        } else {
          start = right_fold<rng::choice_pattern>(elem_refs);
        }

        return rng::pattern(
            rng::grammar_pattern{std::move(start), std::move(defines), {}});
      }
    };

  } // namespace

  rng::pattern
  xsd_to_rng(const schema_set& schemas) {
    translator tr(schemas);
    return tr.translate();
  }

} // namespace xb
