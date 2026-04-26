#include "shader-lang/lowering/glsl-ast-rewrite.hpp"

#include <memory>
#include <type_traits>
#include <variant>

namespace astralix {
namespace {

void remap_struct_field_access_expr(
    GLSLExpr &expr,
    const std::unordered_map<std::string, std::string> &field_rewrites
);

void remap_struct_field_in_expr(
    GLSLExpr &expr,
    const std::unordered_map<std::string, std::string> &field_rewrites
) {
  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBinaryExpr>) {
          if (data.lhs) {
            remap_struct_field_access_expr(*data.lhs, field_rewrites);
          }
          if (data.rhs) {
            remap_struct_field_access_expr(*data.rhs, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLUnaryExpr>) {
          if (data.operand) {
            remap_struct_field_access_expr(*data.operand, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLTernaryExpr>) {
          if (data.cond) {
            remap_struct_field_access_expr(*data.cond, field_rewrites);
          }
          if (data.then_expr) {
            remap_struct_field_access_expr(*data.then_expr, field_rewrites);
          }
          if (data.else_expr) {
            remap_struct_field_access_expr(*data.else_expr, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLCallExpr>) {
          if (data.callee) {
            remap_struct_field_access_expr(*data.callee, field_rewrites);
          }
          for (auto &arg : data.args) {
            if (arg) {
              remap_struct_field_access_expr(*arg, field_rewrites);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLIndexExpr>) {
          if (data.array) {
            remap_struct_field_access_expr(*data.array, field_rewrites);
          }
          if (data.index) {
            remap_struct_field_access_expr(*data.index, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLFieldExpr>) {
          if (data.object) {
            remap_struct_field_access_expr(*data.object, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLConstructExpr>) {
          for (auto &arg : data.args) {
            if (arg) {
              remap_struct_field_access_expr(*arg, field_rewrites);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLAssignExpr>) {
          if (data.lhs) {
            remap_struct_field_access_expr(*data.lhs, field_rewrites);
          }
          if (data.rhs) {
            remap_struct_field_access_expr(*data.rhs, field_rewrites);
          }
        }
      },
      expr.data
  );
}

void remap_struct_field_access_expr(
    GLSLExpr &expr,
    const std::unordered_map<std::string, std::string> &field_rewrites
) {
  auto *field_expr = std::get_if<GLSLFieldExpr>(&expr.data);
  if (field_expr && field_expr->object) {
    auto *index_expr = std::get_if<GLSLIndexExpr>(&field_expr->object->data);
    if (index_expr && index_expr->array) {
      auto *ident_expr =
          std::get_if<GLSLIdentifierExpr>(&index_expr->array->data);
      if (ident_expr) {
        std::string key = ident_expr->name + "." + field_expr->field;
        auto rewrite_it = field_rewrites.find(key);
        if (rewrite_it != field_rewrites.end()) {
          GLSLExpr rewritten_array;
          rewritten_array.location = index_expr->array->location;
          rewritten_array.type = index_expr->array->type;
          rewritten_array.data = GLSLIdentifierExpr{rewrite_it->second};

          GLSLIndexExpr rewritten_index;
          rewritten_index.array =
              std::make_unique<GLSLExpr>(std::move(rewritten_array));
          rewritten_index.index = std::move(index_expr->index);
          expr.data = std::move(rewritten_index);
          return;
        }
      }
    }

    auto *ident_expr =
        std::get_if<GLSLIdentifierExpr>(&field_expr->object->data);
    if (ident_expr) {
      std::string key = ident_expr->name + "." + field_expr->field;
      auto rewrite_it = field_rewrites.find(key);
      if (rewrite_it != field_rewrites.end()) {
        expr.data = GLSLIdentifierExpr{rewrite_it->second};
        return;
      }
    }
  }

  remap_struct_field_in_expr(expr, field_rewrites);
}

} // namespace

void remap_struct_fields_in_stmt(
    GLSLStmt &stmt,
    const std::unordered_map<std::string, std::string> &field_rewrites
) {
  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBlockStmt>) {
          for (auto &child : data.stmts) {
            if (child) {
              remap_struct_fields_in_stmt(*child, field_rewrites);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLExprStmt>) {
          if (data.expr) {
            remap_struct_field_access_expr(*data.expr, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLVarDeclStmt>) {
          if (data.init) {
            remap_struct_field_access_expr(*data.init, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLReturnStmt>) {
          if (data.value) {
            remap_struct_field_access_expr(*data.value, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLIfStmt>) {
          if (data.cond) {
            remap_struct_field_access_expr(*data.cond, field_rewrites);
          }
          if (data.then_br) {
            remap_struct_fields_in_stmt(*data.then_br, field_rewrites);
          }
          if (data.else_br) {
            remap_struct_fields_in_stmt(*data.else_br, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLForStmt>) {
          if (data.init) {
            remap_struct_fields_in_stmt(*data.init, field_rewrites);
          }
          if (data.cond) {
            remap_struct_field_access_expr(*data.cond, field_rewrites);
          }
          if (data.step) {
            remap_struct_field_access_expr(*data.step, field_rewrites);
          }
          if (data.body) {
            remap_struct_fields_in_stmt(*data.body, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLWhileStmt>) {
          if (data.cond) {
            remap_struct_field_access_expr(*data.cond, field_rewrites);
          }
          if (data.body) {
            remap_struct_fields_in_stmt(*data.body, field_rewrites);
          }
        } else if constexpr (std::is_same_v<T, GLSLOutputAssignStmt>) {
          if (data.lhs) {
            remap_struct_field_access_expr(*data.lhs, field_rewrites);
          }
          if (data.rhs) {
            remap_struct_field_access_expr(*data.rhs, field_rewrites);
          }
        }
      },
      stmt.data
  );
}

void remap_struct_field_accesses(
    GLSLStage &stage,
    const std::unordered_map<std::string, std::string> &field_rewrites
) {
  if (field_rewrites.empty()) {
    return;
  }

  for (auto &declaration : stage.declarations) {
    auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function != nullptr && function->body) {
      remap_struct_fields_in_stmt(*function->body, field_rewrites);
    }
  }
}

} // namespace astralix
