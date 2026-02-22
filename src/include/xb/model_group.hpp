#pragma once

#include <xb/element_decl.hpp>
#include <xb/occurrence.hpp>
#include <xb/schema_fwd.hpp>
#include <xb/wildcard.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace xb {

  class model_group;

  struct group_ref {
    qname ref;

    bool
    operator==(const group_ref&) const = default;
  };

  struct particle {
    using term_type = std::variant<element_decl, element_ref, group_ref,
                                   std::unique_ptr<model_group>, wildcard>;

    term_type term;
    occurrence occurs;

    particle(term_type t, occurrence o = {}) : term(std::move(t)), occurs(o) {}

    particle(const particle&) = delete;
    particle&
    operator=(const particle&) = delete;

    particle(particle&&) = default;
    particle&
    operator=(particle&&) = default;

    bool
    operator==(const particle& other) const;
  };

  class model_group {
    compositor_kind compositor_;
    std::vector<particle> particles_;

  public:
    model_group() : compositor_(compositor_kind::sequence) {}

    explicit model_group(compositor_kind c,
                         std::vector<particle> particles = {})
        : compositor_(c), particles_(std::move(particles)) {}

    model_group(const model_group&) = delete;
    model_group&
    operator=(const model_group&) = delete;

    model_group(model_group&&) = default;
    model_group&
    operator=(model_group&&) = default;

    compositor_kind
    compositor() const {
      return compositor_;
    }

    const std::vector<particle>&
    particles() const {
      return particles_;
    }

    void
    add_particle(particle p) {
      particles_.push_back(std::move(p));
    }

    bool
    operator==(const model_group& other) const;
  };

} // namespace xb
