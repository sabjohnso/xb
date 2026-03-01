#include <xb/dtd_parser.hpp>

#include <stdexcept>
#include <unordered_map>

namespace xb {

  namespace {

    // -------------------------------------------------------------------------
    // Lexer
    // -------------------------------------------------------------------------

    enum class token_type {
      eof,
      element_decl,  // <!ELEMENT
      attlist_decl,  // <!ATTLIST
      entity_decl,   // <!ENTITY
      notation_decl, // <!NOTATION
      name,
      literal,     // "..." or '...'
      open_paren,  // (
      close_paren, // )
      pipe,        // |
      comma,       // ,
      star,        // *
      plus,        // +
      question,    // ?
      percent,     // %
      semicolon,   // ;
      close_angle, // >
      kw_empty,    // EMPTY
      kw_any,      // ANY
      kw_pcdata,   // #PCDATA
      kw_cdata,    // CDATA
      kw_id,       // ID
      kw_idref,    // IDREF
      kw_idrefs,   // IDREFS
      kw_entity,   // ENTITY
      kw_entities, // ENTITIES
      kw_nmtoken,  // NMTOKEN
      kw_nmtokens, // NMTOKENS
      kw_notation, // NOTATION
      kw_required, // #REQUIRED
      kw_implied,  // #IMPLIED
      kw_fixed,    // #FIXED
      kw_system,   // SYSTEM
      kw_public,   // PUBLIC
      kw_ndata,    // NDATA
    };

    struct token {
      token_type type = token_type::eof;
      std::string value;
    };

    class lexer {
      std::string input_;
      std::size_t pos_ = 0;

    public:
      explicit lexer(std::string input) : input_(std::move(input)) {}

      void
      reset(std::string input) {
        input_ = std::move(input);
        pos_ = 0;
      }

      token
      next() {
        skip_ws();
        if (pos_ >= input_.size()) return {token_type::eof, {}};

        char c = input_[pos_];

        // Comments: <!-- ... -->
        if (starts_with("<!--")) {
          skip_comment();
          return next();
        }

        // Processing instructions: <? ... ?>
        if (starts_with("<?")) {
          skip_pi();
          return next();
        }

        // Declaration keywords
        if (starts_with("<!ELEMENT")) {
          pos_ += 9;
          return {token_type::element_decl, "<!ELEMENT"};
        }
        if (starts_with("<!ATTLIST")) {
          pos_ += 9;
          return {token_type::attlist_decl, "<!ATTLIST"};
        }
        if (starts_with("<!ENTITY")) {
          pos_ += 8;
          return {token_type::entity_decl, "<!ENTITY"};
        }
        if (starts_with("<!NOTATION")) {
          pos_ += 10;
          return {token_type::notation_decl, "<!NOTATION"};
        }

        // Hash keywords
        if (starts_with("#PCDATA")) {
          pos_ += 7;
          return {token_type::kw_pcdata, "#PCDATA"};
        }
        if (starts_with("#REQUIRED")) {
          pos_ += 9;
          return {token_type::kw_required, "#REQUIRED"};
        }
        if (starts_with("#IMPLIED")) {
          pos_ += 8;
          return {token_type::kw_implied, "#IMPLIED"};
        }
        if (starts_with("#FIXED")) {
          pos_ += 6;
          return {token_type::kw_fixed, "#FIXED"};
        }

        // Single-char tokens
        switch (c) {
          case '(':
            ++pos_;
            return {token_type::open_paren, "("};
          case ')':
            ++pos_;
            return {token_type::close_paren, ")"};
          case '|':
            ++pos_;
            return {token_type::pipe, "|"};
          case ',':
            ++pos_;
            return {token_type::comma, ","};
          case '*':
            ++pos_;
            return {token_type::star, "*"};
          case '+':
            ++pos_;
            return {token_type::plus, "+"};
          case '?':
            ++pos_;
            return {token_type::question, "?"};
          case '%':
            ++pos_;
            return {token_type::percent, "%"};
          case ';':
            ++pos_;
            return {token_type::semicolon, ";"};
          case '>':
            ++pos_;
            return {token_type::close_angle, ">"};
          default:
            break;
        }

        // String literals
        if (c == '"' || c == '\'') return read_literal();

        // Names / keywords
        if (is_name_start(c)) return read_name_or_keyword();

        throw std::runtime_error(
            std::string("dtd_parser: unexpected character '") + c + "'");
      }

