#include "shader-lang/lowering/vulkan-glsl-coordinate-pass.hpp"

#include <string>
#include <type_traits>
#include <variant>

namespace astralix {
namespace {

GLSLExprPtr make_gl_position_expr() {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeVec4, "vec4"};
  expr->data = GLSLIdentifierExpr{"gl_Position"};
  return expr;
}

GLSLExprPtr make_gl_position_z_expr() {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeFloat, "float"};
  expr->data = GLSLFieldExpr{make_gl_position_expr(), "z"};
  return expr;
}

GLSLExprPtr make_gl_position_w_expr() {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeFloat, "float"};
  expr->data = GLSLFieldExpr{make_gl_position_expr(), "w"};
  return expr;
}

GLSLExprPtr make_float_literal_expr(double value) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeFloat, "float"};
  expr->data = GLSLLiteralExpr{value};
  return expr;
}

GLSLStmtPtr make_gl_position_z_remap_stmt() {
  auto sum = std::make_unique<GLSLExpr>();
  sum->type = TypeRef{TokenKind::TypeFloat, "float"};
  sum->data = GLSLBinaryExpr{
      make_gl_position_z_expr(), make_gl_position_w_expr(), TokenKind::Plus
  };

  auto rhs = std::make_unique<GLSLExpr>();
  rhs->type = TypeRef{TokenKind::TypeFloat, "float"};
  rhs->data = GLSLBinaryExpr{
      std::move(sum), make_float_literal_expr(0.5), TokenKind::Star
  };

  auto assign = std::make_unique<GLSLExpr>();
  assign->type = TypeRef{TokenKind::TypeFloat, "float"};
  assign->data = GLSLAssignExpr{
      make_gl_position_z_expr(), std::move(rhs), TokenKind::Eq
  };

  auto stmt = std::make_unique<GLSLStmt>();
  stmt->data = GLSLExprStmt{std::move(assign)};
  return stmt;
}

std::vector<GLSLStmtPtr> make_vulkan_clip_fix_stmts() {
  std::vector<GLSLStmtPtr> statements;
  statements.push_back(make_gl_position_z_remap_stmt());
  return statements;
}

bool is_float_literal_value(const GLSLExpr &expr, double value) {
  const auto *literal = std::get_if<GLSLLiteralExpr>(&expr.data);
  if (!literal) {
    return false;
  }

  if (const auto *float_value = std::get_if<double>(&literal->value)) {
    return *float_value == value;
  }
  if (const auto *int_value = std::get_if<int64_t>(&literal->value)) {
    return static_cast<double>(*int_value) == value;
  }
  return false;
}

bool is_identifier_field_expr(
    const GLSLExpr &expr, std::string_view field,
    std::string *identifier_name = nullptr
) {
  const auto *field_expr = std::get_if<GLSLFieldExpr>(&expr.data);
  if (!field_expr || !field_expr->object || field_expr->field != field) {
    return false;
  }

  const auto *identifier =
      std::get_if<GLSLIdentifierExpr>(&field_expr->object->data);
  if (!identifier) {
    return false;
  }

  if (identifier_name != nullptr) {
    *identifier_name = identifier->name;
  }
  return true;
}

bool is_identifier_expr(
    const GLSLExpr &expr, std::string_view name,
    std::string *identifier_name = nullptr
) {
  const auto *identifier = std::get_if<GLSLIdentifierExpr>(&expr.data);
  if (!identifier || identifier->name != name) {
    return false;
  }

  if (identifier_name != nullptr) {
    *identifier_name = identifier->name;
  }
  return true;
}

bool is_half_vec2_expr(const GLSLExpr &expr) {
  const auto *construct = std::get_if<GLSLConstructExpr>(&expr.data);
  if (!construct || construct->type.kind != TokenKind::TypeVec2) {
    return false;
  }

  if (construct->args.size() == 1) {
    return construct->args[0] &&
           is_float_literal_value(*construct->args[0], 0.5);
  }

  if (construct->args.size() == 2) {
    return construct->args[0] && construct->args[1] &&
           is_float_literal_value(*construct->args[0], 0.5) &&
           is_float_literal_value(*construct->args[1], 0.5);
  }

  return false;
}

