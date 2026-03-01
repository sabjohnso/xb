#include <xb/xpath_expr.hpp>

#include <cctype>

namespace xb {

  namespace {

    // Recursive descent parser for an XPath assertion subset.
    // Grammar:
    //   expr        := or_expr
    //   or_expr     := and_expr ('or' and_expr)*
    //   and_expr    := not_expr ('and' not_expr)*
    //   not_expr    := 'not' '(' expr ')' | comparison
    //   comparison  := primary (comp_op primary)?
    //   comp_op     := '>=' | '<=' | '!=' | '>' | '<' | '='
    //   primary     := '$value' | '@' IDENT | NUMBER | STRING
    //                  | function_call | path_expr | '(' expr ')'
    //   function_call := IDENT '(' (expr (',' expr)*)? ')'
    //   path_expr   := IDENT ('/' IDENT)*

    class xpath_parser {
      std::string_view input_;
      std::size_t pos_ = 0;
      const xpath_context& ctx_;
      bool failed_ = false;

    public:
      xpath_parser(std::string_view input, const xpath_context& ctx)
          : input_(input), ctx_(ctx) {}

      std::optional<std::string>
      parse() {
        skip_ws();
        if (pos_ >= input_.size()) return std::nullopt;

        auto result = parse_expr();
        if (failed_) return std::nullopt;

        skip_ws();
        if (pos_ != input_.size()) return std::nullopt; // trailing junk

        return result;
      }

    private:
      char
      peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
      }

      char
      advance() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
      }

      void
      skip_ws() {
        while (pos_ < input_.size() && std::isspace(input_[pos_]))
          ++pos_;
      }

      bool
      match(char c) {
        skip_ws();
        if (peek() == c) {
          ++pos_;
          return true;
        }
        return false;
      }

      bool
      match_keyword(std::string_view kw) {
        skip_ws();
        if (input_.substr(pos_).starts_with(kw)) {
          // Keyword must not be followed by an identifier char
          auto after = pos_ + kw.size();
          if (after >= input_.size() || !is_ident_char(input_[after])) {
            pos_ = after;
            return true;
          }
        }
        return false;
      }

      static bool
      is_ident_start(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
      }

      static bool
      is_ident_char(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
               c == '-';
      }

      // expr := or_expr
      std::string
      parse_expr() {
        return parse_or_expr();
      }

      // or_expr := and_expr ('or' and_expr)*
      std::string
      parse_or_expr() {
        auto left = parse_and_expr();
        if (failed_) return {};

        while (match_keyword("or")) {
          auto right = parse_and_expr();
          if (failed_) return {};
          left = "(" + left + " || " + right + ")";
        }
        return left;
      }

      // and_expr := not_expr ('and' not_expr)*
      std::string
      parse_and_expr() {
        auto left = parse_not_expr();
        if (failed_) return {};

        while (match_keyword("and")) {
          auto right = parse_not_expr();
          if (failed_) return {};
          left = "(" + left + " && " + right + ")";
        }
        return left;
      }

      // not_expr := 'not' '(' expr ')' | comparison
      std::string
      parse_not_expr() {
        if (match_keyword("not")) {
          if (!match('(')) {
            failed_ = true;
            return {};
          }
          auto inner = parse_expr();
          if (failed_) return {};
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return "(!" + inner + ")";
        }
        return parse_comparison();
      }

      // comparison := primary (comp_op primary)?
      std::string
      parse_comparison() {
        auto left = parse_primary();
        if (failed_) return {};

        skip_ws();
        auto op = try_comp_op();
        if (op.empty()) return left;

        auto right = parse_primary();
        if (failed_) return {};

        return "(" + left + " " + op + " " + right + ")";
      }

      // Try to match a comparison operator. Returns "" if none.
      std::string
      try_comp_op() {
        if (pos_ >= input_.size()) return "";

        // Two-char operators first
        if (pos_ + 1 < input_.size()) {
          auto two = input_.substr(pos_, 2);
          if (two == ">=" || two == "<=" || two == "!=") {
            pos_ += 2;
            return std::string(two);
          }
        }

        char c = input_[pos_];
        if (c == '>') {
          ++pos_;
          return ">";
        }
        if (c == '<') {
          ++pos_;
          return "<";
        }
        if (c == '=') {
          ++pos_;
          return "=="; // XPath = maps to C++ ==
        }

        return "";
      }