      token
      peek() {
        auto saved = pos_;
        auto t = next();
        pos_ = saved;
        return t;
      }

    private:
      bool
      starts_with(const char* prefix) const {
        std::size_t len = 0;
        while (prefix[len]) {
          if (pos_ + len >= input_.size() || input_[pos_ + len] != prefix[len])
            return false;
          ++len;
        }
        return true;
      }

      void
      skip_ws() {
        while (pos_ < input_.size() &&
               (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                input_[pos_] == '\n' || input_[pos_] == '\r'))
          ++pos_;
      }

      void
      skip_comment() {
        pos_ += 4; // skip <!--
        while (pos_ + 2 < input_.size()) {
          if (input_[pos_] == '-' && input_[pos_ + 1] == '-' &&
              input_[pos_ + 2] == '>') {
            pos_ += 3;
            return;
          }
          ++pos_;
        }
        throw std::runtime_error("dtd_parser: unterminated comment");
      }

      void
      skip_pi() {
        pos_ += 2; // skip <?
        while (pos_ + 1 < input_.size()) {
          if (input_[pos_] == '?' && input_[pos_ + 1] == '>') {
            pos_ += 2;
            return;
          }
          ++pos_;
        }
        throw std::runtime_error(
            "dtd_parser: unterminated processing instruction");
      }

      static bool
      is_name_start(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
               c == ':';
      }

      static bool
      is_name_char(char c) {
        return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' ||
               c == '.';
      }

      token
      read_literal() {
        char quote = input_[pos_++];
        std::string val;
        while (pos_ < input_.size() && input_[pos_] != quote)
          val += input_[pos_++];
        if (pos_ >= input_.size())
          throw std::runtime_error("dtd_parser: unterminated string literal");
        ++pos_; // closing quote
        return {token_type::literal, std::move(val)};
      }

      token
      read_name_or_keyword() {
        auto start = pos_;
        while (pos_ < input_.size() && is_name_char(input_[pos_]))
          ++pos_;
        std::string val = input_.substr(start, pos_ - start);

        // Map to keywords
        if (val == "EMPTY") return {token_type::kw_empty, val};
        if (val == "ANY") return {token_type::kw_any, val};
        if (val == "CDATA") return {token_type::kw_cdata, val};
        if (val == "ID") return {token_type::kw_id, val};
        if (val == "IDREF") return {token_type::kw_idref, val};
        if (val == "IDREFS") return {token_type::kw_idrefs, val};
        if (val == "ENTITY") return {token_type::kw_entity, val};
        if (val == "ENTITIES") return {token_type::kw_entities, val};
        if (val == "NMTOKEN") return {token_type::kw_nmtoken, val};
        if (val == "NMTOKENS") return {token_type::kw_nmtokens, val};
        if (val == "NOTATION") return {token_type::kw_notation, val};
        if (val == "SYSTEM") return {token_type::kw_system, val};
        if (val == "PUBLIC") return {token_type::kw_public, val};
        if (val == "NDATA") return {token_type::kw_ndata, val};

        return {token_type::name, std::move(val)};
      }
    };

    // -------------------------------------------------------------------------
    // Parser
    // -------------------------------------------------------------------------

    class parser {
      lexer lex_;
      token current_;
      std::unordered_map<std::string, std::string> param_entities_;
      dtd::document result_;

    public:
      explicit parser(const std::string& source) : lex_(source) { advance(); }