bool is_clip_xy_over_w_expr(const GLSLExpr &expr) {
  const auto *binary = std::get_if<GLSLBinaryExpr>(&expr.data);
  if (!binary || binary->op != TokenKind::Slash || !binary->lhs ||
      !binary->rhs) {
    return false;
  }

  std::string xy_identifier;
  std::string w_identifier;
  return is_identifier_field_expr(*binary->lhs, "xy", &xy_identifier) &&
         is_identifier_field_expr(*binary->rhs, "w", &w_identifier) &&
         xy_identifier == w_identifier;
}

bool is_half_scaled_clip_xy_over_w_expr(const GLSLExpr &expr) {
  const auto *binary = std::get_if<GLSLBinaryExpr>(&expr.data);
  if (!binary || binary->op != TokenKind::Star || !binary->lhs ||
      !binary->rhs) {
    return false;
  }

  return (is_clip_xy_over_w_expr(*binary->lhs) &&
          is_float_literal_value(*binary->rhs, 0.5)) ||
         (is_float_literal_value(*binary->lhs, 0.5) &&
          is_clip_xy_over_w_expr(*binary->rhs));
}

bool is_ndc_to_screen_uv_expr(const GLSLExpr &expr) {
  const auto *binary = std::get_if<GLSLBinaryExpr>(&expr.data);
  if (!binary || binary->op != TokenKind::Plus || !binary->lhs ||
      !binary->rhs) {
    return false;
  }

  return (is_half_scaled_clip_xy_over_w_expr(*binary->lhs) &&
          is_half_vec2_expr(*binary->rhs)) ||
         (is_half_vec2_expr(*binary->lhs) &&
          is_half_scaled_clip_xy_over_w_expr(*binary->rhs));
}

bool is_identifier_scaled_by_half_expr(
    const GLSLExpr &expr, std::string *identifier_name = nullptr
) {
  const auto *binary = std::get_if<GLSLBinaryExpr>(&expr.data);
  if (!binary || binary->op != TokenKind::Star || !binary->lhs ||
      !binary->rhs) {
    return false;
  }

  std::string lhs_identifier;
  std::string rhs_identifier;
  return (is_identifier_expr(
              *binary->lhs, "projection_coordinates", &lhs_identifier
          ) &&
          is_float_literal_value(*binary->rhs, 0.5) &&
          (identifier_name == nullptr ||
           (*identifier_name = lhs_identifier, true))) ||
         (is_float_literal_value(*binary->lhs, 0.5) &&
          is_identifier_expr(
              *binary->rhs, "projection_coordinates", &rhs_identifier
          ) &&
          (identifier_name == nullptr ||
           (*identifier_name = rhs_identifier, true)));
}

bool is_projection_coordinate_remap_expr(
    const GLSLExpr &expr, std::string *identifier_name = nullptr
) {
  const auto *binary = std::get_if<GLSLBinaryExpr>(&expr.data);
  if (!binary || binary->op != TokenKind::Plus || !binary->lhs ||
      !binary->rhs) {
    return false;
  }

  std::string scaled_identifier;
  return (is_identifier_scaled_by_half_expr(*binary->lhs, &scaled_identifier) &&
          is_float_literal_value(*binary->rhs, 0.5) &&
          (identifier_name == nullptr ||
           (*identifier_name = scaled_identifier, true))) ||
         (is_float_literal_value(*binary->lhs, 0.5) &&
          is_identifier_scaled_by_half_expr(*binary->rhs, &scaled_identifier) &&
          (identifier_name == nullptr ||
           (*identifier_name = scaled_identifier, true)));
}

GLSLExprPtr make_identifier_expr(
    std::string name, TypeRef type = TypeRef{TokenKind::Identifier, ""}
) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = std::move(type);
  expr->data = GLSLIdentifierExpr{std::move(name)};
  return expr;
}

GLSLExprPtr make_field_expr(
    GLSLExprPtr object, std::string field, TypeRef type
) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = std::move(type);
  expr->data = GLSLFieldExpr{std::move(object), std::move(field)};
  return expr;
}

GLSLExprPtr make_vulkan_screen_uv_call(GLSLExprPtr uv_expr) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeVec2, "vec2"};

  GLSLCallExpr call;
  call.callee = make_identifier_expr("__astralix_vulkan_screen_uv");
  call.args.push_back(std::move(uv_expr));

  expr->data = std::move(call);
  return expr;
}

