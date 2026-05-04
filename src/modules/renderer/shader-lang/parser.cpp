#include "shader-lang/parser.hpp"
#include "shader-lang/ast.hpp"
#include "shader-lang/diagnostics.hpp"
#include "shader-lang/token.hpp"
#include <optional>
#include <set>
#include <unordered_map>
#include <variant>

namespace astralix {

/* @INFO: Pratt parser binding powers. Each precedence level n maps to:
 * > left_binding_power  = BINDING_POWER_L(n) = 2n-1 (
 * >   used when the op is the left edge of a sub-expr
 * >  )
 * > right_binding_power = BINDING_POWER_R(n) = 2n (
 * >    used when the op is the right edge,
 * >    for right-associative: BINDING_POWER_R(n-1)
 * > )
 */
#define BINDING_POWER_L(n) ((n) * 2 - 1)
#define BINDING_POWER_R(n) ((n) * 2)

#define PRECEDENCE_ASSIGN 1
#define PRECEDENCE_TERNARY 2
#define PRECEDENCE_LOGICAL_OR 3
#define PRECEDENCE_LOGICAL_AND 4
#define PRECEDENCE_BIT_OR 5
#define PRECEDENCE_BIT_XOR 6
#define PRECEDENCE_BIT_AND 7
#define PRECEDENCE_EQUALITY 8
#define PRECEDENCE_RELATIONAL 9
#define PRECEDENCE_SHIFT 10
#define PRECEDENCE_ADDITIVE 11
#define PRECEDENCE_MULT 12
#define PRECEDENCE_UNARY 14
#define PRECEDENCE_POSTFIX 15

static TypeRef type_from_keyword(TokenKind kind) { return TypeRef{kind, {}}; }

static uint32_t token_src_length(const Token &token) {
  if (!token.lexeme.empty()) {
    return static_cast<uint32_t>(token.lexeme.size());
  }

  switch (token.kind) {
    case TokenKind::LtLtEq:
    case TokenKind::GtGtEq:
      return 3;
    case TokenKind::PlusPlus:
    case TokenKind::MinusMinus:
    case TokenKind::EqEq:
    case TokenKind::BangEq:
    case TokenKind::LtEq:
    case TokenKind::GtEq:
    case TokenKind::AmpAmp:
    case TokenKind::PipePipe:
    case TokenKind::LtLt:
    case TokenKind::GtGt:
    case TokenKind::PlusEq:
    case TokenKind::MinusEq:
    case TokenKind::StarEq:
    case TokenKind::SlashEq:
    case TokenKind::PercentEq:
    case TokenKind::AmpEq:
    case TokenKind::PipeEq:
    case TokenKind::CaretEq:
      return 2;
    default:
      return 1;
  }
}

static bool
annotation_allowed(AnnotationKind kind,
                   std::initializer_list<AnnotationKind> allowed_kinds) {
  for (AnnotationKind allowed_kind : allowed_kinds) {
    if (kind == allowed_kind) {
      return true;
    }
  }

  return false;
}

static const Attribute *find_attribute(const AttributeList &attributes,
                                       AttributeKind kind) {
  for (const auto &attribute : attributes) {
    if (attribute.kind == kind) {
      return &attribute;
    }
  }
  return nullptr;
}

static const Annotation *find_annotation(const Annotations &annotations,
                                         AnnotationKind kind) {
  for (const auto &annotation : annotations) {
    if (annotation.kind == kind) {
      return &annotation;
    }
  }
  return nullptr;
}

static void validate_exclusive_attributes(std::vector<std::string> &errors,
                                          const AttributeList &attributes,
                                          std::string_view target,
                                          std::string_view source,
                                          AttributeKind lhs_kind,
                                          AttributeKind rhs_kind) {
  const Attribute *lhs = find_attribute(attributes, lhs_kind);
  const Attribute *rhs = find_attribute(attributes, rhs_kind);

  if (!lhs || !rhs) {
    return;
  }

  PUSH_ATTRIBUTE_EXCLUSIVE_CONFLICT(errors, *lhs, target, source, lhs_kind);
  PUSH_ATTRIBUTE_EXCLUSIVE_CONFLICT(errors, *rhs, target, source, rhs_kind);
}

static void validate_exclusive_annotations(std::vector<std::string> &errors,
                                           const Annotations &annotations,
                                           std::string_view target,
                                           std::string_view source,
                                           AnnotationKind lhs_kind,
                                           AnnotationKind rhs_kind) {
  const Annotation *lhs = find_annotation(annotations, lhs_kind);
  const Annotation *rhs = find_annotation(annotations, rhs_kind);

  if (!lhs || !rhs) {
    return;
  }

  PUSH_ANNOTATION_EXCLUSIVE_CONFLICT(errors, *lhs, target, source, lhs_kind);
  PUSH_ANNOTATION_EXCLUSIVE_CONFLICT(errors, *rhs, target, source, rhs_kind);
}

static void validate_allowed_annotations(
    std::vector<std::string> &errors, const Annotations &annotations,
    std::string_view target, std::string_view source,
    std::initializer_list<AnnotationKind> allowed_kinds) {
  for (const auto &annotation : annotations) {
    if (!annotation_allowed(annotation.kind, allowed_kinds)) {
      push_located_error(
          errors, annotation.location,
          format_invalid_annotation_message(
              annotation_kind_name(annotation.kind), target, allowed_kinds),
          source);
    }
  }
}

static void validate_required_annotations(
    std::vector<std::string> &errors, const Annotations &annotations,
    const SourceLocation &location, std::string_view target,
    std::string_view source,
    std::initializer_list<AnnotationKind> required_kinds) {
  for (AnnotationKind required_kind : required_kinds) {
    if (find_annotation(annotations, required_kind) == nullptr) {
      push_located_error(errors, location,
                         format_missing_required_annotation_message(
                             annotation_kind_name(required_kind), target),
                         source);
    }
  }
}

int Parser::left_binding_power(TokenKind op) {
  switch (op) {
    case TokenKind::Eq:
    case TokenKind::PlusEq:
    case TokenKind::MinusEq:
    case TokenKind::StarEq:
    case TokenKind::SlashEq:
    case TokenKind::PercentEq:
    case TokenKind::AmpEq:
    case TokenKind::PipeEq:
    case TokenKind::CaretEq:
    case TokenKind::LtLtEq:
    case TokenKind::GtGtEq:
      return BINDING_POWER_L(PRECEDENCE_ASSIGN);
    case TokenKind::Question:
      return BINDING_POWER_L(PRECEDENCE_TERNARY);
    case TokenKind::PipePipe:
      return BINDING_POWER_L(PRECEDENCE_LOGICAL_OR);
    case TokenKind::AmpAmp:
      return BINDING_POWER_L(PRECEDENCE_LOGICAL_AND);
    case TokenKind::Pipe:
      return BINDING_POWER_L(PRECEDENCE_BIT_OR);
    case TokenKind::Caret:
      return BINDING_POWER_L(PRECEDENCE_BIT_XOR);
    case TokenKind::Amp:
      return BINDING_POWER_L(PRECEDENCE_BIT_AND);
    case TokenKind::EqEq:
    case TokenKind::BangEq:
      return BINDING_POWER_L(PRECEDENCE_EQUALITY);
    case TokenKind::Lt:
    case TokenKind::Gt:
    case TokenKind::LtEq:
    case TokenKind::GtEq:
      return BINDING_POWER_L(PRECEDENCE_RELATIONAL);
    case TokenKind::LtLt:
    case TokenKind::GtGt:
      return BINDING_POWER_L(PRECEDENCE_SHIFT);
    case TokenKind::Plus:
    case TokenKind::Minus:
      return BINDING_POWER_L(PRECEDENCE_ADDITIVE);
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
      return BINDING_POWER_L(PRECEDENCE_MULT);
    case TokenKind::PlusPlus:
    case TokenKind::MinusMinus:
    case TokenKind::LBracket:
    case TokenKind::Dot:
    case TokenKind::LParen:
      return BINDING_POWER_L(PRECEDENCE_POSTFIX);
    default:
      return -1;
  }
}

int Parser::right_binding_power(TokenKind op) {
  switch (op) {
    case TokenKind::Eq:
    case TokenKind::PlusEq:
    case TokenKind::MinusEq:
    case TokenKind::StarEq:
    case TokenKind::SlashEq:
    case TokenKind::PercentEq:
    case TokenKind::AmpEq:
    case TokenKind::PipeEq:
    case TokenKind::CaretEq:
    case TokenKind::LtLtEq:
    case TokenKind::GtGtEq:
      return BINDING_POWER_R(PRECEDENCE_ASSIGN);
    case TokenKind::Question:
      return BINDING_POWER_R(PRECEDENCE_TERNARY);
    case TokenKind::PipePipe:
      return BINDING_POWER_R(PRECEDENCE_LOGICAL_OR);
    case TokenKind::AmpAmp:
      return BINDING_POWER_R(PRECEDENCE_LOGICAL_AND);
    case TokenKind::Pipe:
      return BINDING_POWER_R(PRECEDENCE_BIT_OR);
    case TokenKind::Caret:
      return BINDING_POWER_R(PRECEDENCE_BIT_XOR);
    case TokenKind::Amp:
      return BINDING_POWER_R(PRECEDENCE_BIT_AND);
    case TokenKind::EqEq:
    case TokenKind::BangEq:
      return BINDING_POWER_R(PRECEDENCE_EQUALITY);
    case TokenKind::Lt:
    case TokenKind::Gt:
    case TokenKind::LtEq:
    case TokenKind::GtEq:
      return BINDING_POWER_R(PRECEDENCE_RELATIONAL);
    case TokenKind::LtLt:
    case TokenKind::GtGt:
      return BINDING_POWER_R(PRECEDENCE_SHIFT);
    case TokenKind::Plus:
    case TokenKind::Minus:
      return BINDING_POWER_R(PRECEDENCE_ADDITIVE);
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
      return BINDING_POWER_R(PRECEDENCE_MULT);
    default:
      return -1;
  }
}

NodeID Parser::parse_expr(int min_bp) {
  NodeID lhs = parse_prefix();

  while (true) {
    TokenKind op = peek().kind;
    int left_bp = left_binding_power(op);

    if (left_bp < 0 || left_bp <= min_bp) {
      break;
    }

    SourceLocation location = peek().location;

    if (op == TokenKind::Question) {
      advance();
      NodeID then_expr = parse_expr(0);
      expect(TokenKind::Colon, "expected ':' in ternary expression");
      NodeID else_expr = parse_expr(right_binding_power(op) - 1);
      lhs = push_node(TernaryExpr{lhs, then_expr, else_expr}, location);
      continue;
    }

    if (op == TokenKind::PlusPlus || op == TokenKind::MinusMinus) {
      advance();
      lhs = push_node(UnaryExpr{lhs, op, false}, location);
      continue;
    }

    if (op == TokenKind::LBracket) {
      advance();
      NodeID index = parse_expr(0);
      expect(TokenKind::RBracket, "expected ']'");
      lhs = push_node(IndexExpr{lhs, index}, location);
      continue;
    }

    if (op == TokenKind::Dot) {
      advance();
      Token field = expect_name("expected field name after '.'");
      lhs = push_node(FieldExpr{lhs, field.lexeme}, location);
      continue;
    }

    if (op == TokenKind::LParen) {
      if (auto *identifier_expr =
              std::get_if<IdentifierExpr>(&peek_node_at(lhs).data)) {
        if (!is_declared(identifier_expr->name)) {
          m_nodes[lhs].data = UnresolvedRef{identifier_expr->name};
        }
      }

      advance();
      std::vector<NodeID> args;
      if (!check(TokenKind::RParen)) {
        args.push_back(parse_expr(0));
        while (match(TokenKind::Comma)) {
          args.push_back(parse_expr(0));
        }
      }

      expect(TokenKind::RParen, "expected ')'");
      lhs = push_node(CallExpr{lhs, std::move(args)}, location);
      continue;
    }

    advance();
    NodeID rhs = parse_expr(right_binding_power(op));
    lhs = push_node(is_assign_op(op) ? ASTNodeData(AssignExpr{lhs, rhs, op})
                                     : ASTNodeData(BinaryExpr{lhs, rhs, op}),
                    location);
  }

  return lhs;
}

NodeID Parser::parse_prefix() {
  Token token = peek();
  SourceLocation location = token.location;

  switch (token.kind) {
    case TokenKind::Int: {
      advance();
      return push_node(LiteralExpr{token.int_val}, location);
    }
    case TokenKind::Float: {
      advance();
      return push_node(LiteralExpr{token.float_val}, location);
    }
    case TokenKind::Bool: {
      advance();
      return push_node(LiteralExpr{token.bool_val}, location);
    }
    case TokenKind::Identifier: {
      advance();
      return push_node(IdentifierExpr{token.lexeme}, location);
    }
    case TokenKind::LParen: {
      advance();
      NodeID inner = parse_expr(0);
      expect(TokenKind::RParen, "expected ')'");
      return inner;
    }
    case TokenKind::Bang:
    case TokenKind::Tilde:
    case TokenKind::Minus:
    case TokenKind::Plus:
    case TokenKind::PlusPlus:
    case TokenKind::MinusMinus: {
      advance();
      NodeID operand = parse_expr(BINDING_POWER_L(PRECEDENCE_UNARY));
      return push_node(UnaryExpr{operand, token.kind, true}, location);
    }
    default: {
      if (is_type_keyword(token.kind)) {
        advance();
        TypeRef type_ref = type_from_keyword(token.kind);
        parse_type_array_suffix(type_ref, true);

        expect(TokenKind::LParen, "expected '(' after type constructor");
        std::vector<NodeID> args;
        if (!check(TokenKind::RParen)) {
          args.push_back(parse_expr(0));
          while (match(TokenKind::Comma)) {
            args.push_back(parse_expr(0));
          }
        }
        expect(TokenKind::RParen, "expected ')'");
        return push_node(ConstructExpr{type_ref, std::move(args)}, location);
      }
      {
        std::string prefix = location.file.empty()
                                 ? (std::to_string(location.line) + ":" +
                                    std::to_string(location.col) + ": ")
                                 : (std::string(location.file) + ":" +
                                    std::to_string(location.line) + ":" +
                                    std::to_string(location.col) + ": ");
        std::string err =
            prefix + "unexpected token in expression: '" + token.lexeme + "'";
        if (!m_source.empty())
          err += '\n' +
                 format_source_context(m_source, location.line, location.col);
        m_errors.push_back(std::move(err));
      }
      synchronize();
      return push_node(LiteralExpr{int64_t(0)}, location);
    }
  }
}

NodeID Parser::parse_stmt() {
  switch (peek().kind) {
    case TokenKind::LBrace: {
      return parse_block_stmt();
    }
    case TokenKind::KeywordIf: {
      return parse_if_stmt();
    }
    case TokenKind::KeywordFor:
      return parse_for_stmt();
    case TokenKind::KeywordWhile:
      return parse_while_stmt();
    case TokenKind::KeywordDo:
      return parse_do_while_stmt();
    case TokenKind::KeywordConst:
      return parse_const_decl();
    case TokenKind::KeywordReturn:
      return parse_return_stmt();
    case TokenKind::KeywordBreak: {
      SourceLocation location = peek().location;
      advance();
      expect(TokenKind::Semicolon, "expected ';'");
      return push_node(BreakStmt{}, location);
    }
    case TokenKind::KeywordContinue: {
      SourceLocation location = peek().location;
      advance();
      expect(TokenKind::Semicolon, "expected ';'");
      return push_node(ContinueStmt{}, location);
    }
    case TokenKind::KeywordDiscard: {
      SourceLocation location = peek().location;
      advance();
      expect(TokenKind::Semicolon, "expected ';'");
      return push_node(DiscardStmt{}, location);
    }
    default:
      return parse_local_var_or_expr_stmt();
  }
}

NodeID Parser::parse_if_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordIf, "expected 'if'");
  expect(TokenKind::LParen, "expected '('");
  NodeID cond = parse_expr(0);
  expect(TokenKind::RParen, "expected ')'");
  NodeID then_br = parse_stmt();
  std::optional<NodeID> else_br;
  if (match(TokenKind::KeywordElse))
    else_br = parse_stmt();
  return push_node(IfStmt{cond, then_br, else_br}, location);
}

