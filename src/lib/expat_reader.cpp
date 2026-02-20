#include <xb/expat_reader.hpp>

#include <expat.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xb {

  namespace {

    struct attribute {
      qname name;
      std::string value;
    };

    struct event {
      xml_node_type type;
      qname name;
      std::string text;
      std::vector<attribute> attributes;
      std::size_t depth;
    };

    // Parse "uri\nlocal" into a qname. Unqualified names have no separator.
    qname
    parse_expat_name(const char* expat_name) {
      const char* sep = std::strchr(expat_name, '\n');
      if (sep == nullptr) {
        return qname{"", std::string(expat_name)};
      }
      return qname{std::string(expat_name, sep), std::string(sep + 1)};
    }

  } // namespace

  struct expat_reader::impl {
    std::vector<event> events;
    std::size_t cursor = 0;
    std::size_t current_depth = 0;

    static void XMLCALL
    on_start_element(void* user_data, const char* name, const char** atts) {
      auto* self = static_cast<impl*>(user_data);
      self->current_depth++;

      event ev;
      ev.type = xml_node_type::start_element;
      ev.name = parse_expat_name(name);
      ev.depth = self->current_depth;

      for (const char** p = atts; *p != nullptr; p += 2) {
        ev.attributes.push_back({parse_expat_name(p[0]), std::string(p[1])});
      }

      self->events.push_back(std::move(ev));
    }

    static void XMLCALL
    on_end_element(void* user_data, const char* name) {
      auto* self = static_cast<impl*>(user_data);

      event ev;
      ev.type = xml_node_type::end_element;
      ev.name = parse_expat_name(name);
      ev.depth = self->current_depth;

      self->events.push_back(std::move(ev));
      self->current_depth--;
    }

    static void XMLCALL
    on_character_data(void* user_data, const char* s, int len) {
      auto* self = static_cast<impl*>(user_data);

      // Coalesce adjacent character data into a single event
      if (!self->events.empty() &&
          self->events.back().type == xml_node_type::characters) {
        self->events.back().text.append(s, static_cast<std::size_t>(len));
        return;
      }

      event ev;
      ev.type = xml_node_type::characters;
      ev.text.assign(s, static_cast<std::size_t>(len));
      ev.depth = self->current_depth;
      self->events.push_back(std::move(ev));
    }
  };

  expat_reader::expat_reader(std::string_view xml) : impl_(std::make_unique<impl>()) {
    // '\n' as the namespace separator
    XML_Parser parser = XML_ParserCreateNS(nullptr, '\n');
    if (parser == nullptr) {
      throw std::runtime_error("failed to create expat parser");
    }

    XML_SetUserData(parser, impl_.get());
    XML_SetElementHandler(parser, impl::on_start_element, impl::on_end_element);
    XML_SetCharacterDataHandler(parser, impl::on_character_data);

    XML_Status status =
        XML_Parse(parser, xml.data(), static_cast<int>(xml.size()), XML_TRUE);

    if (status == XML_STATUS_ERROR) {
      std::string msg = "XML parse error at line ";
      msg += std::to_string(XML_GetCurrentLineNumber(parser));
      msg += ": ";
      msg += XML_ErrorString(XML_GetErrorCode(parser));
      XML_ParserFree(parser);
      throw std::runtime_error(msg);
    }

    XML_ParserFree(parser);

    if (impl_->events.empty()) {
      throw std::runtime_error("XML parse error: no content");
    }
  }

  expat_reader::~expat_reader() = default;
  expat_reader::expat_reader(expat_reader&&) noexcept = default;
  expat_reader& expat_reader::operator=(expat_reader&&) noexcept = default;

  bool
  expat_reader::read() {
    if (impl_->cursor >= impl_->events.size()) {
      return false;
    }
    impl_->cursor++;
    return impl_->cursor <= impl_->events.size();
  }

  xml_node_type
  expat_reader::node_type() const {
    return impl_->events[impl_->cursor - 1].type;
  }

  const qname&
  expat_reader::name() const {
    return impl_->events[impl_->cursor - 1].name;
  }

  std::size_t
  expat_reader::attribute_count() const {
    return impl_->events[impl_->cursor - 1].attributes.size();
  }

  const qname&
  expat_reader::attribute_name(std::size_t index) const {
    return impl_->events[impl_->cursor - 1].attributes[index].name;
  }

  std::string_view
  expat_reader::attribute_value(std::size_t index) const {
    return impl_->events[impl_->cursor - 1].attributes[index].value;
  }

  std::string_view
  expat_reader::attribute_value(const qname& attr_name) const {
    const auto& attrs = impl_->events[impl_->cursor - 1].attributes;
    for (const auto& attr : attrs) {
      if (attr.name == attr_name) {
        return attr.value;
      }
    }
    return {};
  }

  std::string_view
  expat_reader::text() const {
    return impl_->events[impl_->cursor - 1].text;
  }

  std::size_t
  expat_reader::depth() const {
    return impl_->events[impl_->cursor - 1].depth;
  }

} // namespace xb
