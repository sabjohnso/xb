#include <xb/test_doc_generator.hpp>

#include <xb/ostream_writer.hpp>
#include <xb/test_value_generator.hpp>
#include <xb/test_vector_generator.hpp>

#include <sstream>
#include <string>
#include <unordered_map>

namespace xb {

  namespace {

    class xml_renderer {
      xml_writer* writer_ = nullptr;
      mutable std::unordered_map<std::string, std::string> ns_prefixes_;
      mutable std::size_t prefix_counter_ = 0;

      void
      ensure_namespace(const qname& name) {
        const auto& uri = name.namespace_uri();
        if (uri.empty()) return;
        if (ns_prefixes_.count(uri)) return;

        std::string prefix;
        if (ns_prefixes_.empty()) {
          prefix = "";
        } else {
          prefix = "ns" + std::to_string(prefix_counter_++);
        }
        ns_prefixes_[uri] = prefix;
        writer_->namespace_declaration(prefix, uri);
      }

      void
      write_field(const test_field_value& field, const qname& element_name) {
        std::visit(
            [&](const auto& content) {
              using T = std::decay_t<decltype(content)>;
              if constexpr (std::is_same_v<T, std::string>) {
                // Simple value: write as child element or attribute
                writer_->start_element(field.field_name);
                ensure_namespace(field.field_name);
                writer_->characters(content);
                writer_->end_element();
              } else if constexpr (std::is_same_v<
                                       T, std::vector<test_field_value>>) {
                // Nested complex type
                writer_->start_element(field.field_name);
                ensure_namespace(field.field_name);
                for (const auto& child : content)
                  write_field(child, field.field_name);
                writer_->end_element();
              } else if constexpr (std::is_same_v<T, std::vector<std::vector<
                                                         test_field_value>>>) {
                // Repeated elements
                for (const auto& instance : content) {
                  for (const auto& f : instance) {
                    write_field(f, element_name);
                  }
                }
              }
            },
            field.content);
      }

    public:
      xml_renderer() = default;

      std::string
      render(const test_vector& vec, const qname& element_name) {
        std::ostringstream oss;
        {
          ostream_writer writer(oss);
          writer_ = &writer;
          ns_prefixes_.clear();
          prefix_counter_ = 0;

          writer_->start_element(element_name);
          ensure_namespace(element_name);

          for (const auto& field : vec.fields)
            write_field(field, element_name);

          writer_->end_element();
        }
        return oss.str();
      }
    };

  } // namespace

  test_doc_generator::test_doc_generator(const schema_set& schemas)
      : schemas_(schemas) {}

  void
  test_doc_generator::generate(
      const qname& element_name,
      std::function<void(const std::string& xml, const std::string& label)>
          sink,
      const test_vector_options& opts) const {
    test_value_generator val_gen(schemas_);
    test_vector_generator vec_gen(schemas_, val_gen);

    auto vectors = vec_gen.generate(element_name, opts);
    xml_renderer renderer;

    for (const auto& vec : vectors) {
      std::string xml = renderer.render(vec, element_name);
      sink(xml, vec.label);
    }
  }

  std::size_t
  test_doc_generator::count(const qname& element_name,
                            const test_vector_options& opts) const {
    test_value_generator val_gen(schemas_);
    test_vector_generator vec_gen(schemas_, val_gen);
    return vec_gen.generate(element_name, opts).size();
  }

} // namespace xb
