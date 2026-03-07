#include <xb/dtd_writer.hpp>

#include <sstream>

namespace xb {

  namespace {

    std::string
    quantifier_suffix(dtd::quantifier q) {
      switch (q) {
        case dtd::quantifier::one:
          return "";
        case dtd::quantifier::optional:
          return "?";
        case dtd::quantifier::zero_or_more:
          return "*";
        case dtd::quantifier::one_or_more:
          return "+";
      }
      return "";
    }

    void
    write_particle(std::ostream& os, const dtd::content_particle& cp) {
      if (cp.kind == dtd::particle_kind::name) {
        os << cp.name << quantifier_suffix(cp.quant);
        return;
      }
      std::string sep =
          (cp.kind == dtd::particle_kind::sequence) ? ", " : " | ";
      os << "(";
      for (std::size_t i = 0; i < cp.children.size(); ++i) {
        if (i > 0) os << sep;
        write_particle(os, cp.children[i]);
      }
      os << ")" << quantifier_suffix(cp.quant);
    }

    std::string
    attr_type_string(const dtd::attribute_def& ad) {
      switch (ad.type) {
        case dtd::attribute_type::cdata:
          return "CDATA";
        case dtd::attribute_type::id:
          return "ID";
        case dtd::attribute_type::idref:
          return "IDREF";
        case dtd::attribute_type::idrefs:
          return "IDREFS";
        case dtd::attribute_type::entity:
          return "ENTITY";
        case dtd::attribute_type::entities:
          return "ENTITIES";
        case dtd::attribute_type::nmtoken:
          return "NMTOKEN";
        case dtd::attribute_type::nmtokens:
          return "NMTOKENS";
        case dtd::attribute_type::notation: {
          std::string result = "NOTATION (";
          for (std::size_t i = 0; i < ad.enum_values.size(); ++i) {
            if (i > 0) result += " | ";
            result += ad.enum_values[i];
          }
          result += ")";
          return result;
        }
        case dtd::attribute_type::enumeration: {
          std::string result = "(";
          for (std::size_t i = 0; i < ad.enum_values.size(); ++i) {
            if (i > 0) result += " | ";
            result += ad.enum_values[i];
          }
          result += ")";
          return result;
        }
      }
      return "CDATA";
    }

    std::string
    attr_default_string(const dtd::attribute_def& ad) {
      switch (ad.dflt) {
        case dtd::default_kind::required:
          return "#REQUIRED";
        case dtd::default_kind::implied:
          return "#IMPLIED";
        case dtd::default_kind::fixed:
          return "#FIXED \"" + ad.default_value + "\"";
        case dtd::default_kind::value:
          return "\"" + ad.default_value + "\"";
      }
      return "#IMPLIED";
    }

  } // namespace

  std::string
  dtd_write(const dtd::document& doc) {
    std::ostringstream os;

    for (const auto& ed : doc.elements) {
      os << "<!ELEMENT " << ed.name << " ";
      switch (ed.content.kind) {
        case dtd::content_kind::empty:
          os << "EMPTY";
          break;
        case dtd::content_kind::any:
          os << "ANY";
          break;
        case dtd::content_kind::mixed:
          if (ed.content.mixed_names.empty()) {
            os << "(#PCDATA)";
          } else {
            os << "(#PCDATA";
            for (const auto& n : ed.content.mixed_names) {
              os << " | " << n;
            }
            os << ")*";
          }
          break;
        case dtd::content_kind::children:
          if (ed.content.particle) {
            write_particle(os, *ed.content.particle);
          } else {
            os << "EMPTY";
          }
          break;
      }
      os << ">\n";
    }

    for (const auto& al : doc.attlists) {
      os << "<!ATTLIST " << al.element_name;
      for (const auto& ad : al.attributes) {
        os << "\n  " << ad.name << " " << attr_type_string(ad) << " "
           << attr_default_string(ad);
      }
      os << ">\n";
    }

    return os.str();
  }

} // namespace xb