GLSLStmtPtr make_vulkan_shadow_projection_y_flip_stmt(
    std::string identifier_name
) {
  auto lhs = make_field_expr(
      make_identifier_expr(
          identifier_name, TypeRef{TokenKind::TypeVec3, "vec3"}
      ),
      "y",
      TypeRef{TokenKind::TypeFloat, "float"}
  );

  auto rhs = std::make_unique<GLSLExpr>();
  rhs->type = TypeRef{TokenKind::TypeFloat, "float"};
  rhs->data = GLSLBinaryExpr{
      make_float_literal_expr(1.0),
      make_field_expr(
          make_identifier_expr(
              identifier_name, TypeRef{TokenKind::TypeVec3, "vec3"}
          ),
          "y",
          TypeRef{TokenKind::TypeFloat, "float"}
      ),
      TokenKind::Minus
  };

  auto assign = std::make_unique<GLSLExpr>();
  assign->type = TypeRef{TokenKind::TypeFloat, "float"};
  assign->data = GLSLAssignExpr{std::move(lhs), std::move(rhs), TokenKind::Eq};

  auto stmt = std::make_unique<GLSLStmt>();
  stmt->data = GLSLExprStmt{std::move(assign)};
  return stmt;
}

bool rewrite_vulkan_screen_uv_expr(GLSLExprPtr &expr);

bool rewrite_vulkan_screen_uv_stmt(GLSLStmt &stmt) {
  bool rewritten = false;

  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBlockStmt>) {
          for (auto &child : data.stmts) {
            if (child) {
              rewritten |= rewrite_vulkan_screen_uv_stmt(*child);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLIfStmt>) {
          if (data.cond) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.cond);
          }
          if (data.then_br) {
            rewritten |= rewrite_vulkan_screen_uv_stmt(*data.then_br);
          }
          if (data.else_br) {
            rewritten |= rewrite_vulkan_screen_uv_stmt(*data.else_br);
          }
        } else if constexpr (std::is_same_v<T, GLSLForStmt>) {
          if (data.init) {
            rewritten |= rewrite_vulkan_screen_uv_stmt(*data.init);
          }
          if (data.cond) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.cond);
          }
          if (data.step) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.step);
          }
          if (data.body) {
            rewritten |= rewrite_vulkan_screen_uv_stmt(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLWhileStmt>) {
          if (data.cond) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.cond);
          }
          if (data.body) {
            rewritten |= rewrite_vulkan_screen_uv_stmt(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLReturnStmt>) {
          if (data.value) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.value);
          }
        } else if constexpr (std::is_same_v<T, GLSLExprStmt>) {
          if (data.expr) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.expr);
          }
        } else if constexpr (std::is_same_v<T, GLSLVarDeclStmt>) {
          if (data.init) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.init);
          }
        } else if constexpr (std::is_same_v<T, GLSLOutputAssignStmt>) {
          if (data.lhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.lhs);
          }
          if (data.rhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.rhs);
          }
        }
      },
      stmt.data
  );

  return rewritten;
}

bool rewrite_vulkan_screen_uv_expr(GLSLExprPtr &expr) {
  if (!expr) {
    return false;
  }

  bool rewritten = false;

  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBinaryExpr>) {
          if (data.lhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.lhs);
          }
          if (data.rhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.rhs);
          }
        } else if constexpr (std::is_same_v<T, GLSLUnaryExpr>) {
          if (data.operand) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.operand);
          }
        } else if constexpr (std::is_same_v<T, GLSLTernaryExpr>) {
          if (data.cond) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.cond);
          }
          if (data.then_expr) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.then_expr);
          }
          if (data.else_expr) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.else_expr);
          }
        } else if constexpr (std::is_same_v<T, GLSLCallExpr>) {
          if (data.callee) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.callee);
          }
          for (auto &arg : data.args) {
            if (arg) {
              rewritten |= rewrite_vulkan_screen_uv_expr(arg);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLIndexExpr>) {
          if (data.array) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.array);
          }
          if (data.index) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.index);
          }
        } else if constexpr (std::is_same_v<T, GLSLFieldExpr>) {
          if (data.object) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.object);
          }
        } else if constexpr (std::is_same_v<T, GLSLConstructExpr>) {
          for (auto &arg : data.args) {
            if (arg) {
              rewritten |= rewrite_vulkan_screen_uv_expr(arg);
            }
          }
        } else if constexpr (std::is_same_v<T, GLSLAssignExpr>) {
          if (data.lhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.lhs);
          }
          if (data.rhs) {
            rewritten |= rewrite_vulkan_screen_uv_expr(data.rhs);
          }
        }
      },
      expr->data
  );

  if (is_ndc_to_screen_uv_expr(*expr)) {
    expr = make_vulkan_screen_uv_call(std::move(expr));
    return true;
  }

  return rewritten;
}

