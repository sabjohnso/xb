#include <xb/any_element.hpp>
#include <xb/xml_reader.hpp>
#include <xb/xml_writer.hpp>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace xb {

  any_element::any_element(xml_reader& reader) : name_(reader.name()) {
    for (std::size_t i = 0; i < reader.attribute_count(); ++i) {
      attributes_.emplace_back(reader.attribute_name(i),
                               std::string(reader.attribute_value(i)));
    }

    std::size_t start_depth = reader.depth();
    while (reader.read()) {
      switch (reader.node_type()) {
        case xml_node_type::start_element:
          children_.emplace_back(any_element(reader));
          break;
        case xml_node_type::characters:
          children_.emplace_back(std::string(reader.text()));
          break;
        case xml_node_type::end_element:
          if (reader.depth() == start_depth) { return; }
          break;
      }
    }
    throw std::runtime_error("unexpected end of input while parsing element '" +
                             name_.local_name() + "'");
  }

  namespace {

    using uri_prefix_map = std::unordered_map<std::string, std::string>;

    void
    write_element(const any_element& elem, xml_writer& writer,
                  uri_prefix_map declared, int& counter) {
      writer.start_element(elem.name());

      // Declare any namespace URIs not yet in scope.
      auto ensure = [&](const std::string& uri) {
        if (uri.empty() || declared.count(uri)) { return; }
        std::string pfx = "ns" + std::to_string(counter++);
        declared[uri] = pfx;
        writer.namespace_declaration(pfx, uri);
      };

      ensure(elem.name().namespace_uri());
      for (const auto& attr : elem.attributes()) {
        ensure(attr.name().namespace_uri());
      }

      for (const auto& attr : elem.attributes()) {
        writer.attribute(attr.name(), attr.value());
      }

      for (const auto& child : elem.children()) {
        std::visit(
            [&](const auto& v) {
              using T = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<T, std::string>) {
                writer.characters(v);
              } else {
                write_element(v, writer, declared, counter);
              }
            },
            child);
      }

      writer.end_element();
    }

  } // namespace

  void
  any_element::write(xml_writer& writer) const {
    int counter = 0;
    write_element(*this, writer, {}, counter);
  }

} // namespace xb
