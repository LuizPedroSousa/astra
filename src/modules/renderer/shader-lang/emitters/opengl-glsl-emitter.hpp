#pragma once

#include "shader-lang/lowering/glsl-lowering.hpp"
#include <string>
#include <string_view>

namespace astralix {

class OpenGLGLSLEmitter {
public:
  std::string emit(const GLSLStage &stage);

private:
  void emit_decl(const GLSLDecl &decl);
  void emit_struct(const GLSLStructDecl &decl);
  void emit_global_var(const GLSLGlobalVarDecl &decl);
  void emit_interface_block(const GLSLInterfaceBlockDecl &decl);
  void emit_field(const GLSLFieldDecl &decl, bool emit_initializer = true);
  void emit_function(const GLSLFunctionDecl &decl);
  void emit_param(const GLSLParamDecl &decl);

  void emit_stmt(const GLSLStmt &stmt);
  void emit_body_stmt(const GLSLStmt &stmt);
  void emit_for_init(const GLSLStmt *stmt);
  void emit_expr(const GLSLExpr &expr);
  void emit_expr_paren(const GLSLExpr &expr);
  void emit_call_args(const std::vector<GLSLExprPtr> &args);
  void emit_literal(const GLSLLiteralExpr &expr);

  std::string layout_quals(const Annotations &annotations) const;
  std::string type_str(const TypeRef &type_ref) const;
  static std::string_view op_str(TokenKind op);
  static std::string_view param_qual_str(ParamQualifier qualifier);

  void push_indent();
  void pop_indent();
  void writeln(std::string_view text = {});
  void write(std::string_view text);

  std::string m_out;
  std::string m_indent;
};

} // namespace astralix