NodeID Parser::parse_for_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordFor, "expected 'for'");
  expect(TokenKind::LParen, "expected '('");
  NodeID init = parse_local_var_or_expr_stmt(); // consumes its own ';'
  std::optional<NodeID> cond, step;
  if (!check(TokenKind::Semicolon))
    cond = parse_expr(0);
  expect(TokenKind::Semicolon, "expected ';'");
  if (!check(TokenKind::RParen))
    step = parse_expr(0);
  expect(TokenKind::RParen, "expected ')'");
  NodeID body = parse_stmt();
  return push_node(ForStmt{init, cond, step, body}, location);
}

NodeID Parser::parse_var_decl() {
  SourceLocation location = peek().location;
  TypeRef ret = parse_type_ref();
  Token name = expect_name("expected name");
  parse_type_array_suffix(ret, true);

  // if (check(TokenKind::LParen)) {
  //   return parse_function_decl(ret, name.lexeme, location);
  // }

  std::optional<NodeID> init;

  if (match(TokenKind::Eq)) {
    init = parse_expr(0);
  }

  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(VarDecl{ret, name.lexeme, init}, location);
}

NodeID Parser::parse_function_decl(AttributeList attributes) {
  SourceLocation location = peek().location;

  expect(TokenKind::KeywordFunction, "expected 'fn'");

  Token name = expect_name("expected name");

  std::optional<StageKind> stage_kind;
  std::array<uint32_t, 3> local_size = {1u, 1u, 1u};

  for (const auto &attribute : attributes) {
    switch (attribute.kind) {
      case AttributeKind::VertexStage:
        stage_kind = StageKind::Vertex;
        break;
      case AttributeKind::GeometryStage:
        stage_kind = StageKind::Geometry;
        break;
      case AttributeKind::FragmentStage:
        stage_kind = StageKind::Fragment;
        break;
      case AttributeKind::ComputeStage:
        stage_kind = StageKind::Compute;
        if (const auto *sizes =
                std::get_if<std::array<int32_t, 3>>(&attribute.value)) {
          local_size = {
              static_cast<uint32_t>((*sizes)[0]),
              static_cast<uint32_t>((*sizes)[1]),
              static_cast<uint32_t>((*sizes)[2]),
          };
        }
        break;
      default:
        PUSH_INVALID_ATTRIBUTE(
            m_errors, attribute, "function declaration", m_source,
            AttributeKind::VertexStage, AttributeKind::GeometryStage,
            AttributeKind::FragmentStage, AttributeKind::ComputeStage);
        break;
    }
  }

  declare(name.lexeme);

  expect(TokenKind::LParen, "expected '('");

  std::vector<NodeID> params;

  if (!check(TokenKind::RParen)) {
    params.push_back(parse_param());
    while (match(TokenKind::Comma)) {
      params.push_back(parse_param());
    }
  }

  expect(TokenKind::RParen, "expected ')'");

  expect(TokenKind::Arrow, "expected '->'");

  TypeRef ret = parse_type_ref();
  parse_type_array_suffix(ret, false);

  NodeID body = parse_block_stmt();

  return push_node(FuncDecl{
                       ret,
                       std::move(name.lexeme),
                       std::move(params),
                       body,
                       std::move(stage_kind),
                       local_size,
                   },
                   location);
}