GLSLFunctionDecl make_vulkan_screen_uv_helper_decl() {
  GLSLFunctionDecl function;
  function.ret = TypeRef{TokenKind::TypeVec2, "vec2"};
  function.name = "__astralix_vulkan_screen_uv";
  function.params.push_back(GLSLParamDecl{
      .type = TypeRef{TokenKind::TypeVec2, "vec2"},
      .name = "uv",
  });

  auto y_flip = std::make_unique<GLSLExpr>();
  y_flip->type = TypeRef{TokenKind::TypeFloat, "float"};
  y_flip->data = GLSLBinaryExpr{
      make_float_literal_expr(1.0),
      make_field_expr(
          make_identifier_expr("uv", TypeRef{TokenKind::TypeVec2, "vec2"}),
          "y",
          TypeRef{TokenKind::TypeFloat, "float"}
      ),
      TokenKind::Minus
  };

  auto return_value = std::make_unique<GLSLExpr>();
  return_value->type = TypeRef{TokenKind::TypeVec2, "vec2"};
  GLSLConstructExpr construct;
  construct.type = TypeRef{TokenKind::TypeVec2, "vec2"};
  construct.args.push_back(make_field_expr(
      make_identifier_expr("uv", TypeRef{TokenKind::TypeVec2, "vec2"}),
      "x",
      TypeRef{TokenKind::TypeFloat, "float"}
  ));
  construct.args.push_back(std::move(y_flip));
  return_value->data = std::move(construct);

  auto return_stmt = std::make_unique<GLSLStmt>();
  return_stmt->data = GLSLReturnStmt{std::move(return_value)};

  auto body = std::make_unique<GLSLStmt>();
  GLSLBlockStmt block;
  block.stmts.push_back(std::move(return_stmt));
  body->data = std::move(block);
  function.body = std::move(body);
  return function;
}

bool has_vulkan_screen_uv_helper(const GLSLStage &stage) {
  for (const auto &declaration : stage.declarations) {
    const auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function && function->name == "__astralix_vulkan_screen_uv") {
      return true;
    }
  }
  return false;
}

void inject_vulkan_fragment_screen_uv_fix(GLSLStage &stage) {
  if (has_vulkan_screen_uv_helper(stage)) {
    return;
  }

  bool rewrote_uv_conversion = false;
  for (auto &declaration : stage.declarations) {
    auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function && function->body) {
      rewrote_uv_conversion |= rewrite_vulkan_screen_uv_stmt(*function->body);
    }
  }

  if (!rewrote_uv_conversion) {
    return;
  }

  size_t insert_index = stage.declarations.size();
  for (size_t i = 0; i < stage.declarations.size(); ++i) {
    if (std::holds_alternative<GLSLFunctionDecl>(stage.declarations[i])) {
      insert_index = i;
      break;
    }
  }

  stage.declarations.insert(
      stage.declarations.begin() + static_cast<std::ptrdiff_t>(insert_index),
      GLSLDecl{make_vulkan_screen_uv_helper_decl()}
  );
}

void ensure_block_body(GLSLFunctionDecl &function);

bool is_projection_coordinate_remap_stmt(
    const GLSLStmt &stmt, std::string *identifier_name = nullptr
) {
  const auto *expr_stmt = std::get_if<GLSLExprStmt>(&stmt.data);
  if (!expr_stmt || !expr_stmt->expr) {
    return false;
  }

  const auto *assign = std::get_if<GLSLAssignExpr>(&expr_stmt->expr->data);
  if (!assign || assign->op != TokenKind::Eq || !assign->lhs || !assign->rhs) {
    return false;
  }

  std::string lhs_identifier;
  std::string rhs_identifier;
  return is_identifier_expr(*assign->lhs, "projection_coordinates", &lhs_identifier) &&
         is_projection_coordinate_remap_expr(*assign->rhs, &rhs_identifier) &&
         lhs_identifier == rhs_identifier &&
         (identifier_name == nullptr || (*identifier_name = lhs_identifier, true));
}

