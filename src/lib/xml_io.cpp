#include <xb/xml_io.hpp>

namespace xb {

  std::string
  read_text(xml_reader& reader) {
    std::string result;
    auto start_depth = reader.depth();

    while (reader.read()) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == start_depth)
        break;
      if (reader.node_type() == xml_node_type::characters)
        result += std::string(reader.text());
    }

    return result;
  }

  void
  skip_element(xml_reader& reader) {
    auto start_depth = reader.depth();

    while (reader.read()) {
      if (reader.node_type() == xml_node_type::end_element &&
          reader.depth() == start_depth)
        return;
    }
  }

} // namespace xb