NodeID Parser::parse_param() {
  SourceLocation location = peek().location;
  ParamQualifier qual = ParamQualifier::None;
  if (match(TokenKind::KeywordIn))
    qual = ParamQualifier::In;
  else if (match(TokenKind::KeywordOut))
    qual = ParamQualifier::Out;
  else if (match(TokenKind::KeywordInout))
    qual = ParamQualifier::Inout;
  else if (match(TokenKind::KeywordConst))
    qual = ParamQualifier::Const;
  TypeRef type = parse_type_ref();
  Token name = expect_name("expected parameter name");
  parse_type_array_suffix(type, true);
  return push_node(ParamDecl{type, name.lexeme, qual}, location);
}

void Parser::synchronize() {
  while (!check(TokenKind::EoF)) {
    if (peek().kind == TokenKind::Semicolon) {
      advance();
      return;
    }
    if (peek().kind == TokenKind::RBrace)
      return;
    advance();
  }
}

bool Parser::is_type_keyword(TokenKind k) {
  switch (k) {
    case TokenKind::TypeBool:
    case TokenKind::TypeInt:
    case TokenKind::TypeUint:
    case TokenKind::TypeFloat:
    case TokenKind::TypeVec2:
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeIvec4:
    case TokenKind::TypeUvec2:
    case TokenKind::TypeUvec3:
    case TokenKind::TypeUvec4:
    case TokenKind::TypeMat2:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return true;
    default:
      return false;
  }
}

