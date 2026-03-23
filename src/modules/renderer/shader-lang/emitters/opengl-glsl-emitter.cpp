#include "shader-lang/emitters/opengl-glsl-emitter.hpp"

#include <cstdio>

namespace astralix {

namespace {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

std::string array_suffix(const TypeRef &type_ref) {
  if (type_ref.is_runtime_sized ||
      (type_ref.array_size.has_value() && *type_ref.array_size == 0)) {
    return "[]";
  }

  if (type_ref.array_size) {
    return "[" + std::to_string(*type_ref.array_size) + "]";
  }

  return {};
}

std::string array_suffix(std::optional<uint32_t> array_size) {
  if (!array_size) {
    return {};
  }

  return *array_size == 0 ? "[]"
                          : "[" + std::to_string(*array_size) + "]";
}

} // namespace

std::string OpenGLGLSLEmitter::emit(const GLSLStage &stage) {
  m_out.clear();
  m_indent.clear();

  writeln("#version " + std::to_string(stage.version) + " core");
  writeln();

  for (const auto &decl : stage.declarations) {
    emit_decl(decl);
  }

  return m_out;
}

void OpenGLGLSLEmitter::emit_decl(const GLSLDecl &decl) {
  std::visit(
      Overloaded{
          [&](const GLSLStructDecl &value) { emit_struct(value); },
          [&](const GLSLGlobalVarDecl &value) { emit_global_var(value); },
          [&](const GLSLInterfaceBlockDecl &value) {
            emit_interface_block(value);
          },
          [&](const GLSLFunctionDecl &value) { emit_function(value); }},
      decl);
}

void OpenGLGLSLEmitter::emit_struct(const GLSLStructDecl &decl) {
  writeln("struct " + decl.name + " {");
  push_indent();
  for (const auto &field : decl.fields) {
    emit_field(field);
  }
  pop_indent();
  writeln("};");
  writeln();
}

void OpenGLGLSLEmitter::emit_global_var(const GLSLGlobalVarDecl &decl) {
  std::string layout = layout_quals(decl.annotations);
  std::string prefix = layout.empty() ? "" : layout + " ";

  write(m_indent);
  if (!prefix.empty()) {
    write(prefix);
  }
  if (!decl.storage.empty()) {
    write(decl.storage + " ");
  } else if (decl.is_const) {
    write("const ");
  }

  write(type_str(decl.type) + " " + decl.name);
  write(decl.array_size ? array_suffix(decl.array_size)
                        : array_suffix(decl.type));

  if (decl.init) {
    write(" = ");
    emit_expr(*decl.init);
  }

  write(";\n");
}

void OpenGLGLSLEmitter::emit_interface_block(
    const GLSLInterfaceBlockDecl &decl) {
  std::string layout = layout_quals(decl.annotations);
  std::string prefix = layout.empty() ? "" : layout + " ";

  writeln(prefix + decl.storage + " " + decl.block_name + " {");
  push_indent();
  for (const auto &field : decl.fields) {
    emit_field(field, false);
  }
  pop_indent();
  writeln("} " + decl.instance_name.value_or("") + ";");
  writeln();
}

void OpenGLGLSLEmitter::emit_field(const GLSLFieldDecl &decl,
                                   bool emit_initializer) {
  write(m_indent);

  std::string layout = layout_quals(decl.annotations);
  if (!layout.empty()) {
    write(layout + " ");
  }

  write(type_str(decl.type) + " " + decl.name);
  write(decl.array_size ? array_suffix(decl.array_size)
                        : array_suffix(decl.type));

  if (emit_initializer && decl.init) {
    write(" = ");
    emit_expr(*decl.init);
  }

  write(";\n");
}

void OpenGLGLSLEmitter::emit_function(const GLSLFunctionDecl &decl) {
  write(m_indent + type_str(decl.ret) + array_suffix(decl.ret) + " " +
        decl.name + "(");

  for (size_t i = 0; i < decl.params.size(); ++i) {
    if (i) {
      write(", ");
    }
    emit_param(decl.params[i]);
  }

  if (decl.prototype_only) {
    write(");\n");
    return;
  }

  write(") ");
  emit_stmt(*decl.body);
  writeln();
}

void OpenGLGLSLEmitter::emit_param(const GLSLParamDecl &decl) {
  if (decl.qual != ParamQualifier::None) {
    write(std::string(param_qual_str(decl.qual)) + " ");
  }

  write(type_str(decl.type) + " " + decl.name + array_suffix(decl.type));
}

void OpenGLGLSLEmitter::emit_stmt(const GLSLStmt &stmt) {
  std::visit(Overloaded{[&](const GLSLBlockStmt &value) {
                          write("{\n");
                          push_indent();
                          for (const auto &child : value.stmts) {
                            emit_stmt(*child);
                          }
                          pop_indent();
                          write(m_indent + "}\n");
                        },
                        [&](const GLSLIfStmt &value) {
                          write(m_indent + "if (");
                          emit_expr(*value.cond);
                          write(") ");
                          emit_body_stmt(*value.then_br);
                          if (value.else_br) {
                            write(m_indent + "else ");
                            emit_body_stmt(*value.else_br);
                          }
                        },
                        [&](const GLSLForStmt &value) {
                          write(m_indent + "for (");
                          emit_for_init(value.init.get());
                          write(" ");
                          if (value.cond) {
                            emit_expr(*value.cond);
                          }
                          write("; ");
                          if (value.step) {
                            emit_expr(*value.step);
                          }
                          write(") ");
                          emit_body_stmt(*value.body);
                        },
                        [&](const GLSLWhileStmt &value) {
                          write(m_indent + "while (");
                          emit_expr(*value.cond);
                          write(") ");
                          emit_body_stmt(*value.body);
                        },
                        [&](const GLSLReturnStmt &value) {
                          write(m_indent + "return");
                          if (value.value) {
                            write(" ");
                            emit_expr(*value.value);
                          }
                          write(";\n");
                        },
                        [&](const GLSLExprStmt &value) {
                          write(m_indent);
                          emit_expr(*value.expr);
                          write(";\n");
                        },
                        [&](const GLSLVarDeclStmt &value) {
                          write(m_indent);
                          if (value.is_const) {
                            write("const ");
                          }
                          write(type_str(value.type) + " " + value.name +
                                array_suffix(value.type));
                          if (value.init) {
                            write(" = ");
                            emit_expr(*value.init);
                          }
                          write(";\n");
                        },
                        [&](const GLSLOutputAssignStmt &value) {
                          write(m_indent);
                          emit_expr(*value.lhs);
                          write(" ");
                          write(op_str(value.op));
                          write(" ");
                          emit_expr(*value.rhs);
                          write(";\n");
                        },
                        [&](const GLSLBreakStmt &) { writeln("break;"); },
                        [&](const GLSLContinueStmt &) { writeln("continue;"); },
                        [&](const GLSLDiscardStmt &) { writeln("discard;"); }},
             stmt.data);
}

void OpenGLGLSLEmitter::emit_body_stmt(const GLSLStmt &stmt) {
  if (std::holds_alternative<GLSLBlockStmt>(stmt.data)) {
    emit_stmt(stmt);
    return;
  }

  write("{\n");
  push_indent();
  emit_stmt(stmt);
  pop_indent();
  write(m_indent + "}\n");
}

void OpenGLGLSLEmitter::emit_for_init(const GLSLStmt *stmt) {
  if (!stmt) {
    write(";");
    return;
  }

  const auto *var_decl = std::get_if<GLSLVarDeclStmt>(&stmt->data);
  if (var_decl) {
    if (var_decl->is_const) {
      write("const ");
    }
    write(type_str(var_decl->type) + " " + var_decl->name +
          array_suffix(var_decl->type));
    if (var_decl->init) {
      write(" = ");
      emit_expr(*var_decl->init);
    }
    write(";");
    return;
  }

  const auto *expr_stmt = std::get_if<GLSLExprStmt>(&stmt->data);
  if (expr_stmt) {
    emit_expr(*expr_stmt->expr);
    write(";");
    return;
  }

  write(";");
}

void OpenGLGLSLEmitter::emit_expr(const GLSLExpr &expr) {
  std::visit(
      Overloaded{[&](const GLSLLiteralExpr &value) { emit_literal(value); },
                 [&](const GLSLIdentifierExpr &value) { write(value.name); },
                 [&](const GLSLBinaryExpr &value) {
                   emit_expr_paren(*value.lhs);
                   write(" ");
                   write(op_str(value.op));
                   write(" ");
                   emit_expr_paren(*value.rhs);
                 },
                 [&](const GLSLUnaryExpr &value) {
                   if (value.prefix) {
                     write(op_str(value.op));
                     emit_expr(*value.operand);
                   } else {
                     emit_expr(*value.operand);
                     write(op_str(value.op));
                   }
                 },
                 [&](const GLSLTernaryExpr &value) {
                   emit_expr(*value.cond);
                   write(" ? ");
                   emit_expr(*value.then_expr);
                   write(" : ");
                   emit_expr(*value.else_expr);
                 },
                 [&](const GLSLCallExpr &value) {
                   emit_expr(*value.callee);
                   write("(");
                   emit_call_args(value.args);
                   write(")");
                 },
                 [&](const GLSLIndexExpr &value) {
                   emit_expr(*value.array);
                   write("[");
                   emit_expr(*value.index);
                   write("]");
                 },
                 [&](const GLSLFieldExpr &value) {
                   emit_expr(*value.object);
                   write(".");
                   write(value.field);
                 },
                 [&](const GLSLConstructExpr &value) {
                   write(type_str(value.type) + array_suffix(value.type));
                   write("(");
                   emit_call_args(value.args);
                   write(")");
                 },
                 [&](const GLSLAssignExpr &value) {
                   emit_expr(*value.lhs);
                   write(" ");
                   write(op_str(value.op));
                   write(" ");
                   emit_expr(*value.rhs);
                 }},
      expr.data);
}

void OpenGLGLSLEmitter::emit_expr_paren(const GLSLExpr &expr) {
  bool needs = std::holds_alternative<GLSLBinaryExpr>(expr.data) ||
               std::holds_alternative<GLSLTernaryExpr>(expr.data);
  if (needs) {
    write("(");
  }
  emit_expr(expr);
  if (needs) {
    write(")");
  }
}

void OpenGLGLSLEmitter::emit_call_args(const std::vector<GLSLExprPtr> &args) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) {
      write(", ");
    }
    emit_expr(*args[i]);
  }
}

