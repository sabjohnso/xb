#include <xb/model_group.hpp>

namespace xb {

  bool
  particle::operator==(const particle& other) const {
    if (occurs != other.occurs) return false;

    // Compare terms, handling unique_ptr<model_group> specially
    return std::visit(
        [](const auto& a, const auto& b) -> bool {
          using A = std::decay_t<decltype(a)>;
          using B = std::decay_t<decltype(b)>;

          if constexpr (!std::is_same_v<A, B>) {
            return false;
          } else if constexpr (std::is_same_v<A,
                                              std::unique_ptr<model_group>>) {
            if (!a && !b) return true;
            if (!a || !b) return false;
            return *a == *b;
          } else {
            return a == b;
          }
        },
        term, other.term);
  }

  bool
  model_group::operator==(const model_group& other) const {
    if (compositor_ != other.compositor_) return false;
    if (particles_.size() != other.particles_.size()) return false;
    for (std::size_t i = 0; i < particles_.size(); ++i) {
      if (!(particles_[i] == other.particles_[i])) return false;
    }
    return true;
  }

} // namespace xb