bool Parser::is_assign_op(TokenKind op) {
  switch (op) {
    case TokenKind::Eq:
    case TokenKind::PlusEq:
    case TokenKind::MinusEq:
    case TokenKind::StarEq:
    case TokenKind::SlashEq:
    case TokenKind::PercentEq:
    case TokenKind::AmpEq:
    case TokenKind::PipeEq:
    case TokenKind::CaretEq:
    case TokenKind::LtLtEq:
    case TokenKind::GtGtEq:
      return true;
    default:
      return false;
  }
}

Parser::Parser(std::vector<Token> tokens, std::string_view source)
    : m_tokens(std::move(tokens)), m_source(source) {
  for (const char *name : {
           "radians",
           "degrees",
           "sin",
           "cos",
           "tan",
           "asin",
           "acos",
           "atan",
           "sinh",
           "cosh",
           "tanh",
           "asinh",
           "acosh",
           "atanh",
           "pow",
           "exp",
           "log",
           "exp2",
           "log2",
           "sqrt",
           "inversesqrt",
           "abs",
           "sign",
           "floor",
           "trunc",
           "round",
           "roundEven",
           "ceil",
           "fract",
           "mod",
           "modf",
           "min",
           "max",
           "clamp",
           "mix",
           "step",
           "smoothstep",
           "isnan",
           "isinf",
           "length",
           "distance",
           "dot",
           "cross",
           "normalize",
           "reflect",
           "refract",
           "faceforward",
           "matrixCompMult",
           "outerProduct",
           "transpose",
           "determinant",
           "inverse",
           "lessThan",
           "lessThanEqual",
           "greaterThan",
           "greaterThanEqual",
           "equal",
           "notEqual",
           "any",
           "all",
           "not",
           "texture",
           "texture2D",
           "texture3D",
           "textureCube",
           "textureLod",
           "texture2DLod",
           "textureSize",
           "texelFetch",
           "texelFetchOffset",
           "textureOffset",
           "textureGrad",
           "textureGather",
           "imageLoad",
           "imageStore",
           "imageSize",
           "atomicAdd",
           "atomicMin",
           "atomicMax",
           "atomicAnd",
           "atomicOr",
           "atomicXor",
           "atomicExchange",
           "atomicCompSwap",
           "dFdx",
           "dFdy",
           "fwidth",
           "emit",
           "endPrimitive",
           "barrier",
           "memoryBarrier",
           "groupMemoryBarrier",
           "packUnorm2x16",
           "packSnorm2x16",
           "unpackUnorm2x16",
           "unpackSnorm2x16",
           "packHalf2x16",
           "unpackHalf2x16",
       })
    m_declared.insert(name);
}

Program Parser::parse() {
  Program program;

  program.version = 0;

  if (check(TokenKind::AtVersion)) {
    advance();
    Token version = expect(TokenKind::Int, "expected version number");
    program.version = static_cast<int>(version.int_val);
    expect(TokenKind::Semicolon, "expected ';'");
  }

  while (check(TokenKind::AtInclude)) {
    advance();
    Token path = expect(TokenKind::String, "expected include path");
    expect(TokenKind::Semicolon, "expected ';'");
    program.includes.push_back(path.lexeme);
  }

  while (!check(TokenKind::EoF)) {
    size_t pos_before = m_pos;

    // if (check(TokenKind::AtStage)) {
    //   program.stages.push_back(parse_stage_block());
    // } else {
    // }

    NodeID decl = parse_global_decl();

    if (const auto *fn = std::get_if<FuncDecl>(&peek_node_at(decl).data)) {
      if (fn->stage_kind.has_value()) {
        program.stages.push_back(decl);
      } else {
        program.globals.push_back(decl);
      }
    } else {
      program.globals.push_back(decl);
    }

    if (m_pos == pos_before) {
      report_parser_stuck("parse", peek());
      advance();
    }
  }
  return program;
}

inline Token Parser::advance() {
  Token token = m_tokens[m_pos];
  m_last_token = token;

  if (m_pos + 1 < m_tokens.size()) {
    ++m_pos;
  }

  return token;
}

Token Parser::peek(int offset) const {
  size_t index = m_pos + static_cast<size_t>(offset);

  if (index >= m_tokens.size()) {
    return m_tokens.back();
  }

  return m_tokens[index];
}

Token Parser::expect(TokenKind kind, std::string_view msg) {
  if (check(kind)) {
    return advance();
  }

  SourceLocation loc = (m_last_token.location.line == 0)
                           ? peek().location
                           : SourceLocation{m_last_token.location.line,
                                            m_last_token.location.col +
                                                token_src_length(m_last_token),
                                            m_last_token.location.file};
  Token token = peek();
  PUSH_LOCATED_ERROR(m_errors, loc, std::string(msg), m_source);

  return token;
}

Token Parser::expect_name(std::string_view msg) {
  Token token = peek();

  bool is_name = token.kind == TokenKind::Identifier ||
                 (token.kind >= TokenKind::KeywordIn &&
                  token.kind <= TokenKind::TypeUsampler2D);

  if (is_name) {
    return advance();
  }

  SourceLocation location =
      (m_last_token.location.line == 0)
          ? token.location
          : SourceLocation{m_last_token.location.line,
                           m_last_token.location.col +
                               token_src_length(m_last_token),
                           m_last_token.location.file};
  PUSH_LOCATED_ERROR(m_errors, location, std::string(msg), m_source);

  return token;
}

