#pragma once

#include "shader-lang/token.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace astralix {
class Tokenizer {
public:
  explicit Tokenizer(std::string_view source, std::string_view filename = "");

  Token next();
  Token peek();

  const std::vector<std::string> &errors() const { return m_errors; }

private:
  Token scan_token();
  Token scan_decorator();
  Token scan_number();
  Token scan_string();
  Token scan_identifier_or_keyword();
  Token scan_operator();

  void skip_whitespace_and_comments();
  char current() const;
  char lookahead(int offset = 1) const;
  char advance();
  bool match(char expected);

  Token make_token(TokenKind kind, std::string lexeme = {}) const;
  Token error_token(std::string_view msg);

  std::string_view m_source;
  std::string_view m_filename;
  size_t m_pos = 0;
  uint32_t m_line = 1;
  uint32_t m_col = 1;
  uint32_t m_token_start_col = 1;
  std::optional<Token> m_peeked;
  std::vector<std::string> m_errors;

  static const std::unordered_map<std::string_view, TokenKind> s_keywords;
  static const std::unordered_map<std::string_view, TokenKind> s_decorators;
};

} // namespace astralix
