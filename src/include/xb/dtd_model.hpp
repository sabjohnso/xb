#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xb::dtd {

  // -- Content model particles ------------------------------------------------

  enum class quantifier { one, optional, zero_or_more, one_or_more };

  enum class particle_kind { name, sequence, choice };

  struct content_particle {
    particle_kind kind = particle_kind::name;
    std::string name;
    quantifier quantifier = quantifier::one;
    std::vector<content_particle> children;
  };

  // -- Content specification --------------------------------------------------

  enum class content_kind { empty, any, mixed, children };

  struct content_spec {
    content_kind kind = content_kind::empty;
    std::optional<content_particle> particle;
    std::vector<std::string> mixed_names;
  };

  // -- Attribute definitions --------------------------------------------------

  enum class attribute_type {
    cdata,
    id,
    idref,
    idrefs,
    entity,
    entities,
    nmtoken,
    nmtokens,
    notation,
    enumeration
  };

  enum class default_kind { required, implied, fixed, value };

  struct attribute_def {
    std::string name;
    attribute_type type = attribute_type::cdata;
    std::vector<std::string> enum_values;
    default_kind default_kind = default_kind::implied;
    std::string default_value;
  };

  // -- Declarations -----------------------------------------------------------

  struct element_decl {
    std::string name;
    content_spec content;
  };

  struct attlist_decl {
    std::string element_name;
    std::vector<attribute_def> attributes;
  };

  struct entity_decl {
    std::string name;
    bool is_parameter = false;
    std::string value;
    std::string system_id;
    std::string public_id;
  };

  // -- Document ---------------------------------------------------------------

  struct document {
    std::vector<element_decl> elements;
    std::vector<attlist_decl> attlists;
    std::vector<entity_decl> entities;
  };

} // namespace xb::dtd