inline bool Parser::check(TokenKind kind, int offset) const {
  return peek(offset).kind == kind;
}

bool Parser::match(TokenKind kind) {
  if (!check(kind)) {
    return false;
  }

  advance();
  return true;
}

NodeID Parser::push_node(ASTNodeData data, SourceLocation location) {
  NodeID id = static_cast<NodeID>(m_nodes.size());
  m_nodes.push_back(ASTNode{id, location, TypeRef{TokenKind::KeywordVoid, {}},
                            std::move(data)});
  return id;
}

NodeID Parser::parse_global_decl() {
  AttributeList attributes = parse_attributes();

  if (check(TokenKind::KeywordStruct)) {
    return parse_struct_decl();
  }

  if (check(TokenKind::KeywordConst)) {
    return parse_const_decl();
  }

  if (check(TokenKind::KeywordInterface)) {
    return parse_interface_decl(attributes);
  }

  if (check(TokenKind::KeywordIn) || check(TokenKind::KeywordOut)) {
    bool is_in = peek().kind == TokenKind::KeywordIn;

    advance();

    return check(TokenKind::LBrace, 1)
               ? parse_inline_interface_decl(attributes, is_in)
               : parse_interface_ref(is_in);
  }

  if (check(TokenKind::KeywordUniform) || check(TokenKind::KeywordBuffer)) {
    advance();

    bool is_block = (peek().kind == TokenKind::Identifier ||
                     (peek().kind >= TokenKind::KeywordIn &&
                      peek().kind <= TokenKind::TypeUsampler2D)) &&
                    check(TokenKind::LBrace, 1);

    if (is_block) {
      --m_pos;
      return parse_buffer_decl(std::move(attributes),
                               check(TokenKind::KeywordUniform));
    } else {
      --m_pos;
    }

    return parse_uniform_decl(std::move(attributes));
  }

  if (check(TokenKind::KeywordFunction)) {
    return parse_function_decl(std::move(attributes));
  }

  if (is_type_keyword(peek().kind) || peek().kind == TokenKind::KeywordVoid ||
      (peek().kind == TokenKind::Identifier &&
       check(TokenKind::Identifier, 1))) {
    return parse_var_decl();
  }

  SourceLocation location = peek().location;
  PUSH_LOCATED_ERROR(m_errors, location,
                     "unexpected token in global scope: '" + peek().lexeme +
                         "'; expected a global declaration",
                     m_source);
  synchronize();
  return push_node(LiteralExpr{int64_t(0)}, location);
}

// NodeID Parser::parse_stage_block() {
//   auto stage_token = peek();
//
//   SourceLocation location = peek().location;
//   expect(TokenKind::AtStage, "expected '@stage'");
//   StageKind kind = StageKind::Vertex;
//
//   const std::unordered_map<std::string_view, StageKind> s_alias_stages = {
//       {"vertex", StageKind::Vertex},
//       {"fragment", StageKind::Fragment},
//       {"geometry", StageKind::Geometry},
//       {"compute", StageKind::Compute}};
//
//   auto it = s_alias_stages.find(stage_token.lexeme);
//   if (it != s_alias_stages.end()) {
//     kind = it->second;
//   } else {
//     switch (peek().kind) {
//       case TokenKind::KeywordVertex:
//         advance();
//         kind = StageKind::Vertex;
//         break;
//       case TokenKind::KeywordFragment:
//         advance();
//         kind = StageKind::Fragment;
//         break;
//       case TokenKind::KeywordGeometry:
//         advance();
//         kind = StageKind::Geometry;
//         break;
//       case TokenKind::KeywordCompute:
//         advance();
//         kind = StageKind::Compute;
//         break;
//       default:
//         m_errors.push_back("expected stage name");
//     }
//   }
//
//   expect(TokenKind::LBrace, "expected '{'");
//   std::vector<NodeID> items;
//   while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
//     size_t pos_before = m_pos;
//     items.push_back(parse_stage_item());
//     if (m_pos == pos_before) {
//       report_parser_stuck("parse_stage_block", peek());
//       advance();
//     }
//   }
//   expect(TokenKind::RBrace, "expected '}'");
//   return push_node(StageBlock{kind, std::move(items)}, location);
// }

// NodeID Parser::parse_stage_item() {
//   // Annotations annotations = parse_annotations();
//   // if (check(TokenKind::KeywordIn) || check(TokenKind::KeywordOut)) {
//   //   bool is_in = peek().kind == TokenKind::KeywordIn;
//   //   advance();
//   //   return check(TokenKind::LBrace, 1) ? parse_interface_block(is_in)
//   //                                      : parse_interface_ref(is_in);
//   // }
//   //
//   // if (check(TokenKind::KeywordUniform)) {
//   //   bool is_block = (peek(1).kind == TokenKind::Identifier ||
//   //                    (peek(1).kind >= TokenKind::KeywordIn &&
//   //                     peek(1).kind <= TokenKind::TypeUsampler2D)) &&
//   //                   check(TokenKind::LBrace, 2);
//   //   if (is_block)
//   //     return parse_buffer_decl(std::move(annotations), true);
//   //   return parse_uniform_decl(std::move(annotations));
//   // }
//   //
//   // if (check(TokenKind::KeywordBuffer)) {
//   //   return parse_buffer_decl(std::move(annotations), false);
//   // }
//   //
//   // if (check(TokenKind::KeywordStruct)) {
//   //   return parse_struct_decl();
//   // }
//   //
//   // if (check(TokenKind::KeywordConst)) {
//   //   return parse_const_decl();
//   // }
//
//   return parse_var_decl();
// }