      dtd::document
      parse() {
        while (current_.type != token_type::eof) {
          parse_declaration();
        }
        return std::move(result_);
      }

    private:
      void
      advance() {
        current_ = lex_.next();
      }

      void
      expect(token_type t) {
        if (current_.type != t)
          throw std::runtime_error("dtd_parser: unexpected token '" +
                                   current_.value + "'");
        advance();
      }

      std::string
      expect_name() {
        if (current_.type != token_type::name)
          throw std::runtime_error("dtd_parser: expected name, got '" +
                                   current_.value + "'");
        auto val = std::move(current_.value);
        advance();
        return val;
      }

      std::string
      expect_literal() {
        if (current_.type != token_type::literal)
          throw std::runtime_error("dtd_parser: expected literal, got '" +
                                   current_.value + "'");
        auto val = std::move(current_.value);
        advance();
        return val;
      }

      // Expand parameter entity references in a string, then re-lex
      std::string
      expand_param_refs(const std::string& text) {
        std::string result;
        std::size_t i = 0;
        int depth = 0;
        while (i < text.size()) {
          if (text[i] == '%') {
            ++i;
            std::string name;
            while (i < text.size() && text[i] != ';')
              name += text[i++];
            if (i < text.size()) ++i; // skip ;
            auto it = param_entities_.find(name);
            if (it != param_entities_.end()) {
              if (depth > 10)
                throw std::runtime_error(
                    "dtd_parser: parameter entity expansion depth exceeded");
              // recursive expansion not needed for our use cases, but guard
              result += it->second;
            } else {
              // Unknown entity — put it back verbatim
              result += "%" + name + ";";
            }
          } else {
            result += text[i++];
          }
        }
        (void)depth;
        return result;
      }

      void
      parse_declaration() {
        if (current_.type == token_type::element_decl) {
          parse_element_decl();
        } else if (current_.type == token_type::attlist_decl) {
          parse_attlist_decl();
        } else if (current_.type == token_type::entity_decl) {
          parse_entity_decl();
        } else if (current_.type == token_type::notation_decl) {
          skip_to_close_angle();
        } else {
          throw std::runtime_error("dtd_parser: unexpected token '" +
                                   current_.value + "'");
        }
      }

      void
      skip_to_close_angle() {
        while (current_.type != token_type::close_angle &&
               current_.type != token_type::eof)
          advance();
        if (current_.type == token_type::close_angle) advance();
      }

      // <!ELEMENT name content_spec>
      void
      parse_element_decl() {
        advance(); // skip <!ELEMENT
        auto name = expect_name();
        auto cs = parse_content_spec();
        expect(token_type::close_angle);

        dtd::element_decl ed;
        ed.name = std::move(name);
        ed.content = std::move(cs);
        result_.elements.push_back(std::move(ed));
      }

      dtd::content_spec
      parse_content_spec() {
        if (current_.type == token_type::kw_empty) {
          advance();
          return {dtd::content_kind::empty, std::nullopt, {}};
        }
        if (current_.type == token_type::kw_any) {
          advance();
          return {dtd::content_kind::any, std::nullopt, {}};
        }
        if (current_.type == token_type::open_paren) {
          return parse_content_model();
        }
        throw std::runtime_error("dtd_parser: expected content spec");
      }

      // Parse ( ... ) content model — may be mixed or children
      dtd::content_spec
      parse_content_model() {
        expect(token_type::open_paren);

        // Check for mixed content: starts with #PCDATA
        if (current_.type == token_type::kw_pcdata) {
          return parse_mixed_content();
        }

        // Children content model
        auto cp = parse_group_content();
        dtd::content_spec cs;
        cs.kind = dtd::content_kind::children;
        cs.particle = std::move(cp);
        return cs;
      }