void OpenGLGLSLEmitter::emit_literal(const GLSLLiteralExpr &expr) {
  std::visit(Overloaded{[&](bool value) { write(value ? "true" : "false"); },
                        [&](int64_t value) { write(std::to_string(value)); },
                        [&](double value) {
                          char buffer[32];
                          std::snprintf(buffer, sizeof(buffer), "%g",
                                        static_cast<double>(value));
                          write(buffer);
                        }},
             expr.value);
}

std::string
OpenGLGLSLEmitter::layout_quals(const Annotations &annotations) const {
  std::string parts;
  auto append = [&](std::string value) {
    if (!parts.empty()) {
      parts += ", ";
    }
    parts += std::move(value);
  };

  for (const auto &annotation : annotations) {
    switch (annotation.kind) {
      case AnnotationKind::Location:
        if (annotation.slot >= 0) {
          append("location = " + std::to_string(annotation.slot));
        }
        break;
      case AnnotationKind::Binding:
        if (annotation.slot >= 0) {
          append("binding = " + std::to_string(annotation.slot));
        }
        break;
      case AnnotationKind::Std430:
        append("std430");
        break;
      case AnnotationKind::Std140:
        append("std140");
        break;
      default:
        break;
    }
  }

  return parts.empty() ? "" : "layout(" + parts + ")";
}

