#ifndef XB_WIRE_LAYOUT_ENGINE_HPP
#define XB_WIRE_LAYOUT_ENGINE_HPP

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace xb::wire {

  enum class field_category {
    data,
    wire_only,
    padding,
    computed,
    constant,
  };

  struct resolved_field {
    std::string name;
    field_category category;
    unsigned width_bits;
    std::optional<unsigned> offset_bits;
  };

  struct position_marker {
    std::string name;
    unsigned offset_bits;
  };

  struct resolved_alternative {
    std::string name;
    std::string discriminant_value;
    std::vector<resolved_field> fields;
    unsigned total_bits = 0;
  };

  struct resolved_choice {
    std::string name;
    std::string discriminant_field;
    unsigned offset_bits = 0;
    std::vector<resolved_alternative> alternatives;
  };

  struct resolved_repeat {
    std::string count_field;
    std::string element_type;
    unsigned offset_bits = 0;
    unsigned element_wire_bits = 0;
  };

  struct resolved_layout {
    std::string message_name;
    std::vector<resolved_field> fields;
    std::vector<position_marker> markers;
    std::vector<resolved_choice> choices;
    std::vector<resolved_repeat> repeats;
    bool is_fixed = true;
    unsigned total_bits = 0;
  };

  namespace detail {

    inline unsigned
    align_up(unsigned offset, unsigned alignment) {
      if (alignment == 0) return offset;
      auto remainder = offset % alignment;
      return remainder == 0 ? offset : offset + (alignment - remainder);
    }

    // Detect std::unique_ptr<T>
    template <typename T>
    struct is_unique_ptr : std::false_type {};

    template <typename T>
    struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};

    // Member detection concepts
    template <typename T>
    concept has_algorithm = requires(T t) { t.algorithm; };

    template <typename T>
    concept has_offset = requires(T t) { t.offset; };

    template <typename T>
    concept has_align_to = requires(T t) { t.align_to; };

    template <typename T>
    concept has_value = requires(T t) { t.value; };

    template <typename T>
    concept has_field = requires(T t) { t.field; };

    template <typename T>
    concept has_count_field = requires(T t) { t.count_field; };

    template <typename T>
    concept has_discriminant_field = requires(T t) { t.discriminant_field; };

    template <typename T>
    concept has_alternative = requires(T t) { t.alternative; };

    template <typename T>
    concept has_element_type = requires(T t) { t.element_type; };

    template <typename T>
    concept has_name = requires(T t) { t.name; };

    template <typename T>
    concept has_bits = requires(T t) { t.bits; };

    /// Walk a message's choice vector, computing offsets for each item.
    struct layout_walker {
      resolved_layout& layout;
      unsigned& offset;
      bool& fixed;
      unsigned alignment;

      void
      align_if_needed() const {
        if (alignment > 0 && !layout.fields.empty())
          offset = align_up(offset, alignment);
      }

      void
      emit(const std::string& name, field_category cat, unsigned width) {
        layout.fields.push_back(
            {name, cat, width,
             fixed ? std::optional<unsigned>(offset) : std::nullopt});
        offset += width;
      }

      template <typename T>
      void
      operator()(const T& item) {
        if constexpr (is_unique_ptr<T>::value) {
          visit_ptr(item);
        } else {
          visit_value(item);
        }
      }

    private:
      // --- Value types (non-pointer variant alternatives) ---

      template <typename T>
      void
      visit_value(const T& f) {
        // field_type: has bits, name, offset, no algorithm
        if constexpr (has_bits<T> && has_name<T> && has_offset<T> &&
                      !has_algorithm<T>) {
          align_if_needed();
          unsigned field_offset = f.offset.has_value() ? *f.offset : offset;
          layout.fields.push_back(
              {f.name, field_category::data, f.bits,
               fixed ? std::optional<unsigned>(field_offset) : std::nullopt});
          offset = field_offset + f.bits;
        }
        // wire_field_type: has bits, name, no offset, no algorithm,
        // no align_to
        else if constexpr (has_bits<T> && has_name<T> && !has_offset<T> &&
                           !has_algorithm<T> && !has_align_to<T>) {
          align_if_needed();
          emit(f.name, field_category::wire_only, f.bits);
        }
        // computed_field_type: has algorithm, name, bits
        else if constexpr (has_algorithm<T>) {
          align_if_needed();
          emit(f.name, field_category::computed, f.bits);
        }
        // padding_field_type: has align_to, no name
        else if constexpr (has_align_to<T> && !has_name<T>) {
          unsigned pad = 0;
          if (f.align_to.has_value())
            pad = align_up(offset, *f.align_to) - offset;
          else if (f.bits.has_value())
            pad = *f.bits;
          emit("", field_category::padding, pad);
        }
        // constant_type: has value, name, no bits
        else if constexpr (has_value<T> && has_name<T> && !has_bits<T>) {
          layout.fields.push_back(
              {f.name, field_category::constant, 0,
               fixed ? std::optional<unsigned>(offset) : std::nullopt});
        }
        // save_position_type: has name, no bits, no value, no field
        else if constexpr (has_name<T> && !has_bits<T> && !has_value<T> &&
                           !has_field<T>) {
          layout.markers.push_back({f.name, offset});
        }
        // length_type / discriminant_type: metadata only
        else if constexpr (has_field<T> && !has_bits<T>) {
          // No wire presence
        }
      }

      // --- Pointer types (unique_ptr variant alternatives) ---

      template <typename T>
      void
      visit_ptr(const T& p) {
        if (!p) return;
        using pointee = typename T::element_type;

        // Discriminated choice: compute per-alternative layouts
        if constexpr (has_alternative<pointee>) {
          resolved_choice rc;
          rc.name = p->name;
          rc.discriminant_field = p->discriminant_field;
          rc.offset_bits = offset;

          unsigned max_bits = 0;
          bool all_same_size = true;
          bool first = true;
          unsigned first_size = 0;

          for (const auto& alt_ptr : p->alternative) {
            if (!alt_ptr) continue;

            resolved_alternative ra;
            ra.name = alt_ptr->name;
            ra.discriminant_value = alt_ptr->discriminant_value;

            // Compute layout for this alternative's fields starting
            // at the current offset.  Use a fresh walker sharing our
            // alignment but with independent offset and field list.
            unsigned alt_offset = offset;
            bool alt_fixed = true;
            resolved_layout alt_layout;
            layout_walker alt_walker{alt_layout, alt_offset, alt_fixed,
                                     alignment};

            for (const auto& item : alt_ptr->choice)
              std::visit(alt_walker, item);

            ra.fields = std::move(alt_layout.fields);
            ra.total_bits = alt_offset - offset;

            if (!alt_fixed) all_same_size = false;

            if (first) {
              first_size = ra.total_bits;
              first = false;
            } else if (ra.total_bits != first_size) {
              all_same_size = false;
            }

            if (ra.total_bits > max_bits) max_bits = ra.total_bits;

            rc.alternatives.push_back(std::move(ra));
          }

          bool has_alternatives = !rc.alternatives.empty();
          layout.choices.push_back(std::move(rc));

          if (all_same_size && has_alternatives) {
            // All alternatives have the same size — message stays fixed
            offset += first_size;
          } else {
            // Variable-size choice
            fixed = false;
            offset += max_bits;
          }
        }
        // repeat_type with element-type: record the repeat info
        else if constexpr (has_element_type<pointee>) {
          if (p->element_type.has_value()) {
            resolved_repeat rr;
            if (p->count_field.has_value()) rr.count_field = *p->count_field;
            rr.element_type = *p->element_type;
            rr.offset_bits = offset;
            // element_wire_bits will be resolved later by codegen
            // (it needs access to the referenced message's layout)
            layout.repeats.push_back(std::move(rr));
            // Repeat makes the message variable-length
            fixed = false;
          }
        }
        // group_type: emit a placeholder field (variable)
        else if constexpr (has_name<pointee>) {
          fixed = false;
          layout.fields.push_back(
              {p->name, field_category::data, 0, std::nullopt});
        }
      }
    };

  } // namespace detail

  template <typename Message, typename Defaults>
  resolved_layout
  compute_layout(const Message& msg, const Defaults& defaults) {
    resolved_layout layout;
    layout.message_name = msg.name;

    unsigned current_offset = 0;
    bool is_fixed = true;

    unsigned default_alignment = 0;
    if (defaults.alignment.has_value()) default_alignment = *defaults.alignment;

    detail::layout_walker walker{layout, current_offset, is_fixed,
                                 default_alignment};

    for (const auto& item : msg.choice)
      std::visit(walker, item);

    layout.is_fixed = is_fixed;
    layout.total_bits = current_offset;

    return layout;
  }

} // namespace xb::wire

#endif // XB_WIRE_LAYOUT_ENGINE_HPP
