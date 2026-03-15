#pragma once
#include "shader-lang/ast.hpp"
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace astralix {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens, std::string_view source = "");

  Program parse();

  const std::vector<ASTNode> &nodes() const { return m_nodes; }
  inline const ASTNode &peek_node_at(NodeID node_id) const {
    return m_nodes[node_id];
  }
  const std::vector<std::string> &errors() const { return m_errors; }

private:
  NodeID parse_global_decl();
  // NodeID parse_stage_block();
  //
  // NodeID parse_stage_item();
  NodeID parse_var_decl();
  NodeID parse_function_decl(AttributeList attributes);
  NodeID parse_struct_decl();
  NodeID parse_const_decl();
  NodeID parse_interface_decl(AttributeList attributes);
  NodeID parse_inline_interface_decl(AttributeList attributes, bool is_in);
  NodeID parse_interface_ref(bool is_in);
  NodeID parse_uniform_decl(AttributeList attributes);
  NodeID parse_buffer_decl(AttributeList attributes, bool is_uniform = false);
  NodeID
  parse_field_decl(std::optional<AttributeList> attributes = std::nullopt);
  NodeID parse_param();
  AttributeList parse_attributes();
  Annotations parse_annotations();

  TypeRef parse_type_ref();

  NodeID parse_stmt();
  NodeID parse_block_stmt();
  NodeID parse_if_stmt();
  NodeID parse_for_stmt();
  NodeID parse_while_stmt();
  NodeID parse_do_while_stmt();
  NodeID parse_return_stmt();
  NodeID parse_local_var_or_expr_stmt();

  NodeID parse_expr(int min_binding_power = 0);
  NodeID parse_prefix();

  static int left_binding_power(TokenKind op);
  static int right_binding_power(TokenKind op);
  static bool is_assign_op(TokenKind op);
  static bool is_type_keyword(TokenKind k);

  inline NodeID push_node(ASTNodeData data, SourceLocation loc);
  Token advance();
  Token peek(int offset = 0) const;
  Token expect(TokenKind kind, std::string_view msg);
  Token expect_name(std::string_view msg);
  bool check(TokenKind kind, int offset = 0) const;
  bool match(TokenKind kind);
  void synchronize();

  void declare(const std::string &name) { m_declared.insert(name); }
  bool is_declared(const std::string &name) const {
    return m_declared.count(name) > 0;
  }

  std::vector<Token> m_tokens;
  std::string_view m_source;
  size_t m_pos = 0;
  Token m_last_token = {};
  std::vector<ASTNode> m_nodes;
  std::vector<std::string> m_errors;
  std::unordered_set<std::string> m_declared;
};

} // namespace astralix