std::string OpenGLGLSLEmitter::type_str(const TypeRef &type_ref) const {
  switch (type_ref.kind) {
    case TokenKind::KeywordVoid:
      return "void";
    case TokenKind::TypeBool:
      return "bool";
    case TokenKind::TypeInt:
      return "int";
    case TokenKind::TypeUint:
      return "uint";
    case TokenKind::TypeFloat:
      return "float";
    case TokenKind::TypeVec2:
      return "vec2";
    case TokenKind::TypeVec3:
      return "vec3";
    case TokenKind::TypeVec4:
      return "vec4";
    case TokenKind::TypeIvec2:
      return "ivec2";
    case TokenKind::TypeIvec3:
      return "ivec3";
    case TokenKind::TypeIvec4:
      return "ivec4";
    case TokenKind::TypeUvec2:
      return "uvec2";
    case TokenKind::TypeUvec3:
      return "uvec3";
    case TokenKind::TypeUvec4:
      return "uvec4";
    case TokenKind::TypeMat2:
      return "mat2";
    case TokenKind::TypeMat3:
      return "mat3";
    case TokenKind::TypeMat4:
      return "mat4";
    case TokenKind::TypeSampler2D:
      return "sampler2D";
    case TokenKind::TypeSamplerCube:
      return "samplerCube";
    case TokenKind::TypeSampler2DShadow:
      return "sampler2DShadow";
    case TokenKind::TypeIsampler2D:
      return "isampler2D";
    case TokenKind::TypeUsampler2D:
      return "usampler2D";
    case TokenKind::Identifier:
      return type_ref.name;
    default:
      return "<?>";
  }
}