NodeID Parser::parse_interface_decl(AttributeList attributes) {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordInterface, "expected 'interface'");
  Token name = expect(TokenKind::Identifier, "expected interface name");
  InterfaceRole role = InterfaceRole::None;

  validate_exclusive_attributes(m_errors, attributes, "interface declaration",
                                m_source, AttributeKind::In,
                                AttributeKind::Uniform);

  for (const auto &attribute : attributes) {
    switch (attribute.kind) {
      case AttributeKind::In:
        role = InterfaceRole::In;
        break;
      case AttributeKind::Uniform:
        role = InterfaceRole::Uniform;
        break;
      default:
        PUSH_INVALID_ATTRIBUTE(m_errors, attribute, "interface declaration",
                               m_source, AttributeKind::In,
                               AttributeKind::Uniform);
        break;
    }
  }

  expect(TokenKind::LBrace, "expected '{'");

  std::vector<NodeID> fields;

  while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
    size_t pos_before = m_pos;

    auto attributes = parse_attributes();

    auto field_id = parse_field_decl(attributes);
    fields.push_back(field_id);

    if (m_pos == pos_before) {
      report_parser_stuck("parse_interface_decl", name.lexeme, peek());
      advance();
    }

    auto field_decl_node = peek_node_at(field_id);

    if (auto field_decl = std::get_if<FieldDecl>(&field_decl_node.data)) {
      validate_exclusive_annotations(
          m_errors, field_decl->annotations, "interface field declaration",
          m_source, AnnotationKind::Location, AnnotationKind::Uniform);

      switch (role) {
        case InterfaceRole::In:
          validate_required_annotations(m_errors, field_decl->annotations,
                                        field_decl_node.location,
                                        "interface field declaration", m_source,
                                        {AnnotationKind::Location});

          validate_allowed_annotations(m_errors, field_decl->annotations,
                                       "interface field declaration", m_source,
                                       {AnnotationKind::Location});
          break;
        case InterfaceRole::Uniform:
          // validate_required_annotations(m_errors, field_decl->annotations,
          //                               field_decl_node.location,
          //                               "interface field declaration",
          //                               m_source, {AnnotationKind::Binding});

          validate_allowed_annotations(
              m_errors, field_decl->annotations, "interface field declaration",
              m_source, {AnnotationKind::Binding, AnnotationKind::Set});
          break;
        case InterfaceRole::None:
          break;
      }

      // @TODO: Add validations for empty top-level annotations
    }
  }

  expect(TokenKind::RBrace, "expected '}'");

  declare(name.lexeme);

  return push_node(
      InterfaceDecl{name.lexeme, std::move(fields), role, std::nullopt},
      location);
}

NodeID Parser::parse_inline_interface_decl(AttributeList attributes,
                                           bool is_in) {
  SourceLocation location = peek().location;
  Token name = expect(TokenKind::Identifier, "expected inline interface name");
  expect(TokenKind::LBrace, "expected '{'");
  std::vector<NodeID> fields;

  InterfaceRole role = InterfaceRole::None;

  validate_exclusive_attributes(m_errors, attributes, "interface declaration",
                                m_source, AttributeKind::In,
                                AttributeKind::Uniform);

  Annotations annotations;

  if (!attributes.empty() && is_in) {
    for (const auto attribute : attributes) {
      Annotation annotation;
      annotation.location = attribute.location;

      switch (attribute.kind) {
        case AttributeKind::Binding: {
          annotation.kind = AnnotationKind::Binding;
          if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
            annotation.slot = *value;
          }
          annotations.push_back(annotation);
          break;
        }
        case AttributeKind::Set: {
          annotation.kind = AnnotationKind::Set;
          if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
            annotation.set = *value;
          }
          annotations.push_back(annotation);
          break;
        }
        case AttributeKind::Std430: {
          annotation.kind = AnnotationKind::Std430;
          annotations.push_back(annotation);
          break;
        }
        case AttributeKind::Std140: {
          annotation.kind = AnnotationKind::Std140;
          annotations.push_back(annotation);
          break;
        }
        case AttributeKind::PushConstant: {
          annotation.kind = AnnotationKind::PushConstant;
          annotations.push_back(annotation);
          break;
        }
        default:
          PUSH_INVALID_ATTRIBUTE(
              m_errors, attribute, "inline interface declaration", m_source,
              AttributeKind::Binding, AttributeKind::Set, AttributeKind::Std430,
              AttributeKind::Std140, AttributeKind::PushConstant);
          break;
      }
    }
  }

  while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
    size_t pos_before = m_pos;

    auto attributes = parse_attributes();

    fields.push_back(parse_field_decl(attributes));

    if (m_pos == pos_before) {
      report_parser_stuck("parse_inline_interface_decl", name.lexeme, peek());
      advance();
    }
  }

  expect(TokenKind::RBrace, "expected '}'");
  std::optional<std::string> instance;
  if (check(TokenKind::Identifier))
    instance = advance().lexeme;
  expect(TokenKind::Semicolon, "expected ';'");

  return push_node(InlineInterfaceDecl{is_in, name.lexeme, std::move(fields),
                                       instance, annotations},
                   location);
}

NodeID Parser::parse_interface_ref(bool is_in) {
  SourceLocation location = peek().location;
  Token name = expect(TokenKind::Identifier, "expected interface block name");
  std::optional<std::string> instance;
  if (check(TokenKind::Identifier))
    instance = advance().lexeme;
  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(InterfaceRef{is_in, name.lexeme, instance}, location);
}

NodeID Parser::parse_uniform_decl(AttributeList attributes) {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordUniform, "expected 'uniform'");
  TypeRef type = parse_type_ref();
  Token name = expect_name("expected uniform name");
  std::optional<uint32_t> array_size;

  Annotations annotations;

  for (const auto &attribute : attributes) {
    Annotation annotation;
    annotation.location = attribute.location;

    switch (attribute.kind) {
      case AttributeKind::Binding:
        annotation.kind = AnnotationKind::Binding;
        if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
          annotation.slot = *value;
        }
        annotations.push_back(annotation);
        break;
      case AttributeKind::Set:
        annotation.kind = AnnotationKind::Set;
        if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
          annotation.set = *value;
        }
        annotations.push_back(annotation);
        break;
      default:
        PUSH_INVALID_ATTRIBUTE(m_errors, attribute, "uniform declaration",
                               m_source, AttributeKind::Binding,
                               AttributeKind::Set);
        break;
    }
  }

  if (match(TokenKind::LBracket)) {
    if (check(TokenKind::Int)) {
      array_size = static_cast<uint32_t>(advance().int_val);
    }
    expect(TokenKind::RBracket, "expected ']'");
  }
  std::optional<NodeID> default_val;

  if (match(TokenKind::Eq)) {
    default_val = parse_expr(0);
  }

  expect(TokenKind::Semicolon, "expected ';'");

  return push_node(UniformDecl{type, name.lexeme, std::move(annotations),
                               default_val, array_size},
                   location);
}