      // primary := '$value' | '@' IDENT | NUMBER | STRING | IDENT
      //            | '(' expr ')'
      std::string
      parse_primary() {
        skip_ws();
        if (pos_ >= input_.size()) {
          failed_ = true;
          return {};
        }

        char c = peek();

        // Parenthesized expression
        if (c == '(') {
          ++pos_;
          auto inner = parse_expr();
          if (failed_) return {};
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return inner;
        }

        // $value
        if (c == '$') {
          ++pos_;
          auto ident = read_ident();
          if (ident.empty()) {
            failed_ = true;
            return {};
          }
          if (ident != "value") {
            failed_ = true; // only $value supported
            return {};
          }
          return std::string(ctx_.value_prefix);
        }

        // @attr
        if (c == '@') {
          ++pos_;
          auto ident = read_ident();
          if (ident.empty()) {
            failed_ = true;
            return {};
          }
          return std::string(ctx_.value_prefix) + std::string(ident);
        }

        // String literal
        if (c == '\'' || c == '"') { return parse_string_literal(); }

        // Number (integer or decimal)
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
          return parse_number();
        }

        // Identifier: function call, path expression, or field reference
        if (is_ident_start(c)) {
          auto saved = pos_;
          auto ident = read_ident();
          if (ident == "and" || ident == "or" || ident == "not") {
            pos_ = saved;
            failed_ = true;
            return {};
          }
          // Namespace prefix (ident ':') — unsupported
          skip_ws();
          if (peek() == ':') {
            failed_ = true;
            return {};
          }
          // Function call (ident '(')
          if (peek() == '(') { return parse_function_call(std::string(ident)); }
          // Path expression (ident '/' ident ...)
          std::string path =
              std::string(ctx_.value_prefix) + std::string(ident);
          while (peek() == '/') {
            ++pos_; // consume '/'
            auto step = read_ident();
            if (step.empty()) {
              failed_ = true;
              return {};
            }
            path += "." + std::string(step);
          }
          return path;
        }

        // Unsupported character
        failed_ = true;
        return {};
      }

      // Parse a known function call. Returns the C++ translation.
      std::string
      parse_function_call(const std::string& name) {
        if (!match('(')) {
          failed_ = true;
          return {};
        }

        // Zero-arg functions
        if (name == "true") {
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return "true";
        }
        if (name == "false") {
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return "false";
        }

        // One-arg functions: count, string-length
        if (name == "count" || name == "string-length") {
          auto arg = parse_function_arg();
          if (failed_) return {};
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return arg + ".size()";
        }

        // Two-arg functions: contains, starts-with
        if (name == "contains") {
          auto arg1 = parse_function_arg();
          if (failed_) return {};
          if (!match(',')) {
            failed_ = true;
            return {};
          }
          auto arg2 = parse_primary();
          if (failed_) return {};
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return "(" + arg1 + ".find(" + arg2 + ") != std::string::npos)";
        }
        if (name == "starts-with") {
          auto arg1 = parse_function_arg();
          if (failed_) return {};
          if (!match(',')) {
            failed_ = true;
            return {};
          }
          auto arg2 = parse_primary();
          if (failed_) return {};
          if (!match(')')) {
            failed_ = true;
            return {};
          }
          return arg1 + ".starts_with(" + arg2 + ")";
        }

        // Unsupported function
        failed_ = true;
        return {};
      }

      // Parse a function argument (field path or expression)
      std::string
      parse_function_arg() {
        skip_ws();
        if (is_ident_start(peek())) {
          auto ident = read_ident();
          // Path expression inside function arg
          std::string path =
              std::string(ctx_.value_prefix) + std::string(ident);
          while (peek() == '/') {
            ++pos_;
            auto step = read_ident();
            if (step.empty()) {
              failed_ = true;
              return {};
            }
            path += "." + std::string(step);
          }
          return path;
        }
        // Could be $value or other primary
        return parse_primary();
      }

      std::string_view
      read_ident() {
        auto start = pos_;
        if (pos_ < input_.size() && is_ident_start(input_[pos_])) {
          ++pos_;
          while (pos_ < input_.size() && is_ident_char(input_[pos_]))
            ++pos_;
        }
        return input_.substr(start, pos_ - start);
      }

      std::string
      parse_string_literal() {
        char quote = advance();
        std::string result = "\"";
        while (pos_ < input_.size() && input_[pos_] != quote) {
          result += input_[pos_++];
        }
        if (pos_ >= input_.size()) {
          failed_ = true;
          return {};
        }
        ++pos_; // consume closing quote
        result += '"';
        return result;
      }

      std::string
      parse_number() {
        auto start = pos_;
        bool has_dot = false;
        while (pos_ < input_.size()) {
          if (input_[pos_] == '.') {
            if (has_dot) break;
            has_dot = true;
            ++pos_;
          } else if (std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
          } else {
            break;
          }
        }
        if (pos_ == start) {
          failed_ = true;
          return {};
        }
        return std::string(input_.substr(start, pos_ - start));
      }
    };

  } // namespace

  std::optional<std::string>
  translate_xpath_assertion(std::string_view xpath, const xpath_context& ctx) {
    xpath_parser parser(xpath, ctx);
    return parser.parse();
  }

} // namespace xb