std::string_view OpenGLGLSLEmitter::op_str(TokenKind op) {
  switch (op) {
    case TokenKind::Plus:
      return "+";
    case TokenKind::Minus:
      return "-";
    case TokenKind::Star:
      return "*";
    case TokenKind::Slash:
      return "/";
    case TokenKind::Percent:
      return "%";
    case TokenKind::Eq:
      return "=";
    case TokenKind::EqEq:
      return "==";
    case TokenKind::Bang:
      return "!";
    case TokenKind::BangEq:
      return "!=";
    case TokenKind::Lt:
      return "<";
    case TokenKind::Gt:
      return ">";
    case TokenKind::LtEq:
      return "<=";
    case TokenKind::GtEq:
      return ">=";
    case TokenKind::AmpAmp:
      return "&&";
    case TokenKind::PipePipe:
      return "||";
    case TokenKind::Amp:
      return "&";
    case TokenKind::Pipe:
      return "|";
    case TokenKind::Caret:
      return "^";
    case TokenKind::Tilde:
      return "~";
    case TokenKind::LtLt:
      return "<<";
    case TokenKind::GtGt:
      return ">>";
    case TokenKind::PlusEq:
      return "+=";
    case TokenKind::MinusEq:
      return "-=";
    case TokenKind::StarEq:
      return "*=";
    case TokenKind::SlashEq:
      return "/=";
    case TokenKind::PercentEq:
      return "%=";
    case TokenKind::AmpEq:
      return "&=";
    case TokenKind::PipeEq:
      return "|=";
    case TokenKind::CaretEq:
      return "^=";
    case TokenKind::LtLtEq:
      return "<<=";
    case TokenKind::GtGtEq:
      return ">>=";
    case TokenKind::PlusPlus:
      return "++";
    case TokenKind::MinusMinus:
      return "--";
    default:
      return "?";
  }
}

std::string_view OpenGLGLSLEmitter::param_qual_str(ParamQualifier qualifier) {
  switch (qualifier) {
    case ParamQualifier::In:
      return "in";
    case ParamQualifier::Out:
      return "out";
    case ParamQualifier::Inout:
      return "inout";
    case ParamQualifier::Const:
      return "const";
    default:
      return "";
  }
}

void OpenGLGLSLEmitter::push_indent() { m_indent += "  "; }

void OpenGLGLSLEmitter::pop_indent() {
  if (m_indent.size() >= 2) {
    m_indent.resize(m_indent.size() - 2);
  }
}

void OpenGLGLSLEmitter::writeln(std::string_view text) {
  m_out += m_indent;
  m_out += text;
  m_out += '\n';
}

void OpenGLGLSLEmitter::write(std::string_view text) { m_out += text; }

} // namespace astralix
