#pragma once

#include "base.hpp"
#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/reflection.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace astralix {

  struct GLSLExpr;
  struct GLSLStmt;

  using GLSLExprPtr = Scope<GLSLExpr>;
  using GLSLStmtPtr = Scope<GLSLStmt>;

  struct GLSLLiteralExpr {
    std::variant<bool, int64_t, double> value;
  };

  struct GLSLIdentifierExpr {
    std::string name;
  };

  struct GLSLBinaryExpr {
    GLSLExprPtr lhs;
    GLSLExprPtr rhs;
    TokenKind op;
  };

  struct GLSLUnaryExpr {
    GLSLExprPtr operand;
    TokenKind op;
    bool prefix = true;
  };

  struct GLSLTernaryExpr {
    GLSLExprPtr cond;
    GLSLExprPtr then_expr;
    GLSLExprPtr else_expr;
  };

  struct GLSLCallExpr {
    GLSLExprPtr callee;
    std::vector<GLSLExprPtr> args;
  };

  struct GLSLIndexExpr {
    GLSLExprPtr array;
    GLSLExprPtr index;
  };

  struct GLSLFieldExpr {
    GLSLExprPtr object;
    std::string field;
  };

  struct GLSLConstructExpr {
    TypeRef type;
    std::vector<GLSLExprPtr> args;
  };

  struct GLSLAssignExpr {
    GLSLExprPtr lhs;
    GLSLExprPtr rhs;
    TokenKind op;
  };

  using GLSLExprData =
    std::variant<GLSLLiteralExpr, GLSLIdentifierExpr, GLSLBinaryExpr,
    GLSLUnaryExpr, GLSLTernaryExpr, GLSLCallExpr, GLSLIndexExpr,
    GLSLFieldExpr, GLSLConstructExpr, GLSLAssignExpr>;

  struct GLSLExpr {
    SourceLocation location{};
    TypeRef type{};
    GLSLExprData data;
  };

  struct GLSLBlockStmt {
    std::vector<GLSLStmtPtr> stmts;
  };

  struct GLSLIfStmt {
    GLSLExprPtr cond;
    GLSLStmtPtr then_br;
    GLSLStmtPtr else_br;
  };

  struct GLSLForStmt {
    GLSLStmtPtr init;
    GLSLExprPtr cond;
    GLSLExprPtr step;
    GLSLStmtPtr body;
  };

  struct GLSLWhileStmt {
    GLSLExprPtr cond;
    GLSLStmtPtr body;
  };

  struct GLSLReturnStmt {
    GLSLExprPtr value;
  };

  struct GLSLExprStmt {
    GLSLExprPtr expr;
  };

  struct GLSLVarDeclStmt {
    TypeRef type{};
    std::string name;
    GLSLExprPtr init;
    bool is_const = false;
  };

  struct GLSLOutputAssignStmt {
    GLSLExprPtr lhs;
    GLSLExprPtr rhs;
    TokenKind op = TokenKind::Eq;
  };

  struct GLSLBreakStmt {};
  struct GLSLContinueStmt {};
  struct GLSLDiscardStmt {};

  using GLSLStmtData =
    std::variant<GLSLBlockStmt, GLSLIfStmt, GLSLForStmt, GLSLWhileStmt,
    GLSLReturnStmt, GLSLExprStmt, GLSLVarDeclStmt,
    GLSLOutputAssignStmt, GLSLBreakStmt, GLSLContinueStmt,
    GLSLDiscardStmt>;

  struct GLSLStmt {
    SourceLocation location{};
    GLSLStmtData data;
  };

  struct GLSLFieldDecl {
    SourceLocation location{};
    TypeRef type{};
    std::string name;
    std::optional<uint32_t> array_size;
    GLSLExprPtr init;
    Annotations annotations;
  };

  struct GLSLParamDecl {
    SourceLocation location{};
    TypeRef type{};
    std::string name;
    ParamQualifier qual = ParamQualifier::None;
  };

  struct GLSLStructDecl {
    SourceLocation location{};
    std::string name;
    std::vector<GLSLFieldDecl> fields;
  };

  struct GLSLGlobalVarDecl {
    SourceLocation location{};
    TypeRef type{};
    std::string name;
    std::optional<uint32_t> array_size;
    GLSLExprPtr init;
    Annotations annotations;
    bool is_const = false;
    std::string storage;
  };

  struct GLSLInterfaceBlockDecl {
    SourceLocation location{};
    std::string storage;
    std::string block_name;
    std::vector<GLSLFieldDecl> fields;
    std::optional<std::string> instance_name;
    Annotations annotations;
  };

  struct GLSLFunctionDecl {
    SourceLocation location{};
    TypeRef ret{};
    std::string name;
    std::vector<GLSLParamDecl> params;
    GLSLStmtPtr body;
    bool prototype_only = false;
  };

  using GLSLDecl = std::variant<GLSLStructDecl, GLSLGlobalVarDecl,
    GLSLInterfaceBlockDecl, GLSLFunctionDecl>;

  struct GLSLStage {
    int version = 450;
    std::vector<GLSLDecl> declarations;
  };

  struct GlslLoweringResult {
    GLSLStage stage;
    StageReflection reflection;
    std::vector<std::string> errors;

    bool ok() const { return errors.empty(); }
  };

  class GLSLLowering {
  public:
    GlslLoweringResult lower(const CanonicalStage& stage,
      const StageReflection& reflection) const;
  };

} // namespace astralix
