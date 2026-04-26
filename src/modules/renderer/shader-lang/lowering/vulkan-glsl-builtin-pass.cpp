#include "shader-lang/lowering/vulkan-glsl-builtin-pass.hpp"

#include <type_traits>
#include <variant>

namespace astralix {
namespace {

void rename_builtins_in_expr(GLSLExpr &expr) {
  std::visit(
      [](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLIdentifierExpr>) {
          if (data.name == "gl_InstanceID") {
            data.name = "gl_InstanceIndex";
          } else if (data.name == "gl_VertexID") {
            data.name = "gl_VertexIndex";
          }
        } else if constexpr (std::is_same_v<T, GLSLBinaryExpr>) {
          if (data.lhs) {
            rename_builtins_in_expr(*data.lhs);
          }
          if (data.rhs) {
            rename_builtins_in_expr(*data.rhs);
          }
        } else if constexpr (std::is_same_v<T, GLSLUnaryExpr>) {
          if (data.operand) {
            rename_builtins_in_expr(*data.operand);
          }
        } else if constexpr (std::is_same_v<T, GLSLTernaryExpr>) {
          if (data.cond) {
            rename_builtins_in_expr(*data.cond);
          }
          if (data.then_expr) {
            rename_builtins_in_expr(*data.then_expr);
          }
          if (data.else_expr) {
            rename_builtins_in_expr(*data.else_expr);
          }
        } else if constexpr (std::is_same_v<T, GLSLCallExpr>) {
          if (data.callee) {
            rename_builtins_in_expr(*data.callee);
          }
          for (auto &arg : data.args) {
            if (arg) {
              rename_builtins_in_expr(*arg);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLIndexExpr>) {
          if (data.array) {
            rename_builtins_in_expr(*data.array);
          }
          if (data.index) {
            rename_builtins_in_expr(*data.index);
          }
        } else if constexpr (std::is_same_v<T, GLSLFieldExpr>) {
          if (data.object) {
            rename_builtins_in_expr(*data.object);
          }
        } else if constexpr (std::is_same_v<T, GLSLConstructExpr>) {
          for (auto &arg : data.args) {
            if (arg) {
              rename_builtins_in_expr(*arg);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLAssignExpr>) {
          if (data.lhs) {
            rename_builtins_in_expr(*data.lhs);
          }
          if (data.rhs) {
            rename_builtins_in_expr(*data.rhs);
          }
        }
      },
      expr.data
  );
}

void rename_builtins_in_stmt(GLSLStmt &stmt) {
  std::visit(
      [](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBlockStmt>) {
          for (auto &child : data.stmts) {
            if (child) {
              rename_builtins_in_stmt(*child);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLExprStmt>) {
          if (data.expr) {
            rename_builtins_in_expr(*data.expr);
          }
        } else if constexpr (std::is_same_v<T, GLSLVarDeclStmt>) {
          if (data.init) {
            rename_builtins_in_expr(*data.init);
          }
        } else if constexpr (std::is_same_v<T, GLSLReturnStmt>) {
          if (data.value) {
            rename_builtins_in_expr(*data.value);
          }
        } else if constexpr (std::is_same_v<T, GLSLIfStmt>) {
          if (data.cond) {
            rename_builtins_in_expr(*data.cond);
          }
          if (data.then_br) {
            rename_builtins_in_stmt(*data.then_br);
          }
          if (data.else_br) {
            rename_builtins_in_stmt(*data.else_br);
          }
        } else if constexpr (std::is_same_v<T, GLSLForStmt>) {
          if (data.init) {
            rename_builtins_in_stmt(*data.init);
          }
          if (data.cond) {
            rename_builtins_in_expr(*data.cond);
          }
          if (data.step) {
            rename_builtins_in_expr(*data.step);
          }
          if (data.body) {
            rename_builtins_in_stmt(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLWhileStmt>) {
          if (data.cond) {
            rename_builtins_in_expr(*data.cond);
          }
          if (data.body) {
            rename_builtins_in_stmt(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLOutputAssignStmt>) {
          if (data.lhs) {
            rename_builtins_in_expr(*data.lhs);
          }
          if (data.rhs) {
            rename_builtins_in_expr(*data.rhs);
          }
        }
      },
      stmt.data
  );
}

} // namespace

void rename_vulkan_builtins(GLSLStage &stage) {
  for (auto &declaration : stage.declarations) {
    auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function != nullptr && function->body) {
      rename_builtins_in_stmt(*function->body);
    }
  }
}

} // namespace astralix
