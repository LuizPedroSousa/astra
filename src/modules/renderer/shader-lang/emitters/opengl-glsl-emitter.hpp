#pragma once
#include "shader-lang/ast.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix {

class OpenGLGLSLEmitter {
public:
  explicit OpenGLGLSLEmitter(const std::vector<ASTNode> &nodes);

  std::string emit(const Program &program, StageKind stage,
                   const std::unordered_set<std::string> &used_uniforms);

private:
  struct InterfaceBinding {
    std::string param_name;
    const InterfaceDecl *interface_decl = nullptr;
  };

  struct EntryContext {
    StageKind stage;
    std::string output_instance_name = "_stage_out";
    const InterfaceDecl *output_interface = nullptr;
    std::optional<std::string> output_local_name;
    std::vector<InterfaceBinding> varying_inputs;
    std::vector<InterfaceBinding> resource_inputs;
    std::unordered_set<std::string> used_resource_fields;
    std::unordered_map<std::string, std::string> field_aliases;
    std::unordered_map<std::string, std::string> output_aliases;
    std::unordered_map<std::string, std::string> resource_aliases;
  };

  void emit_globals(const Program &prog, StageKind stage,
                    const std::unordered_set<std::string> &used_uniforms,
                    const FuncDecl *entry, const EntryContext *ctx);
  void emit_entry_signature(const EntryContext &ctx);
  void emit_entry_main(const FuncDecl &entry, const EntryContext &ctx);
  EntryContext build_entry_context(const Program &program, StageKind stage,
                                   const FuncDecl &entry) const;
  const FuncDecl *find_stage_entry(const Program &program,
                                   StageKind stage) const;
  bool is_shadowed(const std::string &name, StageKind kind,
                   const Program &prog) const;
  bool interface_has_uniform_fields(const InterfaceDecl &iface) const;
  static bool has_uniform_annotation(const Annotations &annotations);

  void emit_struct(const StructDecl &d);
  void emit_uniform(const UniformDecl &d);
  void emit_buffer(const BufferDecl &d);
  void emit_function_prototype(const FuncDecl &d);
  void emit_function(const FuncDecl &d);
  void emit_field(const FieldDecl &f, bool emit_initializer = true);
  void emit_global_var(const VarDecl &d);
  void emit_global_inline_interface(const InlineInterfaceDecl &d,
                                    StageKind stage);
  void emit_global_interface_ref(const InterfaceRef &d, const Program &prog,
                                 StageKind stage);
  void emit_stage_resource_if_used(
      const UniformDecl &d, StageKind stage, const Program &prog,
      const std::unordered_set<std::string> &used_uniforms);
  void emit_stage_resource_if_used(
      const BufferDecl &d, StageKind stage, const Program &prog,
      const std::unordered_set<std::string> &used_uniforms);
  void emit_interface_block(bool is_in, const std::string &block_name,
                            const std::vector<NodeID> &fields,
                            const std::optional<std::string> &inst);
  void emit_param(NodeID id);
  void emit_var_decl_stmt(const VarDecl &d, const EntryContext *ctx);
  void emit_var_decl_for_init(const VarDecl &d, const EntryContext *ctx);
  void emit_output_field_initializers(const EntryContext &ctx);
  const VarDecl *find_var_decl(NodeID id) const;
  bool try_emit_structured_return(const ReturnStmt &stmt,
                                  const EntryContext *ctx);
  bool try_emit_output_fields(const std::vector<NodeID> &args,
                              const EntryContext &ctx);

  void emit_stmt_node(const BlockStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const IfStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const ForStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const WhileStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const ReturnStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const ExprStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const DeclStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const VarDecl &stmt, const EntryContext *ctx);
  void emit_stmt_node(const BreakStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const ContinueStmt &stmt, const EntryContext *ctx);
  void emit_stmt_node(const DiscardStmt &stmt, const EntryContext *ctx);

  void emit_stmt(NodeID id, const EntryContext *ctx = nullptr);
  void emit_body_stmt(NodeID id, const EntryContext *ctx = nullptr);
  void emit_for_init(NodeID id, const EntryContext *ctx = nullptr);
  void emit_literal(const LiteralExpr &expr);
  void emit_call_args(const std::vector<NodeID> &args,
                      const EntryContext *ctx = nullptr);
  bool try_emit_field_alias(const FieldExpr &expr,
                            const EntryContext *ctx = nullptr);
  void emit_expr_node(const LiteralExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const IdentifierExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const UnresolvedRef &expr, const EntryContext *ctx);
  void emit_expr_node(const BinaryExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const UnaryExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const TernaryExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const CallExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const IndexExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const FieldExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const ConstructExpr &expr, const EntryContext *ctx);
  void emit_expr_node(const AssignExpr &expr, const EntryContext *ctx);
  void emit_expr(NodeID id, const EntryContext *ctx = nullptr);
  void emit_expr_paren(NodeID id, const EntryContext *ctx = nullptr);

  std::string layout_quals(const Annotations &annots) const;
  std::string type_str(const TypeRef &t) const;
  static std::string_view op_str(TokenKind op);
  static std::string_view param_qual_str(ParamQualifier q);

  const InterfaceDecl *find_interface(const Program &prog,
                                      const std::string &name) const;
  const ASTNode &node(NodeID id) const;

  void push_indent();
  void pop_indent();
  void writeln(std::string_view text = {});
  void write(std::string_view text);

  const std::vector<ASTNode> &m_nodes;
  std::string m_out;
  std::string m_indent;
};

} // namespace astralix
