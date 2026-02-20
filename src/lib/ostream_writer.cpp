#include <xb/ostream_writer.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xb {

  namespace {

    void
    escape_text(std::ostream& os, std::string_view text) {
      for (char c : text) {
        switch (c) {
        case '<':
          os << "&lt;";
          break;
        case '>':
          os << "&gt;";
          break;
        case '&':
          os << "&amp;";
          break;
        default:
          os << c;
          break;
        }
      }
    }

    void
    escape_attribute(std::ostream& os, std::string_view text) {
      for (char c : text) {
        switch (c) {
        case '<':
          os << "&lt;";
          break;
        case '>':
          os << "&gt;";
          break;
        case '&':
          os << "&amp;";
          break;
        case '"':
          os << "&quot;";
          break;
        default:
          os << c;
          break;
        }
      }
    }

  } // namespace

  struct ostream_writer::impl {
    std::ostream& os;

    // Namespace URI -> prefix mapping
    std::unordered_map<std::string, std::string> ns_prefixes;

    // Pending tag state: start_element() buffers its name; namespace_declaration()
    // and attribute() accumulate onto this buffer; the tag is flushed (written)
    // when child content arrives or end_element() is called.
    bool tag_pending = false;
    qname pending_name;

    struct pending_attr {
      qname name;
      std::string value;
    };
    std::vector<pending_attr> pending_attrs;

    struct pending_ns {
      std::string prefix;
      std::string uri;
    };
    std::vector<pending_ns> pending_ns_decls;

    struct element_frame {
      qname name;
      bool has_content;
    };
    std::vector<element_frame> stack;

    explicit impl(std::ostream& os) : os(os) {}

    void
    write_prefixed_name(const qname& name) {
      if (!name.namespace_uri.empty()) {
        auto it = ns_prefixes.find(name.namespace_uri);
        if (it != ns_prefixes.end() && !it->second.empty()) {
          os << it->second << ':';
        }
      }
      os << name.local_name;
    }

    // Write the buffered opening tag to the stream.
    void
    flush_pending_tag() {
      if (!tag_pending) {
        return;
      }
      tag_pending = false;

      os << '<';
      write_prefixed_name(pending_name);

      for (const auto& ns : pending_ns_decls) {
        if (ns.prefix.empty()) {
          os << " xmlns=\"";
        } else {
          os << " xmlns:" << ns.prefix << "=\"";
        }
        escape_attribute(os, ns.uri);
        os << '"';
      }

      for (const auto& attr : pending_attrs) {
        os << ' ';
        write_prefixed_name(attr.name);
        os << "=\"";
        escape_attribute(os, attr.value);
        os << '"';
      }

      pending_ns_decls.clear();
      pending_attrs.clear();
    }

    // Ensure the most recent open tag is flushed and closed with '>'.
    // Called before writing child content (text, child elements).
    void
    flush_and_close_tag() {
      if (tag_pending) {
        flush_pending_tag();
        os << '>';
        if (!stack.empty()) {
          stack.back().has_content = true;
        }
      }
    }
  };

  ostream_writer::ostream_writer(std::ostream& os) : impl_(std::make_unique<impl>(os)) {}

  ostream_writer::~ostream_writer() = default;
  ostream_writer::ostream_writer(ostream_writer&&) noexcept = default;
  ostream_writer& ostream_writer::operator=(ostream_writer&&) noexcept = default;

  void
  ostream_writer::start_element(const qname& name) {
    // Flush any previously open tag (it now has child content)
    impl_->flush_and_close_tag();

    // Mark parent as having content
    if (!impl_->stack.empty()) {
      impl_->stack.back().has_content = true;
    }

    impl_->stack.push_back({name, false});
    impl_->tag_pending = true;
    impl_->pending_name = name;
  }

  void
  ostream_writer::end_element() {
    auto frame = impl_->stack.back();
    impl_->stack.pop_back();

    if (impl_->tag_pending) {
      // Self-closing: no child content was written
      impl_->flush_pending_tag();
      impl_->os << "/>";
    } else {
      impl_->os << "</";
      impl_->write_prefixed_name(frame.name);
      impl_->os << '>';
    }
  }

  void
  ostream_writer::attribute(const qname& name, std::string_view value) {
    impl_->pending_attrs.push_back({name, std::string(value)});
  }

  void
  ostream_writer::characters(std::string_view text) {
    impl_->flush_and_close_tag();
    if (!impl_->stack.empty()) {
      impl_->stack.back().has_content = true;
    }
    escape_text(impl_->os, text);
  }

  void
  ostream_writer::namespace_declaration(std::string_view prefix, std::string_view uri) {
    // Register the prefix binding for element/attribute name lookups
    impl_->ns_prefixes[std::string(uri)] = std::string(prefix);

    // Buffer the xmlns declaration to be written when the tag is flushed
    impl_->pending_ns_decls.push_back({std::string(prefix), std::string(uri)});
  }

} // namespace xb