NodeID Parser::parse_buffer_decl(AttributeList attributes, bool is_uniform) {
  SourceLocation location = peek().location;

  if (is_uniform) {
    expect(TokenKind::KeywordUniform, "expected 'uniform'");
  } else {
    expect(TokenKind::KeywordBuffer, "expected 'buffer'");
  }

  Token name = expect_name("expected block name");

  Annotations annotations;

  for (const auto &attribute : attributes) {
    Annotation annotation;
    annotation.location = attribute.location;

    switch (attribute.kind) {
      case AttributeKind::Binding:
        annotation.kind = AnnotationKind::Binding;
        if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
          annotation.slot = *value;
        }
        annotations.push_back(annotation);
        break;
      case AttributeKind::Set:
        annotation.kind = AnnotationKind::Set;
        if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
          annotation.set = *value;
        }
        annotations.push_back(annotation);
        break;
      case AttributeKind::Location:
        annotation.kind = AnnotationKind::Location;
        if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
          annotation.slot = *value;
        }
        annotations.push_back(annotation);
        break;
      case AttributeKind::PushConstant:
        annotation.kind = AnnotationKind::PushConstant;
        annotations.push_back(annotation);
        break;
      default:
        PUSH_INVALID_ATTRIBUTE(m_errors, attribute, "buffer declaration",
                               m_source, AttributeKind::Location,
                               AttributeKind::PushConstant,
                               AttributeKind::Location, AttributeKind::Binding);
        break;
    }
  }

  expect(TokenKind::LBrace, "expected '{'");
  std::vector<NodeID> fields;

  while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
    size_t pos_before = m_pos;
    fields.push_back(parse_field_decl());
    if (m_pos == pos_before) {
      report_parser_stuck("parse_buffer_decl", name.lexeme, peek());
      advance();
    }
  }

  expect(TokenKind::RBrace, "expected '}'");
  std::optional<std::string> instance_name;

  if (check(TokenKind::Identifier)) {
    instance_name = advance().lexeme;
  }

  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(BufferDecl{name.lexeme, std::move(fields),
                              std::move(annotations), instance_name,
                              is_uniform},
                   location);
}

NodeID Parser::parse_struct_decl() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordStruct, "expected 'struct'");
  Token name = expect(TokenKind::Identifier, "expected struct name");
  declare(name.lexeme);
  expect(TokenKind::LBrace, "expected '{'");
  std::vector<NodeID> fields;
  while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
    size_t pos_before = m_pos;
    fields.push_back(parse_field_decl());
    if (m_pos == pos_before) {
      report_parser_stuck("parse_struct_decl", name.lexeme, peek());
      advance();
    }
  }
  expect(TokenKind::RBrace, "expected '}'");
  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(StructDecl{name.lexeme, std::move(fields)}, location);
}

NodeID Parser::parse_const_decl() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordConst, "expected 'const'");
  TypeRef type = parse_type_ref();
  Token name = expect_name("expected name");
  expect(TokenKind::Eq, "expected '='");
  NodeID value_expr = parse_expr(0);
  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(VarDecl{type, name.lexeme, value_expr, true}, location);
}

NodeID Parser::parse_field_decl(std::optional<AttributeList> attributes) {
  SourceLocation location = peek().location;

  TypeRef type = parse_type_ref();
  Token name = expect_name("expected field name");

  Annotations annotations;

  if (attributes.has_value()) {
    validate_exclusive_attributes(m_errors, attributes.value(),
                                  "field declaration", m_source,
                                  AttributeKind::In, AttributeKind::Uniform);

    for (const auto &attribute : *attributes) {
      Annotation annotation;
      annotation.location = attribute.location;

      switch (attribute.kind) {
        case AttributeKind::In:
          annotation.kind = AnnotationKind::In;

          annotations.push_back(annotation);
          break;
        case AttributeKind::Location:
          annotation.kind = AnnotationKind::Location;
          if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
            annotation.slot = *value;
          }
          annotations.push_back(annotation);
          break;
        case AttributeKind::Uniform:
          annotation.kind = AnnotationKind::Uniform;
          annotations.push_back(annotation);
          break;
        case AttributeKind::Binding:
          annotation.kind = AnnotationKind::Binding;
          if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
            annotation.slot = *value;
          }
          annotations.push_back(annotation);
          break;
        case AttributeKind::Set:
          annotation.kind = AnnotationKind::Set;
          if (const auto *value = std::get_if<int32_t>(&attribute.value)) {
            annotation.set = *value;
          }
          annotations.push_back(annotation);
          break;
        default:
          PUSH_INVALID_ATTRIBUTE(m_errors, attribute, "field declaration",
                                 m_source, AttributeKind::In,
                                 AttributeKind::Uniform, AttributeKind::Binding,
                                 AttributeKind::Set, AttributeKind::Location);
          break;
      }
    }
  }

  std::optional<uint32_t> array_size;

  if (match(TokenKind::LBracket)) {
    if (check(TokenKind::Int)) {
      array_size = static_cast<uint32_t>(advance().int_val);
    } else {
      array_size = 0; // unbounded []
    }
    expect(TokenKind::RBracket, "expected ']'");
  }

  std::optional<NodeID> init;

  if (match(TokenKind::Eq)) {
    init = parse_expr(0);
  }

  expect(TokenKind::Semicolon, "expected ';'");

  return push_node(FieldDecl{type, name.lexeme, array_size, init, annotations},
                   location);
}

AttributeList Parser::parse_attributes() {
  AttributeList attributes;

  while (true) {
    auto current_token = peek();

    TokenKind kind = current_token.kind;

    Attribute attribute;

    switch (current_token.kind) {
      case TokenKind::AtLocation:
        attribute.kind = AttributeKind::Location;
        break;
      case TokenKind::AtBinding:
        attribute.kind = AttributeKind::Binding;
        break;
      case TokenKind::AtSet:
        attribute.kind = AttributeKind::Set;
        break;
      case TokenKind::AtStd430:
        attribute.kind = AttributeKind::Std430;
        break;
      case TokenKind::AtStd140:
        attribute.kind = AttributeKind::Std140;
        break;
      case TokenKind::AtPushConstant:
        attribute.kind = AttributeKind::PushConstant;
        break;
      case TokenKind::AtUniform:
        attribute.kind = AttributeKind::Uniform;
        break;
      case TokenKind::AtIn:
        attribute.kind = AttributeKind::In;
        break;
      case TokenKind::AtVertexStage: {
        attribute.kind = AttributeKind::VertexStage;
        break;
      }
      case TokenKind::AtGeometryStage: {
        attribute.kind = AttributeKind::GeometryStage;
        break;
      }
      case TokenKind::AtFragmentStage: {
        attribute.kind = AttributeKind::FragmentStage;
        break;
      }
      case TokenKind::AtComputeStage: {
        attribute.kind = AttributeKind::ComputeStage;
        break;
      }
      default: {
        return attributes;
      }
    }

    advance();

    if (match(TokenKind::LParen)) {
      if (attribute.kind == AttributeKind::ComputeStage) {
        std::array<int32_t, 3> local_size = {1, 1, 1};

        for (size_t axis = 0; axis < local_size.size(); ++axis) {
          Token size_token = expect(TokenKind::Int, "expected integer");
          local_size[axis] = static_cast<int32_t>(size_token.int_val);

          if (axis + 1 < local_size.size()) {
            expect(TokenKind::Comma, "expected ','");
          }
        }

        attribute.value = local_size;
      } else if (check(TokenKind::Int)) {
        attribute.value = static_cast<int32_t>(advance().int_val);
      }

      expect(TokenKind::RParen, "expected ')'");
    }

    attributes.push_back(attribute);
  }

  return attributes;
}