      // (#PCDATA) or (#PCDATA | name1 | name2)*
      dtd::content_spec
      parse_mixed_content() {
        advance(); // skip #PCDATA
        dtd::content_spec cs;
        cs.kind = dtd::content_kind::mixed;

        if (current_.type == token_type::close_paren) {
          advance(); // skip )
          // Optional * after (#PCDATA)
          if (current_.type == token_type::star) advance();
          return cs;
        }

        // (#PCDATA | name1 | name2)*
        while (current_.type == token_type::pipe) {
          advance(); // skip |

          // Handle parameter entity reference in mixed content
          if (current_.type == token_type::percent) {
            advance(); // skip %
            auto ent_name = expect_name();
            expect(token_type::semicolon);

            // Expand the entity and parse names from it
            auto it = param_entities_.find(ent_name);
            if (it != param_entities_.end()) {
              // Parse names from expanded text
              auto expanded = it->second;
              // The expanded text is like "em | strong"
              // Parse names separated by |
              std::size_t i = 0;
              while (i < expanded.size()) {
                // skip whitespace
                while (i < expanded.size() &&
                       (expanded[i] == ' ' || expanded[i] == '\t'))
                  ++i;
                if (i >= expanded.size()) break;
                if (expanded[i] == '|') {
                  ++i;
                  continue;
                }
                std::string n;
                while (i < expanded.size() && expanded[i] != ' ' &&
                       expanded[i] != '|' && expanded[i] != '\t')
                  n += expanded[i++];
                if (!n.empty()) cs.mixed_names.push_back(std::move(n));
              }
            }
          } else {
            cs.mixed_names.push_back(expect_name());
          }
        }

        expect(token_type::close_paren);
        // Must be followed by *
        if (current_.type == token_type::star) advance();
        return cs;
      }

      // Parse inside ( ... ) for children content
      dtd::content_particle
      parse_group_content() {
        // Already consumed '('
        auto first = parse_cp();

        // Determine separator: , or |
        if (current_.type == token_type::comma) {
          dtd::content_particle group;
          group.kind = dtd::particle_kind::sequence;
          group.children.push_back(std::move(first));
          while (current_.type == token_type::comma) {
            advance(); // skip ,
            group.children.push_back(parse_cp());
          }
          expect(token_type::close_paren);
          group.quantifier = parse_quantifier();
          return group;
        } else if (current_.type == token_type::pipe) {
          dtd::content_particle group;
          group.kind = dtd::particle_kind::choice;
          group.children.push_back(std::move(first));
          while (current_.type == token_type::pipe) {
            advance(); // skip |
            group.children.push_back(parse_cp());
          }
          expect(token_type::close_paren);
          group.quantifier = parse_quantifier();
          return group;
        } else {
          // Single child in parens: (a) or (a+)
          expect(token_type::close_paren);
          auto q = parse_quantifier();
          dtd::content_particle group;
          group.kind = dtd::particle_kind::sequence;
          group.children.push_back(std::move(first));
          group.quantifier = q;
          return group;
        }
      }

      // Parse a single content particle: name or (group)
      dtd::content_particle
      parse_cp() {
        if (current_.type == token_type::open_paren) {
          advance(); // skip (
          auto inner = parse_group_content();
          // inner already consumed ) and quantifier
          return inner;
        }

        // Simple name
        dtd::content_particle cp;
        cp.kind = dtd::particle_kind::name;
        cp.name = expect_name();
        cp.quantifier = parse_quantifier();
        return cp;
      }

      dtd::quantifier
      parse_quantifier() {
        if (current_.type == token_type::star) {
          advance();
          return dtd::quantifier::zero_or_more;
        }
        if (current_.type == token_type::plus) {
          advance();
          return dtd::quantifier::one_or_more;
        }
        if (current_.type == token_type::question) {
          advance();
          return dtd::quantifier::optional;
        }
        return dtd::quantifier::one;
      }

      // <!ATTLIST element_name att_def+ >
      void
      parse_attlist_decl() {
        advance(); // skip <!ATTLIST
        auto element_name = expect_name();

        dtd::attlist_decl al;
        al.element_name = std::move(element_name);

        while (current_.type != token_type::close_angle &&
               current_.type != token_type::eof) {
          al.attributes.push_back(parse_attribute_def());
        }
        expect(token_type::close_angle);
        result_.attlists.push_back(std::move(al));
      }

