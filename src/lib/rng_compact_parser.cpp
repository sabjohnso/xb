#include <xb/rng_compact_parser.hpp>

#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace xb {

  namespace {

    // -----------------------------------------------------------------------
    // Token types
    // -----------------------------------------------------------------------

    enum class token_kind {
      eof,
      identifier, // NCName (not a keyword, or backslash-escaped)
      cname,      // prefix:localName
      ns_name,    // prefix:*
      literal,    // "..." or '...' (including triple-quoted)
      kw_attribute,
      kw_default,
      kw_datatypes,
      kw_div,
      kw_element,
      kw_empty,
      kw_external,
      kw_grammar,
      kw_include,
      kw_inherit,
      kw_list,
      kw_mixed,
      kw_namespace,
      kw_notAllowed,
      kw_parent,
      kw_start,
      kw_string,
      kw_token,
      kw_text,
      eq,       // =
      pipe_eq,  // |=
      amp_eq,   // &=
      lbrace,   // {
      rbrace,   // }
      lparen,   // (
      rparen,   // )
      comma,    // ,
      pipe,     // |
      amp,      // &
      star,     // *
      plus,     // +
      question, // ?
      minus,    // -
      tilde,    // ~
    };

    struct token {
      token_kind kind = token_kind::eof;
      std::string value;
    };

    // -----------------------------------------------------------------------
    // Keyword table
    // -----------------------------------------------------------------------

    const std::unordered_map<std::string, token_kind> keywords = {
        {"attribute", token_kind::kw_attribute},
        {"default", token_kind::kw_default},
        {"datatypes", token_kind::kw_datatypes},
        {"div", token_kind::kw_div},
        {"element", token_kind::kw_element},
        {"empty", token_kind::kw_empty},
        {"external", token_kind::kw_external},
        {"grammar", token_kind::kw_grammar},
        {"include", token_kind::kw_include},
        {"inherit", token_kind::kw_inherit},
        {"list", token_kind::kw_list},
        {"mixed", token_kind::kw_mixed},
        {"namespace", token_kind::kw_namespace},
        {"notAllowed", token_kind::kw_notAllowed},
        {"parent", token_kind::kw_parent},
        {"start", token_kind::kw_start},
        {"string", token_kind::kw_string},
        {"token", token_kind::kw_token},
        {"text", token_kind::kw_text},
    };

    // -----------------------------------------------------------------------
    // Lexer
    // -----------------------------------------------------------------------

    bool
    is_ncname_start(char c) {
      return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool
    is_ncname_char(char c) {
      return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
             c == '-' || c == '.';
    }

    class lexer {
    public:
      explicit lexer(const std::string& source) : src_(source), pos_(0) {}

      std::size_t
      position() const {
        return pos_;
      }

      int
      line_number() const {
        int line = 1;
        for (std::size_t i = 0; i < pos_ && i < src_.size(); ++i)
          if (src_[i] == '\n') ++line;
        return line;
      }

      token
      next() {
        skip_whitespace_and_comments();
        if (pos_ >= src_.size()) return {token_kind::eof, ""};

        char c = src_[pos_];

        // Backslash-escaped identifier (keyword escaping)
        if (c == '\\') {
          ++pos_;
          return read_ncname_as_identifier();
        }

        // String literals
        if (c == '"' || c == '\'') return read_literal();

        // NCName / keyword / CName / nsName
        if (is_ncname_start(c)) return read_name_or_keyword();

        // Operators
        switch (c) {
          case '{':
            ++pos_;
            return {token_kind::lbrace, "{"};
          case '}':
            ++pos_;
            return {token_kind::rbrace, "}"};
          case '(':
            ++pos_;
            return {token_kind::lparen, "("};
          case ')':
            ++pos_;
            return {token_kind::rparen, ")"};
          case ',':
            ++pos_;
            return {token_kind::comma, ","};
          case '+':
            ++pos_;
            return {token_kind::plus, "+"};
          case '?':
            ++pos_;
            return {token_kind::question, "?"};
          case '-':
            ++pos_;
            return {token_kind::minus, "-"};
          case '~':
            ++pos_;
            return {token_kind::tilde, "~"};
          case '*':
            ++pos_;
            return {token_kind::star, "*"};
          case '|':
            ++pos_;
            if (pos_ < src_.size() && src_[pos_] == '=') {
              ++pos_;
              return {token_kind::pipe_eq, "|="};
            }
            return {token_kind::pipe, "|"};
          case '&':
            ++pos_;
            if (pos_ < src_.size() && src_[pos_] == '=') {
              ++pos_;
              return {token_kind::amp_eq, "&="};
            }
            return {token_kind::amp, "&"};
          case '=':
            ++pos_;
            return {token_kind::eq, "="};
          default:
            throw std::runtime_error(std::string("unexpected character: '") +
                                     c + "'");
        }
      }

      token
      peek() {
        auto saved = pos_;
        auto tok = next();
        pos_ = saved;
        return tok;
      }

    private:
      const std::string& src_;
      std::size_t pos_;

      void
      skip_whitespace_and_comments() {
        while (pos_ < src_.size()) {
          char c = src_[pos_];
          if (std::isspace(static_cast<unsigned char>(c))) {
            ++pos_;
            continue;
          }
          if (c == '#') {
            // Skip to end of line
            while (pos_ < src_.size() && src_[pos_] != '\n')
              ++pos_;
            continue;
          }
          if (c == '[') {
            // Standalone annotation block [ ... ]. These annotate
            // patterns and definitions with metadata (e.g. Schematron
            // rules). Skip the balanced bracket content.
            skip_annotation_block();
            continue;
          }
          break;
        }
      }

    public:
      // Skip a balanced [ ... ] annotation block at the current position.
      // Returns true if an annotation was found and consumed.
      bool
      skip_annotation_if_present() {
        auto saved = pos_;
        skip_whitespace_and_comments();
        if (pos_ < src_.size() && src_[pos_] == '[') {
          skip_annotation_block();
          return true;
        }
        pos_ = saved;
        return false;
      }

    private:
      void
      skip_annotation_block() {
        ++pos_; // consume opening '['
        int depth = 1;
        while (pos_ < src_.size() && depth > 0) {
          char c = src_[pos_++];
          if (c == '#') {
            // Skip comment to end of line — quotes inside comments
            // are not string delimiters (e.g. "wouldn't").
            while (pos_ < src_.size() && src_[pos_] != '\n')
              ++pos_;
          } else if (c == '[')
            ++depth;
          else if (c == ']')
            --depth;
          else if (c == '"' || c == '\'')
            skip_quoted_in_annotation(c);
        }
      }

      void
      skip_quoted_in_annotation(char quote) {
        while (pos_ < src_.size() && src_[pos_] != quote)
          ++pos_;
        if (pos_ < src_.size()) ++pos_; // consume closing quote
      }

      token
      read_ncname_as_identifier() {
        std::string name;
        while (pos_ < src_.size() && is_ncname_char(src_[pos_])) {
          name += src_[pos_++];
        }
        if (name.empty())
          throw std::runtime_error("expected identifier after '\\'");
        return {token_kind::identifier, name};
      }

      token
      read_name_or_keyword() {
        std::string name;
        while (pos_ < src_.size() && is_ncname_char(src_[pos_])) {
          name += src_[pos_++];
        }

        // Check for colon → CName or nsName
        if (pos_ < src_.size() && src_[pos_] == ':') {
          std::size_t colon_pos = pos_;
          ++pos_;
          if (pos_ < src_.size() && src_[pos_] == '*') {
            // nsName: prefix:*
            ++pos_;
            return {token_kind::ns_name, name};
          }
          if (pos_ < src_.size() && is_ncname_start(src_[pos_])) {
            // CName: prefix:localName
            std::string local;
            while (pos_ < src_.size() && is_ncname_char(src_[pos_])) {
              local += src_[pos_++];
            }
            return {token_kind::cname, name + ":" + local};
          }
          // Not a CName or nsName, backtrack
          pos_ = colon_pos;
        }

        // Check keywords
        auto it = keywords.find(name);
        if (it != keywords.end()) { return {it->second, name}; }

        return {token_kind::identifier, name};
      }

      token
      read_literal() {
        char quote = src_[pos_];
        ++pos_;

        // Check for triple-quoted string
        if (pos_ + 1 < src_.size() && src_[pos_] == quote &&
            src_[pos_ + 1] == quote) {
          pos_ += 2;
          return read_triple_quoted(quote);
        }

        std::string value;
        while (pos_ < src_.size() && src_[pos_] != quote) {
          value += src_[pos_++];
        }
        if (pos_ >= src_.size())
          throw std::runtime_error("unterminated string literal");
        ++pos_; // skip closing quote
        return {token_kind::literal, value};
      }

      token
      read_triple_quoted(char quote) {
        std::string value;
        while (pos_ + 2 < src_.size()) {
          if (src_[pos_] == quote && src_[pos_ + 1] == quote &&
              src_[pos_ + 2] == quote) {
            pos_ += 3;
            return {token_kind::literal, value};
          }
          value += src_[pos_++];
        }
        throw std::runtime_error("unterminated triple-quoted string");
      }
    };

    // -----------------------------------------------------------------------
    // Parser
    // -----------------------------------------------------------------------

    class parser {
    public:
      explicit parser(const std::string& source) : lex_(source) { advance(); }

      rng::pattern
      parse_top_level() {
        parse_preamble();

        // Check if this is an implicit grammar (definitions at top level)
        // or an explicit pattern
        if (is_grammar_content_start()) { return parse_implicit_grammar(); }

        // Bare pattern at top level → wrap in grammar
        auto p = parse_pattern();
        rng::grammar_pattern gp;
        gp.start = std::make_unique<rng::pattern>(std::move(p));
        return rng::pattern(std::move(gp));
      }

    private:
      lexer lex_;
      token current_;
      std::string default_ns_;
      std::unordered_map<std::string, std::string> ns_map_;
      std::unordered_map<std::string, std::string> dt_map_;

      void
      advance() {
        current_ = lex_.next();
      }

      [[noreturn]] void
      error(const std::string& msg) {
        throw std::runtime_error("rnc parse error (line " +
                                 std::to_string(lex_.line_number()) +
                                 "): " + msg);
      }

      void
      expect(token_kind k, const std::string& what) {
        if (current_.kind != k) {
          auto got = current_.kind == token_kind::eof
                         ? "end of input"
                         : "'" + current_.value + "'";
          error("expected " + what + ", got " + got);
        }
        advance();
      }

      bool
      match(token_kind k) {
        if (current_.kind == k) {
          advance();
          return true;
        }
        return false;
      }

      std::string
      expect_literal() {
        std::string result;
        if (current_.kind != token_kind::literal)
          error("expected string literal");
        result = current_.value;
        advance();
        // Handle tilde concatenation
        while (current_.kind == token_kind::tilde) {
          advance();
          if (current_.kind != token_kind::literal)
            error("expected string literal after '~'");
          result += current_.value;
          advance();
        }
        return result;
      }

      std::string
      expect_identifier() {
        if (current_.kind != token_kind::identifier)
          error("expected identifier, got '" + current_.value + "'");
        auto val = current_.value;
        advance();
        return val;
      }

      // -------------------------------------------------------------------
      // Preamble: namespace/datatypes declarations
      // -------------------------------------------------------------------

      void
      parse_preamble() {
        while (true) {
          if (current_.kind == token_kind::kw_namespace) {
            parse_namespace_decl();
          } else if (current_.kind == token_kind::kw_default) {
            parse_default_decl();
          } else if (current_.kind == token_kind::kw_datatypes) {
            parse_datatypes_decl();
          } else if (current_.kind == token_kind::cname) {
            // Annotation element (CName [ ... ]). The bracket content
            // was already consumed by the lexer's whitespace skipper.
            // Discard the orphaned CName prefix.
            advance();
          } else {
            break;
          }
        }
      }

      void
      parse_namespace_decl() {
        advance(); // consume 'namespace'
        auto prefix = expect_identifier();
        expect(token_kind::eq, "'='");
        auto uri = expect_literal();
        ns_map_[prefix] = uri;
      }

      void
      parse_default_decl() {
        advance(); // consume 'default'
        expect(token_kind::kw_namespace, "'namespace'");
        // optional identifier for the prefix
        if (current_.kind == token_kind::identifier) {
          auto prefix = current_.value;
          advance();
          expect(token_kind::eq, "'='");
          auto uri = expect_literal();
          default_ns_ = uri;
          ns_map_[prefix] = uri;
        } else {
          expect(token_kind::eq, "'='");
          default_ns_ = expect_literal();
        }
      }

      void
      parse_datatypes_decl() {
        advance(); // consume 'datatypes'
        auto prefix = expect_identifier();
        expect(token_kind::eq, "'='");
        auto uri = expect_literal();
        dt_map_[prefix] = uri;
      }

      // -------------------------------------------------------------------
      // Grammar detection and parsing
      // -------------------------------------------------------------------

      bool
      is_grammar_content_start() {
        return current_.kind == token_kind::kw_start ||
               current_.kind == token_kind::kw_include ||
               current_.kind == token_kind::kw_div || is_define_start();
      }

      bool
      is_define_start() {
        if (current_.kind != token_kind::identifier) return false;
        // Peek to see if next token is an assign op
        auto peek = lex_.peek();
        return peek.kind == token_kind::eq ||
               peek.kind == token_kind::pipe_eq ||
               peek.kind == token_kind::amp_eq;
      }

      rng::pattern
      parse_implicit_grammar() {
        rng::grammar_pattern gp;
        parse_grammar_content(gp);
        return rng::pattern(std::move(gp));
      }

      void
      parse_grammar_content(rng::grammar_pattern& gp) {
        while (current_.kind != token_kind::eof &&
               current_.kind != token_kind::rbrace) {
          if (current_.kind == token_kind::kw_start) {
            parse_start_def(gp);
          } else if (current_.kind == token_kind::kw_include) {
            parse_include(gp);
          } else if (current_.kind == token_kind::kw_div) {
            parse_div(gp);
          } else if (current_.kind == token_kind::identifier) {
            parse_define(gp);
          } else if (current_.kind == token_kind::cname) {
            // Annotation element (CName [ ... ]). The bracket content
            // was already consumed by the lexer. Discard the prefix.
            advance();
          } else {
            break;
          }
        }
      }

      void
      parse_start_def(rng::grammar_pattern& gp) {
        advance(); // consume 'start'
        auto assign_kind = parse_assign_op();
        auto body = parse_pattern();

        if (assign_kind == token_kind::eq) {
          // If start pattern is an element, create a synthetic define
          // and set start as a ref to it
          if (body.holds<rng::element_pattern>()) {
            auto& elem = body.get<rng::element_pattern>();
            std::string def_name;
            if (elem.name.holds<rng::specific_name>()) {
              def_name = elem.name.get<rng::specific_name>().local_name;
            } else {
              def_name = "__start__";
            }
            gp.start = rng::make_pattern(rng::ref_pattern{def_name});
            gp.defines.push_back(
                rng::define{def_name, rng::combine_method::none,
                            std::make_unique<rng::pattern>(std::move(body))});
          } else {
            // For non-element patterns, wrap in a synthetic define
            std::string def_name = "__start__";
            gp.start = rng::make_pattern(rng::ref_pattern{def_name});
            gp.defines.push_back(
                rng::define{def_name, rng::combine_method::none,
                            std::make_unique<rng::pattern>(std::move(body))});
          }
        } else {
          // Combine start: |= or &=
          auto cm = (assign_kind == token_kind::pipe_eq)
                        ? rng::combine_method::choice
                        : rng::combine_method::interleave;
          gp.defines.push_back(
              rng::define{"__start__", cm,
                          std::make_unique<rng::pattern>(std::move(body))});
          if (!gp.start)
            gp.start = rng::make_pattern(rng::ref_pattern{"__start__"});
        }
      }

      token_kind
      parse_assign_op() {
        if (current_.kind == token_kind::eq ||
            current_.kind == token_kind::pipe_eq ||
            current_.kind == token_kind::amp_eq) {
          auto k = current_.kind;
          advance();
          return k;
        }
        error("expected '=', '|=', or '&='");
      }

      void
      parse_define(rng::grammar_pattern& gp) {
        auto name = current_.value;
        advance(); // consume identifier

        auto assign_kind = parse_assign_op();
        auto body = parse_pattern();

        rng::combine_method cm = rng::combine_method::none;
        if (assign_kind == token_kind::pipe_eq)
          cm = rng::combine_method::choice;
        else if (assign_kind == token_kind::amp_eq)
          cm = rng::combine_method::interleave;

        gp.defines.push_back(rng::define{
            name, cm, std::make_unique<rng::pattern>(std::move(body))});
      }

      void
      parse_include(rng::grammar_pattern& gp) {
        advance(); // consume 'include'
        auto href = expect_literal();

        rng::include_directive inc;
        inc.href = href;

        // Optional inherit
        if (current_.kind == token_kind::kw_inherit) {
          advance();
          expect(token_kind::eq, "'='");
          // inherit = prefix → resolve ns
          if (current_.kind == token_kind::identifier) {
            auto prefix = current_.value;
            advance();
            auto it = ns_map_.find(prefix);
            if (it != ns_map_.end()) inc.ns = it->second;
          }
        }

        // Optional override body
        if (current_.kind == token_kind::lbrace) {
          advance(); // consume '{'
          // Parse overrides (start and define statements)
          while (current_.kind != token_kind::rbrace &&
                 current_.kind != token_kind::eof) {
            if (current_.kind == token_kind::kw_start) {
              advance();
              parse_assign_op(); // consume =
              auto body = parse_pattern();
              inc.start_override =
                  std::make_unique<rng::pattern>(std::move(body));
            } else if (current_.kind == token_kind::identifier) {
              auto name = current_.value;
              advance();
              auto ak = parse_assign_op();
              auto body = parse_pattern();
              rng::combine_method cm = rng::combine_method::none;
              if (ak == token_kind::pipe_eq)
                cm = rng::combine_method::choice;
              else if (ak == token_kind::amp_eq)
                cm = rng::combine_method::interleave;
              inc.overrides.push_back(rng::define{
                  name, cm, std::make_unique<rng::pattern>(std::move(body))});
            } else {
              break;
            }
          }
          expect(token_kind::rbrace, "'}'");
        }

        gp.includes.push_back(std::move(inc));
      }

      void
      parse_div(rng::grammar_pattern& gp) {
        advance(); // consume 'div'
        expect(token_kind::lbrace, "'{'");
        parse_grammar_content(gp);
        expect(token_kind::rbrace, "'}'");
      }

      // -------------------------------------------------------------------
      // Pattern parsing (with operator precedence)
      // -------------------------------------------------------------------

      rng::pattern
      parse_pattern() {
        auto left = parse_particle();

        // Check for binary operator (,  |  &)
        // All operators at this level must be the same (no mixing)
        if (current_.kind == token_kind::comma) {
          return parse_binary_chain(std::move(left), token_kind::comma);
        }
        if (current_.kind == token_kind::pipe) {
          return parse_binary_chain(std::move(left), token_kind::pipe);
        }
        if (current_.kind == token_kind::amp) {
          return parse_binary_chain(std::move(left), token_kind::amp);
        }

        return left;
      }

      rng::pattern
      parse_binary_chain(rng::pattern left, token_kind op) {
        while (current_.kind == op) {
          // Check for operator mixing
          if (current_.kind != op && (current_.kind == token_kind::comma ||
                                      current_.kind == token_kind::pipe ||
                                      current_.kind == token_kind::amp)) {
            error("cannot mix ',', '|', and '&' operators without "
                  "parentheses");
          }
          advance(); // consume operator
          auto right = parse_particle();

          switch (op) {
            case token_kind::comma:
              left = rng::pattern(
                  rng::group_pattern{rng::make_pattern(std::move(left)),
                                     rng::make_pattern(std::move(right))});
              break;
            case token_kind::pipe:
              left = rng::pattern(
                  rng::choice_pattern{rng::make_pattern(std::move(left)),
                                      rng::make_pattern(std::move(right))});
              break;
            case token_kind::amp:
              left = rng::pattern(
                  rng::interleave_pattern{rng::make_pattern(std::move(left)),
                                          rng::make_pattern(std::move(right))});
              break;
            default:
              break;
          }
        }
        return left;
      }

      rng::pattern
      parse_particle() {
        auto p = parse_primary();

        // Postfix repetition operators
        if (current_.kind == token_kind::star) {
          advance();
          return rng::pattern(
              rng::zero_or_more_pattern{rng::make_pattern(std::move(p))});
        }
        if (current_.kind == token_kind::plus) {
          advance();
          return rng::pattern(
              rng::one_or_more_pattern{rng::make_pattern(std::move(p))});
        }
        if (current_.kind == token_kind::question) {
          advance();
          return rng::pattern(
              rng::optional_pattern{rng::make_pattern(std::move(p))});
        }
        return p;
      }

      rng::pattern
      parse_primary() {
        switch (current_.kind) {
          case token_kind::kw_element:
            return parse_element();
          case token_kind::kw_attribute:
            return parse_attribute();
          case token_kind::kw_mixed:
            return parse_mixed();
          case token_kind::kw_list:
            return parse_list();
          case token_kind::kw_grammar:
            return parse_grammar_block();
          case token_kind::kw_external:
            return parse_external();
          case token_kind::kw_parent:
            return parse_parent_ref();
          case token_kind::kw_empty:
            advance();
            return rng::pattern(rng::empty_pattern{});
          case token_kind::kw_notAllowed:
            advance();
            return rng::pattern(rng::not_allowed_pattern{});
          case token_kind::kw_text:
            advance();
            return rng::pattern(rng::text_pattern{});
          case token_kind::kw_string:
            return parse_builtin_datatype("string", "");
          case token_kind::kw_token:
            return parse_builtin_datatype("token", "");
          case token_kind::identifier:
            return parse_ref();
          case token_kind::cname:
            return parse_cname_datatype();
          case token_kind::literal:
            return parse_value();
          case token_kind::lparen:
            return parse_paren();
          case token_kind::eof:
            error("unexpected end of input");
          case token_kind::star:
            // This is * in name class context but shouldn't appear as
            // primary pattern. Fall through to error.
          default:
            error("unexpected token: '" + current_.value + "'");
        }
      }

      // -------------------------------------------------------------------
      // Primary pattern productions
      // -------------------------------------------------------------------

      rng::pattern
      parse_element() {
        advance(); // consume 'element'
        auto nc = parse_name_class();
        expect(token_kind::lbrace, "'{'");
        auto content = parse_pattern();
        expect(token_kind::rbrace, "'}'");
        return rng::pattern(rng::element_pattern{
            std::move(nc), rng::make_pattern(std::move(content))});
      }

      rng::pattern
      parse_attribute() {
        advance(); // consume 'attribute'
        auto nc = parse_name_class_for_attr();
        expect(token_kind::lbrace, "'{'");
        auto content = parse_pattern();
        expect(token_kind::rbrace, "'}'");
        return rng::pattern(rng::attribute_pattern{
            std::move(nc), rng::make_pattern(std::move(content))});
      }

      rng::pattern
      parse_mixed() {
        advance(); // consume 'mixed'
        expect(token_kind::lbrace, "'{'");
        auto content = parse_pattern();
        expect(token_kind::rbrace, "'}'");
        return rng::pattern(
            rng::mixed_pattern{rng::make_pattern(std::move(content))});
      }

      rng::pattern
      parse_list() {
        advance(); // consume 'list'
        expect(token_kind::lbrace, "'{'");
        auto content = parse_pattern();
        expect(token_kind::rbrace, "'}'");
        return rng::pattern(
            rng::list_pattern{rng::make_pattern(std::move(content))});
      }

      rng::pattern
      parse_grammar_block() {
        advance(); // consume 'grammar'
        expect(token_kind::lbrace, "'{'");
        rng::grammar_pattern gp;
        parse_grammar_content(gp);
        expect(token_kind::rbrace, "'}'");
        return rng::pattern(std::move(gp));
      }

      rng::pattern
      parse_external() {
        advance(); // consume 'external'
        auto href = expect_literal();
        std::string ns;
        if (current_.kind == token_kind::kw_inherit) {
          advance();
          expect(token_kind::eq, "'='");
          if (current_.kind == token_kind::identifier) {
            auto prefix = current_.value;
            advance();
            auto it = ns_map_.find(prefix);
            if (it != ns_map_.end()) ns = it->second;
          }
        }
        return rng::pattern(rng::external_ref_pattern{href, ns});
      }

      rng::pattern
      parse_parent_ref() {
        advance(); // consume 'parent'
        auto name = expect_identifier();
        return rng::pattern(rng::parent_ref_pattern{name});
      }

      rng::pattern
      parse_ref() {
        auto name = current_.value;
        advance();
        return rng::pattern(rng::ref_pattern{name});
      }

      rng::pattern
      parse_builtin_datatype(const std::string& type,
                             const std::string& library) {
        advance(); // consume 'string' or 'token'

        // Check for params
        if (current_.kind == token_kind::lbrace) {
          auto params = parse_params();
          return rng::pattern(
              rng::data_pattern{library, type, std::move(params), nullptr});
        }

        // Check for value (literal following builtin type)
        if (current_.kind == token_kind::literal) {
          auto val = expect_literal();
          return rng::pattern(
              rng::value_pattern{library, type, val, default_ns_});
        }

        return rng::pattern(rng::data_pattern{library, type, {}, nullptr});
      }

      rng::pattern
      parse_cname_datatype() {
        auto cname = current_.value;
        advance();

        // Split CName into prefix:local
        auto colon = cname.find(':');
        auto prefix = cname.substr(0, colon);
        auto local = cname.substr(colon + 1);

        // Resolve datatype library
        std::string dt_lib;
        auto it = dt_map_.find(prefix);
        if (it != dt_map_.end()) { dt_lib = it->second; }

        // Check for params
        if (current_.kind == token_kind::lbrace) {
          auto params = parse_params();
          // Check for except
          std::unique_ptr<rng::pattern> except;
          if (current_.kind == token_kind::minus) {
            advance();
            auto ep = parse_particle();
            except = rng::make_pattern(std::move(ep));
          }
          return rng::pattern(rng::data_pattern{
              dt_lib, local, std::move(params), std::move(except)});
        }

        // Check for value (literal following datatype name)
        if (current_.kind == token_kind::literal) {
          auto val = expect_literal();
          return rng::pattern(
              rng::value_pattern{dt_lib, local, val, default_ns_});
        }

        // Check for except
        std::unique_ptr<rng::pattern> except;
        if (current_.kind == token_kind::minus) {
          advance();
          auto ep = parse_particle();
          except = rng::make_pattern(std::move(ep));
        }

        return rng::pattern(
            rng::data_pattern{dt_lib, local, {}, std::move(except)});
      }

      rng::pattern
      parse_value() {
        auto val = expect_literal();
        return rng::pattern(rng::value_pattern{"", "token", val, default_ns_});
      }

      rng::pattern
      parse_paren() {
        advance(); // consume '('
        auto p = parse_pattern();
        expect(token_kind::rparen, "')'");
        return p;
      }

      std::vector<rng::data_param>
      parse_params() {
        std::vector<rng::data_param> params;
        advance(); // consume '{'
        while (current_.kind != token_kind::rbrace &&
               current_.kind != token_kind::eof) {
          auto name = expect_identifier();
          expect(token_kind::eq, "'='");
          auto val = expect_literal();
          params.push_back(rng::data_param{name, val});
        }
        expect(token_kind::rbrace, "'}'");
        return params;
      }

      // -------------------------------------------------------------------
      // Name class parsing
      // -------------------------------------------------------------------

      rng::name_class
      parse_name_class() {
        auto nc = parse_simple_name_class(true);

        // Check for name class choice (|)
        if (current_.kind == token_kind::pipe) {
          return parse_name_class_choice(std::move(nc));
        }

        // Check for except (-)
        if (current_.kind == token_kind::minus) {
          return parse_name_class_except(std::move(nc));
        }

        return nc;
      }

      // In RNC, keywords (namespace, element, attribute, etc.) can be
      // used as unquoted element/attribute names after the keyword that
      // introduces the construct.
      bool
      is_keyword_as_name() const {
        switch (current_.kind) {
          case token_kind::kw_attribute:
          case token_kind::kw_default:
          case token_kind::kw_datatypes:
          case token_kind::kw_div:
          case token_kind::kw_element:
          case token_kind::kw_empty:
          case token_kind::kw_external:
          case token_kind::kw_grammar:
          case token_kind::kw_include:
          case token_kind::kw_inherit:
          case token_kind::kw_list:
          case token_kind::kw_mixed:
          case token_kind::kw_namespace:
          case token_kind::kw_notAllowed:
          case token_kind::kw_parent:
          case token_kind::kw_start:
          case token_kind::kw_string:
          case token_kind::kw_token:
          case token_kind::kw_text:
            return true;
          default:
            return false;
        }
      }

      // Attributes: unqualified names have empty namespace (not default)
      rng::name_class
      parse_name_class_for_attr() {
        if (current_.kind == token_kind::star) {
          advance();
          if (current_.kind == token_kind::minus) {
            auto nc = rng::name_class(rng::any_name_nc{nullptr});
            return parse_name_class_except(std::move(nc));
          }
          return rng::name_class(rng::any_name_nc{nullptr});
        }
        if (current_.kind == token_kind::ns_name) {
          auto prefix = current_.value;
          advance();
          std::string ns;
          auto it = ns_map_.find(prefix);
          if (it != ns_map_.end()) ns = it->second;
          return rng::name_class(rng::ns_name_nc{ns, nullptr});
        }
        if (current_.kind == token_kind::cname) {
          return parse_cname_as_name_class();
        }
        if (current_.kind == token_kind::identifier || is_keyword_as_name()) {
          auto local = current_.value;
          advance();
          // Attribute names: empty namespace (not default)
          return rng::name_class(rng::specific_name{"", local});
        }
        error("expected name class for attribute");
      }

      rng::name_class
      parse_simple_name_class(bool use_default_ns) {
        if (current_.kind == token_kind::star) {
          advance();
          return rng::name_class(rng::any_name_nc{nullptr});
        }
        if (current_.kind == token_kind::ns_name) {
          auto prefix = current_.value;
          advance();
          std::string ns;
          auto it = ns_map_.find(prefix);
          if (it != ns_map_.end()) ns = it->second;
          return rng::name_class(rng::ns_name_nc{ns, nullptr});
        }
        if (current_.kind == token_kind::cname) {
          return parse_cname_as_name_class();
        }
        if (current_.kind == token_kind::identifier || is_keyword_as_name()) {
          auto local = current_.value;
          advance();
          std::string ns = use_default_ns ? default_ns_ : "";
          return rng::name_class(rng::specific_name{ns, local});
        }
        error("expected name class");
      }

      rng::name_class
      parse_cname_as_name_class() {
        auto cname = current_.value;
        advance();
        auto colon = cname.find(':');
        auto prefix = cname.substr(0, colon);
        auto local = cname.substr(colon + 1);
        std::string ns;
        auto it = ns_map_.find(prefix);
        if (it != ns_map_.end()) ns = it->second;
        return rng::name_class(rng::specific_name{ns, local});
      }

      rng::name_class
      parse_name_class_choice(rng::name_class left) {
        while (current_.kind == token_kind::pipe) {
          advance();
          auto right = parse_simple_name_class(true);
          left = rng::name_class(
              rng::choice_name_class{rng::make_name_class(std::move(left)),
                                     rng::make_name_class(std::move(right))});
        }
        return left;
      }

      rng::name_class
      parse_name_class_except(rng::name_class base) {
        advance(); // consume '-'
        auto exc = parse_name_class_primary(true);
        if (base.holds<rng::any_name_nc>()) {
          return rng::name_class(
              rng::any_name_nc{rng::make_name_class(std::move(exc))});
        }
        if (base.holds<rng::ns_name_nc>()) {
          auto& nsn = base.get<rng::ns_name_nc>();
          return rng::name_class(
              rng::ns_name_nc{nsn.ns, rng::make_name_class(std::move(exc))});
        }
        error("except only valid after * or nsName");
      }

      // Parse a primary name class: either a parenthesized group or a
      // simple name. Parenthesized groups may contain choice (|).
      rng::name_class
      parse_name_class_primary(bool use_default_ns) {
        if (current_.kind == token_kind::lparen) {
          advance(); // consume '('
          auto nc = parse_simple_name_class(use_default_ns);
          if (current_.kind == token_kind::pipe)
            nc = parse_name_class_choice(std::move(nc));
          if (current_.kind == token_kind::minus)
            nc = parse_name_class_except(std::move(nc));
          expect(token_kind::rparen, "')'");
          return nc;
        }
        return parse_simple_name_class(use_default_ns);
      }
    };

  } // namespace

  // -----------------------------------------------------------------------
  // Public interface
  // -----------------------------------------------------------------------

  rng::pattern
  rng_compact_parser::parse(const std::string& source) {
    parser p(source);
    return p.parse_top_level();
  }

} // namespace xb