void inject_vulkan_shadow_projection_y_flip(GLSLStmt &stmt) {
  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBlockStmt>) {
          std::vector<GLSLStmtPtr> rewritten;
          rewritten.reserve(data.stmts.size() * 2);
          for (auto &child : data.stmts) {
            if (!child) {
              continue;
            }

            inject_vulkan_shadow_projection_y_flip(*child);
            rewritten.push_back(std::move(child));

            std::string identifier_name;
            if (is_projection_coordinate_remap_stmt(
                    *rewritten.back(), &identifier_name
                )) {
              rewritten.push_back(
                  make_vulkan_shadow_projection_y_flip_stmt(identifier_name)
              );
            }
          }
          data.stmts = std::move(rewritten);
        } else if constexpr (std::is_same_v<T, GLSLIfStmt>) {
          if (data.then_br) {
            inject_vulkan_shadow_projection_y_flip(*data.then_br);
          }
          if (data.else_br) {
            inject_vulkan_shadow_projection_y_flip(*data.else_br);
          }
        } else if constexpr (std::is_same_v<T, GLSLForStmt>) {
          if (data.init) {
            inject_vulkan_shadow_projection_y_flip(*data.init);
          }
          if (data.body) {
            inject_vulkan_shadow_projection_y_flip(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLWhileStmt>) {
          if (data.body) {
            inject_vulkan_shadow_projection_y_flip(*data.body);
          }
        }
      },
      stmt.data
  );
}

void inject_vulkan_shadow_sampling_fix(GLSLStage &stage) {
  for (auto &declaration : stage.declarations) {
    auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function == nullptr || function->name != "get_shadow" ||
        !function->body) {
      continue;
    }

    ensure_block_body(*function);
    inject_vulkan_shadow_projection_y_flip(*function->body);
  }
}

bool is_return_stmt(const GLSLStmt &stmt) {
  return std::holds_alternative<GLSLReturnStmt>(stmt.data);
}

void inject_flip_before_returns(GLSLStmt &stmt) {
  std::visit(
      [&](auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, GLSLBlockStmt>) {
          std::vector<GLSLStmtPtr> rewritten;
          rewritten.reserve(data.stmts.size() * 2);
          for (auto &child : data.stmts) {
            if (!child) {
              continue;
            }

            inject_flip_before_returns(*child);
            if (is_return_stmt(*child)) {
              auto fix_stmts = make_vulkan_clip_fix_stmts();
              for (auto &fix_stmt : fix_stmts) {
                rewritten.push_back(std::move(fix_stmt));
              }
            }
            rewritten.push_back(std::move(child));
          }
          data.stmts = std::move(rewritten);
        } else if constexpr (std::is_same_v<T, GLSLIfStmt>) {
          if (data.then_br) {
            inject_flip_before_returns(*data.then_br);
          }
          if (data.else_br) {
            inject_flip_before_returns(*data.else_br);
          }
        } else if constexpr (std::is_same_v<T, GLSLForStmt>) {
          if (data.init) {
            inject_flip_before_returns(*data.init);
          }
          if (data.body) {
            inject_flip_before_returns(*data.body);
          }
        } else if constexpr (std::is_same_v<T, GLSLWhileStmt>) {
          if (data.body) {
            inject_flip_before_returns(*data.body);
          }
        }
      },
      stmt.data
  );
}

void ensure_block_body(GLSLFunctionDecl &function) {
  if (!function.body ||
      std::holds_alternative<GLSLBlockStmt>(function.body->data)) {
    return;
  }

  std::vector<GLSLStmtPtr> stmts;
  stmts.push_back(std::move(function.body));

  auto body = std::make_unique<GLSLStmt>();
  body->data = GLSLBlockStmt{std::move(stmts)};
  function.body = std::move(body);
}

} // namespace

void normalize_vulkan_fragment_coordinates(GLSLStage &stage) {
  inject_vulkan_fragment_screen_uv_fix(stage);
  inject_vulkan_shadow_sampling_fix(stage);
}

void normalize_vulkan_vertex_clip_space(GLSLStage &stage) {
  for (auto &declaration : stage.declarations) {
    auto *function = std::get_if<GLSLFunctionDecl>(&declaration);
    if (function == nullptr || !function->body || function->name != "main") {
      continue;
    }

    ensure_block_body(*function);
    inject_flip_before_returns(*function->body);

    auto *block = std::get_if<GLSLBlockStmt>(&function->body->data);
    if (!block || (!block->stmts.empty() && is_return_stmt(*block->stmts.back()))) {
      continue;
    }

    auto fix_stmts = make_vulkan_clip_fix_stmts();
    for (auto &fix_stmt : fix_stmts) {
      block->stmts.push_back(std::move(fix_stmt));
    }
  }
}

} // namespace astralix
