#include <xb/railroad.hpp>

#include <xb/complex_type.hpp>
#include <xb/content_type.hpp>
#include <xb/element_decl.hpp>
#include <xb/model_group.hpp>
#include <xb/model_group_def.hpp>
#include <xb/wildcard.hpp>

#include <set>
#include <string>

namespace xb::railroad {

  namespace {

    const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema";

    const element_decl*
    resolve_element(const particle& p, const schema_set& schemas) {
      if (auto* elem = std::get_if<element_decl>(&p.term)) return elem;
      if (auto* ref = std::get_if<element_ref>(&p.term))
        return schemas.find_element(ref->ref);
      return nullptr;
    }

    const model_group*
    get_model_group(const complex_type& ct) {
      const auto& content = ct.content();
      if (content.kind != content_kind::element_only &&
          content.kind != content_kind::mixed)
        return nullptr;
      const auto* cc = std::get_if<complex_content>(&content.detail);
      if (!cc || !cc->content_model) return nullptr;
      return &*cc->content_model;
    }

    std::string
    occurrence_label(std::size_t min_occurs, std::size_t max_occurs) {
      if (max_occurs == xb::unbounded)
        return std::to_string(min_occurs) + "..*";
      return std::to_string(min_occurs) + ".." + std::to_string(max_occurs);
    }

    node
    make_terminal(const element_decl& elem) {
      std::string type_label;
      if (elem.type_name().namespace_uri() == xsd_ns)
        type_label = elem.type_name().local_name();
      return node{terminal{elem.name().local_name(), type_label, false}};
    }

    node
    make_reference(const element_decl& elem) {
      return node{reference{elem.name().local_name(), elem.type_name()}};
    }

    node
    build_node(const model_group& mg, const schema_set& schemas);

    node
    wrap_occurrence(node inner, const particle& p) {
      if (p.occurs.max_occurs > 1) {
        auto rep = std::make_unique<node>(std::move(inner));
        inner = node{repeat_node{
            std::move(rep),
            occurrence_label(p.occurs.min_occurs > 0 ? p.occurs.min_occurs : 1,
                             p.occurs.max_occurs)}};
      }
      if (p.occurs.min_occurs == 0) {
        auto opt = std::make_unique<node>(std::move(inner));
        inner = node{optional_node{std::move(opt)}};
      }
      return inner;
    }

    node
    build_particle_node(const particle& p, const schema_set& schemas) {
      // Check for nested model group (inline xs:sequence/xs:choice)
      if (auto* mg_ptr = std::get_if<std::unique_ptr<model_group>>(&p.term)) {
        if (*mg_ptr) return wrap_occurrence(build_node(**mg_ptr, schemas), p);
      }

      // Check for group_ref (xs:group ref="...")
      if (auto* gref = std::get_if<group_ref>(&p.term)) {
        const auto* mgd = schemas.find_model_group_def(gref->ref);
        if (mgd) return wrap_occurrence(build_node(mgd->group(), schemas), p);
        return node{terminal{gref->ref.local_name(), "group", false}};
      }

      // Check for wildcard (xs:any)
      if (std::get_if<wildcard>(&p.term)) {
        return wrap_occurrence(node{terminal{"any", "wildcard", false}}, p);
      }

      const auto* elem = resolve_element(p, schemas);
      if (!elem) return node{terminal{"?", "", false}};

      // Decide if this is a terminal or a reference to a complex type
      bool is_complex = elem->type_name().namespace_uri() != xsd_ns &&
                        schemas.find_complex_type(elem->type_name()) != nullptr;

      node inner = is_complex ? make_reference(*elem) : make_terminal(*elem);
      return wrap_occurrence(std::move(inner), p);
    }

    node
    build_node(const model_group& mg, const schema_set& schemas) {
      switch (mg.compositor()) {
        case compositor_kind::sequence:
        case compositor_kind::all:
        case compositor_kind::interleave: {
          sequence seq;
          for (const auto& p : mg.particles())
            seq.children.push_back(build_particle_node(p, schemas));
          if (seq.children.size() == 1) return std::move(seq.children[0]);
          return node{std::move(seq)};
        }

        case compositor_kind::choice: {
          choice ch;
          for (const auto& p : mg.particles())
            ch.alternatives.push_back(build_particle_node(p, schemas));
          if (ch.alternatives.size() == 1) return std::move(ch.alternatives[0]);
          return node{std::move(ch)};
        }
      }
      return node{terminal{"?", "", false}};
    }

  } // namespace

  diagram
  build_diagram(const schema_set& schemas, const qname& type_name) {
    diagram result;
    result.name = type_name.local_name();
    result.type_name = type_name;

    const auto* ct = schemas.find_complex_type(type_name);
    if (!ct) {
      result.root = node{terminal{type_name.local_name(), "", false}};
      return result;
    }

    // Collect attributes
    for (const auto& attr : ct->attributes()) {
      std::string type_label;
      if (attr.type_name.namespace_uri() == xsd_ns)
        type_label = attr.type_name.local_name();
      result.attributes.push_back(
          {attr.name.local_name(), type_label, true, attr.required});
    }

    const auto* mg = get_model_group(*ct);
    if (!mg) {
      result.root = node{sequence{}};
      return result;
    }

    result.root = build_node(*mg, schemas);
    return result;
  }

  std::vector<diagram>
  build_diagrams(const schema_set& schemas, const qname& element_name) {
    std::vector<diagram> result;
    std::set<qname> visited;

    // BFS/DFS to find all reachable complex types
    std::vector<qname> worklist;

    const auto* elem = schemas.find_element(element_name);
    if (!elem) return result;

    worklist.push_back(elem->type_name());

    while (!worklist.empty()) {
      qname current = worklist.back();
      worklist.pop_back();

      if (visited.count(current)) continue;
      visited.insert(current);

      const auto* ct = schemas.find_complex_type(current);
      if (!ct) continue;

      result.push_back(build_diagram(schemas, current));

      // Find referenced complex types in the content model
      const auto* mg = get_model_group(*ct);
      if (!mg) continue;

      // Collect elements from particles, recursing into group refs
      std::vector<const model_group*> groups_to_scan;
      groups_to_scan.push_back(mg);
      while (!groups_to_scan.empty()) {
        const auto* g = groups_to_scan.back();
        groups_to_scan.pop_back();
        for (const auto& p : g->particles()) {
          const auto* child_elem = resolve_element(p, schemas);
          if (child_elem && child_elem->type_name().namespace_uri() != xsd_ns) {
            worklist.push_back(child_elem->type_name());
          }
          if (auto* gref = std::get_if<group_ref>(&p.term)) {
            const auto* mgd = schemas.find_model_group_def(gref->ref);
            if (mgd) groups_to_scan.push_back(&mgd->group());
          }
          if (auto* nested =
                  std::get_if<std::unique_ptr<model_group>>(&p.term)) {
            if (*nested) groups_to_scan.push_back(nested->get());
          }
        }
      }
    }

    return result;
  }

} // namespace xb::railroad
