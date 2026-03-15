#pragma once

#include "shader-lang/ast.hpp"
#include "shader-lang/linker.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace astralix {

struct CanonicalExpr;
struct CanonicalStmt;

using CanonicalExprPtr = std::unique_ptr<CanonicalExpr>;
using CanonicalStmtPtr = std::unique_ptr<CanonicalStmt>;

struct CanonicalLiteralExpr {
  std::variant<bool, int64_t, double> value;
};

struct CanonicalIdentifierExpr {
  std::string name;
};

struct CanonicalStageInputFieldRef {
  std::string param_name;
  std::string field;
};

struct CanonicalStageResourceFieldRef {
  std::string param_name;
  std::string field;
};

struct CanonicalOutputFieldRef {
  std::string field;
};

struct CanonicalBinaryExpr {
  CanonicalExprPtr lhs;
  CanonicalExprPtr rhs;
  TokenKind op;
};

struct CanonicalUnaryExpr {
  CanonicalExprPtr operand;
  TokenKind op;
  bool prefix = true;
};

struct CanonicalTernaryExpr {
  CanonicalExprPtr cond;
  CanonicalExprPtr then_expr;
  CanonicalExprPtr else_expr;
};

struct CanonicalCallExpr {
  CanonicalExprPtr callee;
  std::vector<CanonicalExprPtr> args;
};

struct CanonicalIndexExpr {
  CanonicalExprPtr array;
  CanonicalExprPtr index;
};

struct CanonicalFieldExpr {
  CanonicalExprPtr object;
  std::string field;
};

struct CanonicalConstructExpr {
  TypeRef type;
  std::vector<CanonicalExprPtr> args;
};

struct CanonicalAssignExpr {
  CanonicalExprPtr lhs;
  CanonicalExprPtr rhs;
  TokenKind op;
};

using CanonicalExprData =
    std::variant<CanonicalLiteralExpr, CanonicalIdentifierExpr,
                 CanonicalStageInputFieldRef, CanonicalStageResourceFieldRef,
                 CanonicalOutputFieldRef, CanonicalBinaryExpr,
                 CanonicalUnaryExpr, CanonicalTernaryExpr, CanonicalCallExpr,
                 CanonicalIndexExpr, CanonicalFieldExpr, CanonicalConstructExpr,
                 CanonicalAssignExpr>;

struct CanonicalExpr {
  SourceLocation location{};
  TypeRef type{};
  CanonicalExprData data;
};

struct CanonicalBlockStmt {
  std::vector<CanonicalStmtPtr> stmts;
};

struct CanonicalIfStmt {
  CanonicalExprPtr cond;
  CanonicalStmtPtr then_br;
  CanonicalStmtPtr else_br;
};

struct CanonicalForStmt {
  CanonicalStmtPtr init;
  CanonicalExprPtr cond;
  CanonicalExprPtr step;
  CanonicalStmtPtr body;
};

struct CanonicalWhileStmt {
  CanonicalExprPtr cond;
  CanonicalStmtPtr body;
};

struct CanonicalReturnStmt {
  CanonicalExprPtr value;
};

struct CanonicalExprStmt {
  CanonicalExprPtr expr;
};

struct CanonicalVarDeclStmt {
  TypeRef type;
  std::string name;
  CanonicalExprPtr init;
  bool is_const = false;
};

struct CanonicalOutputAssignStmt {
  std::string field;
  CanonicalExprPtr value;
  TokenKind op = TokenKind::Eq;
};

struct CanonicalBreakStmt {};
struct CanonicalContinueStmt {};
struct CanonicalDiscardStmt {};

using CanonicalStmtData =
    std::variant<CanonicalBlockStmt, CanonicalIfStmt, CanonicalForStmt,
                 CanonicalWhileStmt, CanonicalReturnStmt, CanonicalExprStmt,
                 CanonicalVarDeclStmt, CanonicalOutputAssignStmt,
                 CanonicalBreakStmt, CanonicalContinueStmt,
                 CanonicalDiscardStmt>;

struct CanonicalStmt {
  SourceLocation location{};
  CanonicalStmtData data;
};

struct CanonicalFieldDecl {
  SourceLocation location{};
  TypeRef type{};
  std::string name;
  std::optional<uint32_t> array_size;
  CanonicalExprPtr init;
  Annotations annotations;
};

struct CanonicalParamDecl {
  SourceLocation location{};
  TypeRef type{};
  std::string name;
  ParamQualifier qual = ParamQualifier::None;
};

struct CanonicalStructDecl {
  SourceLocation location{};
  std::string name;
  std::vector<CanonicalFieldDecl> fields;
};

struct CanonicalGlobalConstDecl {
  SourceLocation location{};
  TypeRef type{};
  std::string name;
  CanonicalExprPtr init;
  bool is_const = true;
};

struct CanonicalUniformDecl {
  SourceLocation location{};
  TypeRef type{};
  std::string name;
  Annotations annotations;
  CanonicalExprPtr default_val;
  std::optional<uint32_t> array_size;
};

struct CanonicalBufferDecl {
  SourceLocation location{};
  std::string name;
  std::vector<CanonicalFieldDecl> fields;
  Annotations annotations;
  std::optional<std::string> instance_name;
  bool is_uniform = false;
};

struct CanonicalInterfaceBlockDecl {
  SourceLocation location{};
  bool is_in = false;
  bool is_storage_block = false;
  std::string name;
  std::vector<CanonicalFieldDecl> fields;
  std::optional<std::string> instance_name;
  Annotations annotations;
};

struct CanonicalFunctionDecl {
  SourceLocation location{};
  TypeRef ret{};
  std::string name;
  std::vector<CanonicalParamDecl> params;
  CanonicalStmtPtr body;
};

using CanonicalDecl =
    std::variant<CanonicalGlobalConstDecl, CanonicalUniformDecl,
                 CanonicalBufferDecl, CanonicalInterfaceBlockDecl,
                 CanonicalFunctionDecl>;

struct CanonicalInterfaceBinding {
  SourceLocation location{};
  std::string param_name;
  std::string interface_name;
  std::vector<CanonicalFieldDecl> fields;
  InterfaceRole role = InterfaceRole::None;
};

struct CanonicalStageOutput {
  SourceLocation location{};
  std::string interface_name;
  std::vector<CanonicalFieldDecl> fields;
  std::optional<std::string> sink_name;
};

struct CanonicalEntryPoint {
  SourceLocation location{};
  StageKind stage = StageKind::Vertex;
  TypeRef ret{};
  std::string name = "main";
  std::vector<CanonicalInterfaceBinding> varying_inputs;
  std::vector<CanonicalInterfaceBinding> resource_inputs;
  std::optional<CanonicalStageOutput> output;
  CanonicalStmtPtr body;
};

struct CanonicalStage {
  int version = 450;
  StageKind stage = StageKind::Vertex;
  std::vector<CanonicalStructDecl> structs;
  std::vector<CanonicalDecl> declarations;
  CanonicalEntryPoint entry;
};

struct CanonicalLoweringResult {
  CanonicalStage stage;
  std::vector<std::string> errors;

  bool ok() const { return errors.empty(); }
};

class CanonicalLowering {
public:
  explicit CanonicalLowering(const std::vector<ASTNode> &nodes);

  CanonicalLoweringResult lower(const Program &program,
                                const LinkResult &link_result,
                                StageKind stage) const;

private:
  struct EntryContext;

  const std::vector<ASTNode> &m_nodes;
};

} // namespace astralix
