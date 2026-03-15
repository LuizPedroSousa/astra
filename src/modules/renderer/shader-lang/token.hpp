#pragma once

#include <cstdint>
#include <string>
#include <string_view>
namespace astralix {

enum class TokenKind : uint8_t {
  // Literals
  Int,
  Float,
  Bool,
  String,
  // Identifier
  Identifier,
  // Decorators
  AtVertexStage,
  AtGeometryStage,
  AtComputeStage,
  AtFragmentStage,
  AtVersion,
  AtInclude,
  AtDefine,
  AtLocation,
  AtBinding,
  AtSet,
  AtStd430,
  AtStd140,
  AtPushConstant,
  AtUniform,
  AtIn,
  // Keywords
  KeywordIn,
  KeywordOut,
  KeywordInout,
  KeywordUniform,
  KeywordBuffer,
  KeywordStruct,
  KeywordConst,
  KeywordVoid,
  KeywordIf,
  KeywordElse,
  KeywordFor,
  KeywordWhile,
  KeywordDo,
  KeywordReturn,
  KeywordBreak,
  KeywordContinue,
  KeywordDiscard,
  KeywordInterface,
  KeywordFunction,
  // Types
  TypeBool,
  TypeInt,
  TypeUint,
  TypeFloat,
  TypeVec2,
  TypeVec3,
  TypeVec4,
  TypeIvec2,
  TypeIvec3,
  TypeIvec4,
  TypeUvec2,
  TypeUvec3,
  TypeUvec4,
  TypeMat2,
  TypeMat3,
  TypeMat4,
  TypeSampler2D,
  TypeSamplerCube,
  TypeSampler2DShadow,
  TypeIsampler2D,
  TypeUsampler2D,
  // Punctuation
  LBrace,
  RBrace,
  LParen,
  RParen,
  LBracket,
  RBracket,
  Semicolon,
  Comma,
  Dot,
  // Operators
  Plus,
  Minus,
  Arrow,
  Star,
  Slash,
  Percent,
  Eq,
  EqEq,
  Bang,
  BangEq,
  Lt,
  Gt,
  LtEq,
  GtEq,
  AmpAmp,
  PipePipe,
  Amp,
  Pipe,
  Caret,
  Tilde,
  LtLt,
  GtGt,
  PlusEq,
  MinusEq,
  StarEq,
  SlashEq,
  PercentEq,
  AmpEq,
  PipeEq,
  CaretEq,
  LtLtEq,
  GtGtEq,
  PlusPlus,
  MinusMinus,
  Question,
  Colon,
  // Special
  EoF,
  Error,
};

struct SourceLocation {
  uint32_t line, col;
  std::string_view file;
};

struct Token {
  TokenKind kind = TokenKind::Error;
  SourceLocation location = {};
  std::string lexeme;
  union {
    int64_t int_val = 0;
    double float_val;
    bool bool_val;
  };
};

} // namespace astralix