      dtd::attribute_def
      parse_attribute_def() {
        dtd::attribute_def ad;
        ad.name = expect_name();
        ad.type = parse_attribute_type(ad.enum_values);
        parse_default_decl(ad);
        return ad;
      }

      dtd::attribute_type
      parse_attribute_type(std::vector<std::string>& enum_vals) {
        switch (current_.type) {
          case token_type::kw_cdata:
            advance();
            return dtd::attribute_type::cdata;
          case token_type::kw_id:
            advance();
            return dtd::attribute_type::id;
          case token_type::kw_idref:
            advance();
            return dtd::attribute_type::idref;
          case token_type::kw_idrefs:
            advance();
            return dtd::attribute_type::idrefs;
          case token_type::kw_entity:
            advance();
            return dtd::attribute_type::entity;
          case token_type::kw_entities:
            advance();
            return dtd::attribute_type::entities;
          case token_type::kw_nmtoken:
            advance();
            return dtd::attribute_type::nmtoken;
          case token_type::kw_nmtokens:
            advance();
            return dtd::attribute_type::nmtokens;
          case token_type::kw_notation:
            advance();
            // NOTATION ( name | name )
            expect(token_type::open_paren);
            enum_vals.push_back(expect_name());
            while (current_.type == token_type::pipe) {
              advance();
              enum_vals.push_back(expect_name());
            }
            expect(token_type::close_paren);
            return dtd::attribute_type::notation;
          case token_type::open_paren:
            // Enumeration: ( val1 | val2 )
            advance();
            enum_vals.push_back(expect_name());
            while (current_.type == token_type::pipe) {
              advance();
              enum_vals.push_back(expect_name());
            }
            expect(token_type::close_paren);
            return dtd::attribute_type::enumeration;
          default:
            throw std::runtime_error(
                "dtd_parser: expected attribute type, got '" + current_.value +
                "'");
        }
      }

      void
      parse_default_decl(dtd::attribute_def& ad) {
        if (current_.type == token_type::kw_required) {
          advance();
          ad.default_kind = dtd::default_kind::required;
        } else if (current_.type == token_type::kw_implied) {
          advance();
          ad.default_kind = dtd::default_kind::implied;
        } else if (current_.type == token_type::kw_fixed) {
          advance();
          ad.default_kind = dtd::default_kind::fixed;
          ad.default_value = expect_literal();
        } else if (current_.type == token_type::literal) {
          ad.default_kind = dtd::default_kind::value;
          ad.default_value = expect_literal();
        } else {
          ad.default_kind = dtd::default_kind::implied;
        }
      }

      // <!ENTITY [%] name literal_or_external >
      void
      parse_entity_decl() {
        advance(); // skip <!ENTITY

        dtd::entity_decl ent;

        // Check for parameter entity
        if (current_.type == token_type::percent) {
          advance();
          ent.is_parameter = true;
        }

        ent.name = expect_name();

        // Value or external id
        if (current_.type == token_type::literal) {
          ent.value = expect_literal();

          // Register parameter entities for expansion
          if (ent.is_parameter) { param_entities_[ent.name] = ent.value; }
        } else if (current_.type == token_type::kw_system) {
          advance();
          ent.system_id = expect_literal();
        } else if (current_.type == token_type::kw_public) {
          advance();
          ent.public_id = expect_literal();
          ent.system_id = expect_literal();
        }

        // Skip optional NDATA for general entities
        if (current_.type == token_type::kw_ndata) {
          advance();
          expect_name(); // notation name
        }

        expect(token_type::close_angle);
        result_.entities.push_back(std::move(ent));
      }
    };

  } // namespace

  dtd::document
  dtd_parser::parse(const std::string& source) {
    parser p(source);
    return p.parse();
  }

} // namespace xb
