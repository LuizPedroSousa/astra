#include "shader-lang/tokenizer.hpp"
#include "shader-lang/token.hpp"
#include <string_view>
#include <unordered_map>

namespace astralix {
const std::unordered_map<std::string_view, TokenKind> Tokenizer::s_keywords = {
    {"in", TokenKind::KeywordIn},
    {"out", TokenKind::KeywordOut},
    {"inout", TokenKind::KeywordInout},
    {"uniform", TokenKind::KeywordUniform},
    {"buffer", TokenKind::KeywordBuffer},
    {"struct", TokenKind::KeywordStruct},
    {"const", TokenKind::KeywordConst},
    {"void", TokenKind::KeywordVoid},
    {"fn", TokenKind::KeywordFunction},
    {"if", TokenKind::KeywordIf},
    {"else", TokenKind::KeywordElse},
    {"for", TokenKind::KeywordFor},
    {"while", TokenKind::KeywordWhile},
    {"do", TokenKind::KeywordDo},
    {"return", TokenKind::KeywordReturn},
    {"break", TokenKind::KeywordBreak},
    {"continue", TokenKind::KeywordContinue},
    {"discard", TokenKind::KeywordDiscard},
    {"interface", TokenKind::KeywordInterface},
    {"true", TokenKind::Bool},
    {"false", TokenKind::Bool},
    {"bool", TokenKind::TypeBool},
    {"int", TokenKind::TypeInt},
    {"uint", TokenKind::TypeUint},
    {"float", TokenKind::TypeFloat},
    {"vec2", TokenKind::TypeVec2},
    {"vec3", TokenKind::TypeVec3},
    {"vec4", TokenKind::TypeVec4},
    {"ivec2", TokenKind::TypeIvec2},
    {"ivec3", TokenKind::TypeIvec3},
    {"ivec4", TokenKind::TypeIvec4},
    {"uvec2", TokenKind::TypeUvec2},
    {"uvec3", TokenKind::TypeUvec3},
    {"uvec4", TokenKind::TypeUvec4},
    {"mat2", TokenKind::TypeMat2},
    {"mat3", TokenKind::TypeMat3},
    {"mat4", TokenKind::TypeMat4},
    {"sampler2D", TokenKind::TypeSampler2D},
    {"samplerCube", TokenKind::TypeSamplerCube},
    {"sampler2DShadow", TokenKind::TypeSampler2DShadow},
    {"isampler2D", TokenKind::TypeIsampler2D},
    {"usampler2D", TokenKind::TypeUsampler2D},
};

const std::unordered_map<std::string_view, TokenKind> Tokenizer::s_decorators =
    {
        {"vertex", TokenKind::AtVertexStage},
        {"fragment", TokenKind::AtFragmentStage},
        {"compute", TokenKind::AtComputeStage},
        {"geometry", TokenKind::AtGeometryStage},
        {"version", TokenKind::AtVersion},
        {"include", TokenKind::AtInclude},
        {"define", TokenKind::AtDefine},
        {"location", TokenKind::AtLocation},
        {"binding", TokenKind::AtBinding},
        {"set", TokenKind::AtSet},
        {"std430", TokenKind::AtStd430},
        {"std140", TokenKind::AtStd140},
        {"push_constant", TokenKind::AtPushConstant},
        {"in", TokenKind::AtIn},
        {"uniform", TokenKind::AtUniform},
};

Token Tokenizer::next() {
  if (m_peeked) {
    Token token = *m_peeked;
    m_peeked.reset();
    return token;
  }

  return scan_token();
}

Token Tokenizer::peek() {
  if (!m_peeked) {
    m_peeked = scan_token();
  }

  return *m_peeked;
}

Token Tokenizer::scan_token() {
  skip_whitespace_and_comments();
  m_token_start_col = m_col;

  if (m_pos >= m_source.size()) {
    return make_token(TokenKind::EoF);
  }

  char character = current();

  if (character == '@') {
    return scan_decorator();
  }

  if (character == '"') {
    return scan_string();
  }

  if (std::isdigit(character)) {
    return scan_number();
  }

  if (std::isalpha(character) || character == '_') {
    return scan_identifier_or_keyword();
  }

  return scan_operator();
}

void Tokenizer::skip_whitespace_and_comments() {
  while (m_pos < m_source.size()) {
    char character = current();

    if (character == '\n') {
      ++m_line;
      m_col = 1;
      ++m_pos;
      continue;
    }

    if (std::isspace(character)) {
      advance();
      continue;
    }

    if (character == '/' && lookahead() == '/') {
      while (m_pos < m_source.size() && current() != '\n')
        ++m_pos;
      continue;
    }

    if (character == '/' && lookahead() == '*') {
      advance();
      advance();

      while (m_pos + 1 < m_source.size() &&
             !(current() == '*' && lookahead() == '/')) {
        if (current() == '\n') {
          ++m_line;
          m_col = 1;
          ++m_pos;
        } else {
          advance();
        }
      }

      advance();
      advance();
      continue;
    }
    break;
  }
}

Tokenizer::Tokenizer(std::string_view source, std::string_view filename)
    : m_source(source), m_filename(filename) {}

char Tokenizer::current() const {
  if (m_pos >= m_source.size()) {
    return '\0';
  }

  return m_source[m_pos];
}

inline char Tokenizer::lookahead(int offset) const {
  size_t index = m_pos + static_cast<size_t>(offset);

  if (index >= m_source.size()) {
    return '\0';
  }

  return m_source[index];
}

inline char Tokenizer::advance() {
  char c = m_source[m_pos++];
  ++m_col;
  return c;
}

inline bool Tokenizer::match(char expected) {
  if (current() != expected) {
    return false;
  }

  ++m_pos;
  return true;
}

inline Token Tokenizer::make_token(TokenKind kind, std::string lexeme) const {
  Token token;

  token.kind = kind;
  token.location = {m_line, m_token_start_col, m_filename};
  token.lexeme = std::move(lexeme);

  return token;
}

Token Tokenizer::error_token(std::string_view msg) {
  m_errors.emplace_back(msg);
  return make_token(TokenKind::Error, std::string(msg));
}

Token Tokenizer::scan_decorator() {
  advance();

  size_t start = m_pos;

  while (std::isalnum(current()) || current() == '_') {
    advance();
  }

  std::string_view name = m_source.substr(start, m_pos - start);

  auto it = s_decorators.find(name);

  if (it == s_decorators.end()) {
    return error_token("unknown decorator '@" + std::string(name) + "'");
  }

  return make_token(it->second, std::string(name));
}

Token Tokenizer::scan_number() {
  size_t start = m_pos;

  bool is_hex = (current() == '0' && lookahead() == 'x');

  if (is_hex) {
    advance();
    advance();
  }

  while (std::isxdigit(current())) {
    advance();
  }

  bool is_float = !is_hex && current() == '.' && !std::isalpha(lookahead()) &&
                  lookahead() != '_';

  if (is_float) {
    advance();
    while (std::isdigit(current()))
      advance();
  }

  bool has_float_suffix = !is_hex && current() == 'f';
  bool has_unasigned_suffix = !is_float && !is_hex && current() == 'u';

  if (has_float_suffix || has_unasigned_suffix) {
    advance();
  }

  std::string raw(m_source.substr(start, m_pos - start));

  Token token = make_token(
      is_float || has_float_suffix ? TokenKind::Float : TokenKind::Int, raw);

  if (is_float || has_float_suffix) {
    token.float_val = std::stod(raw);
  } else if (is_hex) {
    token.int_val = std::stoll(raw, nullptr, 0);
  } else {
    token.int_val = std::stoll(raw);
  }

  return token;
}

Token Tokenizer::scan_string() {
  advance();

  size_t start = m_pos;

  while (m_pos < m_source.size() && current() != '"' && current() != '\n') {
    advance();
  }

  if (current() != '"') {
    return error_token("unterminated string literal");
  }

  std::string value(m_source.substr(start, m_pos - start));

  advance();

  return make_token(TokenKind::String, std::move(value));
}

Token Tokenizer::scan_identifier_or_keyword() {
  size_t start = m_pos;

  while (std::isalnum(current()) || current() == '_') {
    advance();
  }

  std::string_view text = m_source.substr(start, m_pos - start);
  auto it = s_keywords.find(text);

  if (it == s_keywords.end()) {
    return make_token(TokenKind::Identifier, std::string(text));
  }

  Token token = make_token(it->second, std::string(text));

  if (it->second == TokenKind::Bool) {
    token.bool_val = (text == "true");
  }

  return token;
}

Token Tokenizer::scan_operator() {
  char character = advance();
  switch (character) {
    case '{':
      return make_token(TokenKind::LBrace);
    case '}':
      return make_token(TokenKind::RBrace);
    case '(':
      return make_token(TokenKind::LParen);
    case ')':
      return make_token(TokenKind::RParen);
    case '[':
      return make_token(TokenKind::LBracket);
    case ']':
      return make_token(TokenKind::RBracket);
    case ';':
      return make_token(TokenKind::Semicolon);
    case ',':
      return make_token(TokenKind::Comma);
    case '.':
      return make_token(TokenKind::Dot);
    case '?':
      return make_token(TokenKind::Question);
    case ':':
      return make_token(TokenKind::Colon);
    case '~':
      return make_token(TokenKind::Tilde);
    case '+': {
      if (match('+')) {
        return make_token(TokenKind::PlusPlus);
      }

      if (match('=')) {
        return make_token(TokenKind::PlusEq);
      }

      return make_token(TokenKind::Plus);
    }
    case '-': {
      if (match('>')) {
        return make_token(TokenKind::Arrow);
      }

      if (match('-')) {
        return make_token(TokenKind::MinusMinus);
      }

      if (match('=')) {
        return make_token(TokenKind::MinusEq);
      }

      return make_token(TokenKind::Minus);
    }
    case '*': {
      return match('=') ? make_token(TokenKind::StarEq)
                        : make_token(TokenKind::Star);
    }
    case '/': {
      return match('=') ? make_token(TokenKind::SlashEq)
                        : make_token(TokenKind::Slash);
    }
    case '%': {
      return match('=') ? make_token(TokenKind::PercentEq)
                        : make_token(TokenKind::Percent);
    }
    case '=': {
      return match('=') ? make_token(TokenKind::EqEq)
                        : make_token(TokenKind::Eq);
    }
    case '!': {
      return match('=') ? make_token(TokenKind::BangEq)
                        : make_token(TokenKind::Bang);
    }
    case '<': {
      if (match('<')) {
        return match('=') ? make_token(TokenKind::LtLtEq)
                          : make_token(TokenKind::LtLt);
      }

      return match('=') ? make_token(TokenKind::LtEq)
                        : make_token(TokenKind::Lt);
    }
    case '>': {
      if (match('>')) {
        return match('=') ? make_token(TokenKind::GtGtEq)
                          : make_token(TokenKind::GtGt);
      }

      return match('=') ? make_token(TokenKind::GtEq)
                        : make_token(TokenKind::Gt);
    }
    case '&': {
      if (match('&')) {
        return make_token(TokenKind::AmpAmp);
      }

      return match('=') ? make_token(TokenKind::AmpEq)
                        : make_token(TokenKind::Amp);
    }
    case '|': {
      if (match('|')) {
        return make_token(TokenKind::PipePipe);
      }

      return match('=') ? make_token(TokenKind::PipeEq)
                        : make_token(TokenKind::Pipe);
    }

    case '^': {
      return match('=') ? make_token(TokenKind::CaretEq)
                        : make_token(TokenKind::Caret);
    }

    default:
      return error_token(std::string("unexpected character '") + character +
                         "'");
  }
}

} // namespace astralix
