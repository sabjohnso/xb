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

  struct resolved_layout {
    std::string message_name;
    std::vector<resolved_field> fields;
    std::vector<position_marker> markers;
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

        fixed = false;

        // group_type / choice_type: emit a placeholder field
        if constexpr (has_name<pointee>) {
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
    if (is_fixed) layout.total_bits = current_offset;

    return layout;
  }

} // namespace xb::wire

#endif // XB_WIRE_LAYOUT_ENGINE_HPP