Annotations Parser::parse_annotations() {
  Annotations annotations;

  const std::unordered_map<std::string_view, StageKind> s_alias_stages = {
      {"vertex", StageKind::Vertex},
      {"fragment", StageKind::Fragment},
      {"geometry", StageKind::Geometry},
      {"compute", StageKind::Compute}};

  while (true) {

    auto current_token = peek();

    TokenKind kind = current_token.kind;

    auto it = s_alias_stages.find(current_token.lexeme);

    switch (kind) {
      case TokenKind::AtLocation:
      case TokenKind::AtBinding:
      case TokenKind::AtSet:
      case TokenKind::AtStd430:
      case TokenKind::AtStd140:
      case TokenKind::AtPushConstant: {
        advance();
        Annotation annotation;
        annotation.location = current_token.location;

        switch (kind) {
          case TokenKind::AtLocation:
            annotation.kind = AnnotationKind::Location;
            break;
          case TokenKind::AtBinding:
            annotation.kind = AnnotationKind::Binding;
            break;
          case TokenKind::AtSet:
            annotation.kind = AnnotationKind::Set;
            break;
          case TokenKind::AtStd430:
            annotation.kind = AnnotationKind::Std430;
            break;
          case TokenKind::AtStd140:
            annotation.kind = AnnotationKind::Std140;
            break;
          case TokenKind::AtPushConstant:
            annotation.kind = AnnotationKind::PushConstant;
            break;
          default:
            break;
        }

        if (match(TokenKind::LParen)) {
          if (check(TokenKind::Int))
            annotation.slot = static_cast<int32_t>(advance().int_val);
          expect(TokenKind::RParen, "expected ')'");
        }
        annotations.push_back(annotation);
      };
      default:
        break;
    }
  }
  return annotations;
}

TypeRef Parser::parse_type_ref() {
  Token token = peek();

  if (is_type_keyword(token.kind) || token.kind == TokenKind::KeywordVoid) {
    advance();
    return TypeRef{token.kind, {}};
  }
  if (token.kind == TokenKind::Identifier) {
    advance();
    return TypeRef{TokenKind::Identifier, token.lexeme};
  }
  {
    const auto &loc = token.location;
    std::string prefix =
        loc.file.empty()
            ? (std::to_string(loc.line) + ":" + std::to_string(loc.col) + ": ")
            : (std::string(loc.file) + ":" + std::to_string(loc.line) + ":" +
               std::to_string(loc.col) + ": ");
    std::string err = prefix + "expected type";
    if (!m_source.empty())
      err += '\n' + format_source_context(m_source, loc.line, loc.col);
    m_errors.push_back(std::move(err));
  }
  return TypeRef{TokenKind::KeywordVoid, {}};
}

void Parser::parse_type_array_suffix(TypeRef &type_ref,
                                     bool allow_runtime_sized) {
  if (!match(TokenKind::LBracket)) {
    return;
  }

  const SourceLocation bracket_location = m_last_token.location;

  if (check(TokenKind::Int)) {
    type_ref.array_size = static_cast<uint32_t>(advance().int_val);
  } else if (check(TokenKind::RBracket)) {
    if (allow_runtime_sized) {
      type_ref.array_size = 0;
      type_ref.is_runtime_sized = true;
    } else {
      PUSH_LOCATED_ERROR(m_errors, bracket_location,
                         "runtime-sized array types are not allowed here",
                         m_source);
    }
  }

  expect(TokenKind::RBracket, "expected ']'");
}

NodeID Parser::parse_block_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::LBrace, "expected '{'");
  std::vector<NodeID> stmts;
  while (!check(TokenKind::RBrace) && !check(TokenKind::EoF)) {
    size_t pos_before = m_pos;
    stmts.push_back(parse_stmt());
    if (m_pos == pos_before) {
      report_parser_stuck("parse_block_stmt", peek());
      advance();
    }
  }
  expect(TokenKind::RBrace, "expected '}'");
  return push_node(BlockStmt{std::move(stmts)}, location);
}

NodeID Parser::parse_while_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordWhile, "expected 'while'");
  expect(TokenKind::LParen, "expected '('");
  NodeID cond = parse_expr(0);
  expect(TokenKind::RParen, "expected ')'");
  NodeID body = parse_stmt();
  return push_node(WhileStmt{cond, body}, location);
}

NodeID Parser::parse_do_while_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordDo, "expected 'do'");
  NodeID body = parse_stmt();
  expect(TokenKind::KeywordWhile, "expected 'while'");
  expect(TokenKind::LParen, "expected '('");
  NodeID cond = parse_expr(0);
  expect(TokenKind::RParen, "expected ')'");
  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(WhileStmt{cond, body}, location);
}

NodeID Parser::parse_return_stmt() {
  SourceLocation location = peek().location;
  expect(TokenKind::KeywordReturn, "expected 'return'");
  std::optional<NodeID> value;
  if (!check(TokenKind::Semicolon))
    value = parse_expr(0);
  expect(TokenKind::Semicolon, "expected ';'");
  return push_node(ReturnStmt{value}, location);
}

NodeID Parser::parse_local_var_or_expr_stmt() {
  bool starts_type =
      is_type_keyword(peek().kind) || peek().kind == TokenKind::KeywordVoid ||
      (peek().kind == TokenKind::Identifier && check(TokenKind::Identifier, 1));

  if (starts_type) {
    return parse_var_decl();
  }

  NodeID expr = parse_expr(0);

  expect(TokenKind::Semicolon, "expected ';'");

  return push_node(ExprStmt{expr}, m_nodes[expr].location);
}

#undef PRECEDENCE_ASSIGN
#undef PRECEDENCE_TERNARY
#undef PRECEDENCE_LOGICAL_OR
#undef PRECEDENCE_LOGICAL_AND
#undef PRECEDENCE_BIT_OR
#undef PRECEDENCE_BIT_XOR
#undef PRECEDENCE_BIT_AND
#undef PRECEDENCE_EQUALITY
#undef PRECEDENCE_RELATIONAL
#undef PRECEDENCE_SHIFT
#undef PRECEDENCE_ADDITIVE
#undef PRECEDENCE_MULT
#undef PRECEDENCE_UNARY
#undef PRECEDENCE_POSTFIX

} // namespace astralix
