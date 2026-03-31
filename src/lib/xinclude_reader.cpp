#include <xb/xinclude_reader.hpp>

#include <xb/expat_reader.hpp>

#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace xb {

  namespace {
    const std::string xi_ns = "http://www.w3.org/2001/XInclude";
  }

  // -- Include frame: one level of inclusion --

  struct include_frame {
    std::string content; // owned content string (reader references it)
    std::unique_ptr<expat_reader> reader;
    std::size_t depth_offset; // added to inner reader's depth
    std::string href;         // for circular detection
  };

  // -- Synthetic event: for text inclusion and fallback --

  struct synthetic_event {
    xml_node_type type;
    std::string text_content;
  };

  // -- Pimpl --

  struct xinclude_reader::impl {
    xml_reader* outer;
    xinclude_resolver resolver;
    std::vector<include_frame> stack;
    std::set<std::string> active_hrefs; // circular detection

    // When we need to emit a synthetic characters event (parse="text")
    std::optional<synthetic_event> pending_event;
    std::size_t pending_depth = 0;

    // Fallback buffering: when xi:include has xi:fallback children
    struct fallback_data {
      std::string content;
      bool available = false;
    };

    // Current active reader
    xml_reader&
    current() {
      if (!stack.empty()) return *stack.back().reader;
      return *outer;
    }

    std::size_t
    depth_offset() const {
      if (!stack.empty()) return stack.back().depth_offset;
      return 0;
    }

    bool
    has_pending() const {
      return pending_event.has_value();
    }

    // Skip the xi:include element and its children on the outer/current reader.
    void
    skip_include_element(xml_reader& reader) {
      std::size_t start_depth = reader.depth();
      while (reader.read()) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == start_depth)
          return;
      }
    }

    // Collect fallback content from inside an xi:include element.
    // We're positioned at the start_element of xi:include.
    // Returns the fallback content string if xi:fallback is found.
    fallback_data
    collect_fallback(xml_reader& reader) {
      fallback_data result;
      std::size_t include_depth = reader.depth();
      std::string fallback_xml;
      bool in_fallback = false;
      std::size_t fallback_depth = 0;

      while (reader.read()) {
        if (reader.node_type() == xml_node_type::end_element &&
            reader.depth() == include_depth) {
          // End of xi:include
          break;
        }

        if (reader.node_type() == xml_node_type::start_element &&
            reader.name().namespace_uri() == xi_ns &&
            reader.name().local_name() == "fallback") {
          in_fallback = true;
          fallback_depth = reader.depth();
          continue;
        }

        if (in_fallback) {
          if (reader.node_type() == xml_node_type::end_element &&
              reader.depth() == fallback_depth) {
            in_fallback = false;
            result.available = true;
            continue;
          }

          // Accumulate fallback content as XML
          switch (reader.node_type()) {
            case xml_node_type::start_element: {
              fallback_xml += "<" + reader.name().local_name();
              for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
                fallback_xml += " " + reader.attribute_name(i).local_name() +
                                "=\"" + std::string(reader.attribute_value(i)) +
                                "\"";
              }
              fallback_xml += ">";
              break;
            }
            case xml_node_type::end_element:
              fallback_xml += "</" + reader.name().local_name() + ">";
              break;
            case xml_node_type::characters:
              fallback_xml += std::string(reader.text());
              break;
          }
        }
      }

      if (result.available) result.content = fallback_xml;
      return result;
    }

    // Process an xi:include element. Returns true if events are ready.
    bool
    process_include(xml_reader& reader) {
      std::string href;
      std::string parse_mode = "xml";

      for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
        auto attr_local = reader.attribute_name(i).local_name();
        if (attr_local == "href")
          href = std::string(reader.attribute_value(i));
        else if (attr_local == "parse")
          parse_mode = std::string(reader.attribute_value(i));
      }

      std::size_t include_depth = reader.depth();
      std::size_t target_depth = include_depth - 1 + depth_offset();

      // Try to resolve
      std::string content;
      try {
        if (active_hrefs.count(href))
          throw std::runtime_error("xinclude: circular include detected: " +
                                   href);
        content = resolver(href);
      } catch (...) {
        // Check for fallback
        auto fb = collect_fallback(reader);
        if (fb.available) {
          // Parse fallback content as XML events
          auto frame = std::make_unique<include_frame>();
          frame->content = "<_xb_fb_root>" + fb.content + "</_xb_fb_root>";
          frame->reader = std::make_unique<expat_reader>(frame->content);
          frame->depth_offset = target_depth;
          frame->href = "";
          // Skip the synthetic root element
          frame->reader->read(); // start _xb_fb_root
          stack.push_back(std::move(*frame));
          return false; // next read() will get first real event
        }
        throw; // no fallback, propagate error
      }

      // Skip the xi:include element and its children
      skip_include_element(reader);

      if (parse_mode == "text") {
        // Emit as a characters event
        pending_event =
            synthetic_event{xml_node_type::characters, std::move(content)};
        pending_depth = target_depth + 1;
        return true;
      }

      // parse="xml" — push a new include frame
      active_hrefs.insert(href);
      auto frame = std::make_unique<include_frame>();
      frame->content = std::move(content);
      frame->reader = std::make_unique<expat_reader>(frame->content);
      frame->depth_offset = target_depth;
      frame->href = href;
      stack.push_back(std::move(*frame));
      return false; // next read() will get first event from inner reader
    }

    // Pop the current include frame.
    void
    pop_frame() {
      if (stack.empty()) return;
      auto& frame = stack.back();
      if (!frame.href.empty()) active_hrefs.erase(frame.href);
      stack.pop_back();
    }
  };

  // -- Public interface --

  xinclude_reader::xinclude_reader(xml_reader& inner,
                                   xinclude_resolver resolver)
      : impl_(std::make_unique<impl>()) {
    impl_->outer = &inner;
    impl_->resolver = std::move(resolver);
  }

  // Need explicit destructor for pimpl with unique_ptr.
  xinclude_reader::~xinclude_reader() = default;

  bool
  xinclude_reader::read() {
    // Clear previously delivered pending event
    if (impl_->pending_event) {
      impl_->pending_event.reset();
      // Fall through to read from the actual reader
    }

    for (;;) {
      auto& reader = impl_->current();

      if (!reader.read()) {
        // Inner reader exhausted
        if (impl_->stack.empty()) return false; // truly done

        // Pop the include frame and continue with outer reader
        impl_->pop_frame();
        continue;
      }

      // Check for xi:include
      if (reader.node_type() == xml_node_type::start_element &&
          reader.name().namespace_uri() == xi_ns &&
          reader.name().local_name() == "include") {
        bool has_synthetic = impl_->process_include(reader);
        if (has_synthetic) return true; // pending text event ready
        continue;                       // events will come from new frame
      }

      // Skip xi:fallback end elements and synthetic roots from fallback
      if (reader.node_type() == xml_node_type::end_element &&
          reader.name().local_name() == "_xb_fb_root") {
        // End of fallback synthetic root — pop frame
        impl_->pop_frame();
        continue;
      }

      // Normal event — pass through
      return true;
    }
  }

  xml_node_type
  xinclude_reader::node_type() const {
    if (impl_->pending_event) return impl_->pending_event->type;
    return impl_->current().node_type();
  }

  const qname&
  xinclude_reader::name() const {
    static const qname empty_name{"", ""};
    if (impl_->pending_event) return empty_name;
    return impl_->current().name();
  }

  std::size_t
  xinclude_reader::attribute_count() const {
    if (impl_->pending_event) return 0;
    return impl_->current().attribute_count();
  }

  const qname&
  xinclude_reader::attribute_name(std::size_t index) const {
    return impl_->current().attribute_name(index);
  }

  std::string_view
  xinclude_reader::attribute_value(std::size_t index) const {
    return impl_->current().attribute_value(index);
  }

  std::string_view
  xinclude_reader::attribute_value(const qname& attr_name) const {
    return impl_->current().attribute_value(attr_name);
  }

  std::string_view
  xinclude_reader::text() const {
    if (impl_->pending_event) return impl_->pending_event->text_content;
    return impl_->current().text();
  }

  std::size_t
  xinclude_reader::depth() const {
    if (impl_->pending_event) return impl_->pending_depth;
    auto& reader = impl_->current();
    return reader.depth() + impl_->depth_offset();
  }

  std::string_view
  xinclude_reader::namespace_uri_for_prefix(std::string_view prefix) const {
    return impl_->current().namespace_uri_for_prefix(prefix);
  }

  std::size_t
  xinclude_reader::line() const {
    return impl_->current().line();
  }

  std::size_t
  xinclude_reader::column() const {
    return impl_->current().column();
  }

} // namespace xb
