#include "shader-lang/lowering/glsl-stage-clone.hpp"

#include <memory>
#include <utility>
#include <variant>

namespace astralix {
namespace {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

GLSLExprPtr clone_expr_ptr(const GLSLExprPtr &expr);
GLSLStmtPtr clone_stmt_ptr(const GLSLStmtPtr &stmt);

GLSLExpr clone_expr(const GLSLExpr &expr) {
  GLSLExpr cloned;
  cloned.location = expr.location;
  cloned.type = expr.type;
  cloned.data = std::visit(
      Overloaded{
          [](const GLSLLiteralExpr &value) -> GLSLExprData { return value; },
          [](const GLSLIdentifierExpr &value) -> GLSLExprData { return value; },
          [](const GLSLBinaryExpr &value) -> GLSLExprData {
            return GLSLBinaryExpr{
                clone_expr_ptr(value.lhs), clone_expr_ptr(value.rhs), value.op};
          },
          [](const GLSLUnaryExpr &value) -> GLSLExprData {
            return GLSLUnaryExpr{
                clone_expr_ptr(value.operand), value.op, value.prefix};
          },
          [](const GLSLTernaryExpr &value) -> GLSLExprData {
            return GLSLTernaryExpr{
                clone_expr_ptr(value.cond),
                clone_expr_ptr(value.then_expr),
                clone_expr_ptr(value.else_expr)};
          },
          [](const GLSLCallExpr &value) -> GLSLExprData {
            GLSLCallExpr cloned_call;
            cloned_call.callee = clone_expr_ptr(value.callee);
            cloned_call.args.reserve(value.args.size());
            for (const auto &arg : value.args) {
              cloned_call.args.push_back(clone_expr_ptr(arg));
            }
            return cloned_call;
          },
          [](const GLSLIndexExpr &value) -> GLSLExprData {
            return GLSLIndexExpr{
                clone_expr_ptr(value.array), clone_expr_ptr(value.index)};
          },
          [](const GLSLFieldExpr &value) -> GLSLExprData {
            return GLSLFieldExpr{clone_expr_ptr(value.object), value.field};
          },
          [](const GLSLConstructExpr &value) -> GLSLExprData {
            GLSLConstructExpr cloned_construct;
            cloned_construct.type = value.type;
            cloned_construct.args.reserve(value.args.size());
            for (const auto &arg : value.args) {
              cloned_construct.args.push_back(clone_expr_ptr(arg));
            }
            return cloned_construct;
          },
          [](const GLSLAssignExpr &value) -> GLSLExprData {
            return GLSLAssignExpr{
                clone_expr_ptr(value.lhs), clone_expr_ptr(value.rhs), value.op};
          }},
      expr.data
  );
  return cloned;
}

GLSLExprPtr clone_expr_ptr(const GLSLExprPtr &expr) {
  if (!expr) {
    return nullptr;
  }
  return std::make_unique<GLSLExpr>(clone_expr(*expr));
}

GLSLStmt clone_stmt(const GLSLStmt &stmt) {
  GLSLStmt cloned;
  cloned.location = stmt.location;
  cloned.data = std::visit(
      Overloaded{
          [](const GLSLBlockStmt &value) -> GLSLStmtData {
            GLSLBlockStmt cloned_block;
            cloned_block.stmts.reserve(value.stmts.size());
            for (const auto &child : value.stmts) {
              cloned_block.stmts.push_back(clone_stmt_ptr(child));
            }
            return cloned_block;
          },
          [](const GLSLIfStmt &value) -> GLSLStmtData {
            return GLSLIfStmt{
                clone_expr_ptr(value.cond),
                clone_stmt_ptr(value.then_br),
                clone_stmt_ptr(value.else_br)};
          },
          [](const GLSLForStmt &value) -> GLSLStmtData {
            return GLSLForStmt{
                clone_stmt_ptr(value.init),
                clone_expr_ptr(value.cond),
                clone_expr_ptr(value.step),
                clone_stmt_ptr(value.body)};
          },
          [](const GLSLWhileStmt &value) -> GLSLStmtData {
            return GLSLWhileStmt{
                clone_expr_ptr(value.cond), clone_stmt_ptr(value.body)};
          },
          [](const GLSLReturnStmt &value) -> GLSLStmtData {
            return GLSLReturnStmt{clone_expr_ptr(value.value)};
          },
          [](const GLSLExprStmt &value) -> GLSLStmtData {
            return GLSLExprStmt{clone_expr_ptr(value.expr)};
          },
          [](const GLSLVarDeclStmt &value) -> GLSLStmtData {
            GLSLVarDeclStmt cloned_decl;
            cloned_decl.type = value.type;
            cloned_decl.name = value.name;
            cloned_decl.init = clone_expr_ptr(value.init);
            cloned_decl.is_const = value.is_const;
            return cloned_decl;
          },
          [](const GLSLOutputAssignStmt &value) -> GLSLStmtData {
            return GLSLOutputAssignStmt{
                clone_expr_ptr(value.lhs), clone_expr_ptr(value.rhs), value.op};
          },
          [](const GLSLBreakStmt &value) -> GLSLStmtData { return value; },
          [](const GLSLContinueStmt &value) -> GLSLStmtData { return value; },
          [](const GLSLDiscardStmt &value) -> GLSLStmtData { return value; }},
      stmt.data
  );
  return cloned;
}

GLSLStmtPtr clone_stmt_ptr(const GLSLStmtPtr &stmt) {
  if (!stmt) {
    return nullptr;
  }
  return std::make_unique<GLSLStmt>(clone_stmt(*stmt));
}

GLSLFieldDecl clone_field_decl(const GLSLFieldDecl &field) {
  GLSLFieldDecl cloned;
  cloned.location = field.location;
  cloned.type = field.type;
  cloned.name = field.name;
  cloned.array_size = field.array_size;
  cloned.init = clone_expr_ptr(field.init);
  cloned.annotations = field.annotations;
  return cloned;
}

GLSLParamDecl clone_param_decl(const GLSLParamDecl &param) { return param; }

GLSLDecl clone_decl(const GLSLDecl &decl) {
  return std::visit(
      Overloaded{
          [](const GLSLStructDecl &value) -> GLSLDecl {
            GLSLStructDecl cloned;
            cloned.location = value.location;
            cloned.name = value.name;
            cloned.fields.reserve(value.fields.size());
            for (const auto &field : value.fields) {
              cloned.fields.push_back(clone_field_decl(field));
            }
            return cloned;
          },
          [](const GLSLGlobalVarDecl &value) -> GLSLDecl {
            GLSLGlobalVarDecl cloned;
            cloned.location = value.location;
            cloned.type = value.type;
            cloned.name = value.name;
            cloned.array_size = value.array_size;
            cloned.init = clone_expr_ptr(value.init);
            cloned.annotations = value.annotations;
            cloned.is_const = value.is_const;
            cloned.storage = value.storage;
            return cloned;
          },
          [](const GLSLInterfaceBlockDecl &value) -> GLSLDecl {
            GLSLInterfaceBlockDecl cloned;
            cloned.location = value.location;
            cloned.storage = value.storage;
            cloned.block_name = value.block_name;
            cloned.fields.reserve(value.fields.size());
            for (const auto &field : value.fields) {
              cloned.fields.push_back(clone_field_decl(field));
            }
            cloned.instance_name = value.instance_name;
            cloned.annotations = value.annotations;
            return cloned;
          },
          [](const GLSLFunctionDecl &value) -> GLSLDecl {
            GLSLFunctionDecl cloned;
            cloned.location = value.location;
            cloned.ret = value.ret;
            cloned.name = value.name;
            cloned.params.reserve(value.params.size());
            for (const auto &param : value.params) {
              cloned.params.push_back(clone_param_decl(param));
            }
            cloned.body = clone_stmt_ptr(value.body);
            cloned.prototype_only = value.prototype_only;
            return cloned;
          }},
      decl
  );
}

} // namespace

GLSLStage clone_glsl_stage(const GLSLStage &stage) {
  GLSLStage cloned;
  cloned.version = stage.version;
  cloned.stage = stage.stage;
  cloned.local_size = stage.local_size;
  cloned.declarations.reserve(stage.declarations.size());
  for (const auto &decl : stage.declarations) {
    cloned.declarations.push_back(clone_decl(decl));
  }
  return cloned;
}

} // namespace astralix
