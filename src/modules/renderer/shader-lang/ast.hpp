#pragma once
#include "shader-lang/token.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace astralix {
using NodeID = uint32_t;

enum class StageKind : uint8_t { Vertex, Fragment, Geometry, Compute };

enum class ParamQualifier : uint8_t { None, In, Out, Inout, Const };

enum class AttributeKind : uint8_t {
  In,
  Uniform,
  Location,
  Binding,
  Set,
  Std140,
  Std430,
  PushConstant,
  VertexStage,
  FragmentStage,
  GeometryStage,
  ComputeStage
};

inline std::string_view attribute_kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::In:
      return "@in";
    case AttributeKind::Uniform:
      return "@uniform";
    case AttributeKind::Location:
      return "@location";
    case AttributeKind::Binding:
      return "@binding";
    case AttributeKind::Set:
      return "@set";
    case AttributeKind::Std140:
      return "@std140";
    case AttributeKind::Std430:
      return "@std430";
    case AttributeKind::PushConstant:
      return "@push_constant";

    case AttributeKind::VertexStage:
      return "@vertex";

    case AttributeKind::FragmentStage:
      return "@fragment";

    case AttributeKind::GeometryStage:
      return "@geometry";
  }

  return "@<unknown>";
}

using AttributeValue = std::variant<std::monostate, StageKind, int32_t>;

struct Attribute {
  AttributeKind kind;
  AttributeValue value{};
  SourceLocation location;
};

using AttributeList = std::vector<Attribute>;

enum class InterfaceRole : uint8_t {
  None,
  In,
  Uniform,
};

enum class AnnotationKind : uint8_t {
  In,
  Uniform,
  Location,
  Binding,
  Set,
  Std140,
  Std430,
  PushConstant,
};

inline std::string_view annotation_kind_name(AnnotationKind kind) {
  switch (kind) {
    case AnnotationKind::In:
      return "@in";
    case AnnotationKind::Uniform:
      return "@uniform";
    case AnnotationKind::Location:
      return "@location";
    case AnnotationKind::Binding:
      return "@binding";
    case AnnotationKind::Set:
      return "@set";
    case AnnotationKind::Std140:
      return "@std140";
    case AnnotationKind::Std430:
      return "@std430";
    case AnnotationKind::PushConstant:
      return "@push_constant";
  }

  return "@<unknown>";
}

struct TypeRef {
  TokenKind kind;
  std::string name;
  std::optional<uint32_t> array_size;
  bool is_runtime_sized = false;
};

struct Annotation {
  AnnotationKind kind;
  int32_t slot = -1;
  int32_t set = -1;
  SourceLocation location;
};

using Annotations = std::vector<Annotation>;

struct LiteralExpr {
  std::variant<bool, int64_t, double> value;

  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct IdentifierExpr {
  std::string name;

  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct UnresolvedRef {
  std::string name;

  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct BinaryExpr {
  NodeID lhs, rhs;
  TokenKind op;

  void visit_offset(NodeID off) {
    lhs += off;
    rhs += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(lhs);
    fn(rhs);
  }
};

struct UnaryExpr {
  NodeID operand;
  TokenKind op;
  bool prefix;

  void visit_offset(NodeID off) { operand += off; }
  template <class Fn> void visit_child_ids(Fn &&fn) const { fn(operand); }
};

struct TernaryExpr {
  NodeID cond, then_expr, else_expr;

  void visit_offset(NodeID off) {
    cond += off;
    then_expr += off;
    else_expr += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(cond);
    fn(then_expr);
    fn(else_expr);
  }
};

struct CallExpr {
  NodeID callee;
  std::vector<NodeID> args;

  void visit_offset(NodeID off) {
    callee += off;

    for (auto &arg : args) {
      arg += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(callee);
    for (NodeID arg : args) {
      fn(arg);
    }
  }
};

struct IndexExpr {
  NodeID array, index;

  void visit_offset(NodeID off) {
    array += off;
    index += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(array);
    fn(index);
  }
};

struct FieldExpr {
  NodeID object;
  std::string field;

  void visit_offset(NodeID off) { object += off; }
  template <class Fn> void visit_child_ids(Fn &&fn) const { fn(object); }
};

struct ConstructExpr {
  TypeRef type;
  std::vector<NodeID> args;

  void visit_offset(NodeID off) {
    for (auto &arg : args) {
      arg += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID arg : args) {
      fn(arg);
    }
  }
};

struct AssignExpr {
  NodeID lhs, rhs;
  TokenKind op;

  void visit_offset(NodeID off) {
    lhs += off;
    rhs += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(lhs);
    fn(rhs);
  }
};

struct BlockStmt {
  std::vector<NodeID> stmts;

  void visit_offset(NodeID off) {
    for (auto &stmt : stmts) {
      stmt += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID stmt : stmts) {
      fn(stmt);
    }
  }
};

struct IfStmt {
  NodeID cond;
  NodeID then_br;
  std::optional<NodeID> else_br;
  void visit_offset(NodeID off) {
    cond += off;
    then_br += off;

    if (else_br) {
      *else_br += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(cond);
    fn(then_br);
    if (else_br) {
      fn(*else_br);
    }
  }
};

struct ForStmt {
  NodeID init;
  std::optional<NodeID> cond, step;
  NodeID body;

  void visit_offset(NodeID off) {
    init += off;

    if (cond) {
      *cond += off;
    }

    if (step) {
      *step += off;
    }

    body += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(init);
    if (cond) {
      fn(*cond);
    }
    if (step) {
      fn(*step);
    }
    fn(body);
  }
};

struct WhileStmt {
  NodeID cond, body;

  void visit_offset(NodeID off) {
    cond += off;
    body += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    fn(cond);
    fn(body);
  }
};

struct ReturnStmt {
  std::optional<NodeID> value;

  void visit_offset(NodeID off) {
    if (value)
      *value += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    if (value) {
      fn(*value);
    }
  }
};

struct ExprStmt {
  NodeID expr;

  void visit_offset(NodeID off) { expr += off; }
  template <class Fn> void visit_child_ids(Fn &&fn) const { fn(expr); }
};

struct DeclStmt {
  NodeID decl;

  void visit_offset(NodeID off) { decl += off; }
  template <class Fn> void visit_child_ids(Fn &&fn) const { fn(decl); }
};

struct BreakStmt {
  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct ContinueStmt {
  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct DiscardStmt {
  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct VarDecl {
  TypeRef type;
  std::string name;
  std::optional<NodeID> init;
  bool is_const = false;

  void visit_offset(NodeID off) {
    if (init) {
      *init += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    if (init) {
      fn(*init);
    }
  }
};

struct ParamDecl {
  TypeRef type;
  std::string name;
  ParamQualifier qual;

  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct FuncDecl {
  TypeRef ret;
  std::string name;
  std::vector<NodeID> params;
  NodeID body;

  std::optional<StageKind> stage_kind;

  void visit_offset(NodeID off) {
    for (auto &param : params) {
      param += off;
    }
    body += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID param : params) {
      fn(param);
    }
    fn(body);
  }
};

struct StructDecl {
  std::string name;
  std::vector<NodeID> fields;

  void visit_offset(NodeID off) {
    for (auto &field : fields) {
      field += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID field : fields) {
      fn(field);
    }
  }
};

struct FieldDecl {
  TypeRef type;
  std::string name;

  std::optional<uint32_t> array_size;

  std::optional<NodeID> init;

  Annotations annotations;

  void visit_offset(NodeID off) {
    if (init) {
      *init += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    if (init) {
      fn(*init);
    }
  }
};

struct UniformDecl {
  TypeRef type;
  std::string name;
  Annotations annotations;
  std::optional<NodeID> default_val;

  std::optional<uint32_t> array_size;

  void visit_offset(NodeID off) {
    if (default_val) {
      *default_val += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    if (default_val) {
      fn(*default_val);
    }
  }
};

struct BufferDecl {
  std::string name;
  std::vector<NodeID> fields;
  Annotations annotations;
  std::optional<std::string> instance_name;
  bool is_uniform = false;

  void visit_offset(NodeID off) {
    for (auto &field : fields) {
      field += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID field : fields) {
      fn(field);
    }
  }
};

struct InterfaceDecl {
  std::string name;
  std::vector<NodeID> fields;
  InterfaceRole role = InterfaceRole::None;
  std::optional<std::string> implicit_instance_name;

  void visit_offset(NodeID off) {
    for (auto &field : fields) {
      field += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID field : fields) {
      fn(field);
    }
  }
};

struct Decorator {
  std::optional<StageKind> stage;
  InterfaceRole interface_role = InterfaceRole::None;
};

struct InlineInterfaceDecl {
  bool is_in;
  std::string name;
  std::vector<NodeID> fields;
  std::optional<std::string> instance_name;

  Annotations annotations;

  void visit_offset(NodeID off) {
    for (auto &field : fields)
      field += off;
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID field : fields) {
      fn(field);
    }
  }
};

struct InterfaceRef {
  bool is_in;
  std::string block_name;
  std::optional<std::string> instance_name;
  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&) const {}
};

struct StageBlock {
  StageKind kind;
  std::vector<NodeID> items;

  void visit_offset(NodeID off) {
    for (auto &item : items) {
      item += off;
    }
  }

  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID item : items) {
      fn(item);
    }
  }
};

struct Program {
  int version;
  std::vector<std::string> includes;
  std::vector<NodeID> globals;
  std::vector<NodeID> stages;

  void visit_offset(NodeID) {}
  template <class Fn> void visit_child_ids(Fn &&fn) const {
    for (NodeID global : globals) {
      fn(global);
    }
    for (NodeID stage : stages) {
      fn(stage);
    }
  }
};

using ASTNodeData =
    std::variant<LiteralExpr, IdentifierExpr, UnresolvedRef, BinaryExpr,
                 UnaryExpr, TernaryExpr, CallExpr, IndexExpr, FieldExpr,
                 ConstructExpr, AssignExpr, BlockStmt, IfStmt, ForStmt,
                 WhileStmt, ReturnStmt, ExprStmt, DeclStmt, BreakStmt,
                 ContinueStmt, DiscardStmt, VarDecl, ParamDecl, FuncDecl,
                 StructDecl, FieldDecl, UniformDecl, BufferDecl, InterfaceDecl,
                 InlineInterfaceDecl, StageBlock, InterfaceRef, Program>;

struct ASTNode {
  NodeID id;
  SourceLocation location;
  TypeRef type;
  ASTNodeData data;
};

template <class Fn>
inline void visit_child_ids(const ASTNodeData &data, Fn &&fn) {
  auto &&visitor = fn;
  std::visit([&visitor](const auto &node) { node.visit_child_ids(visitor); },
             data);
}

template <class Fn> inline void visit_child_ids(const ASTNode &node, Fn &&fn) {
  visit_child_ids(node.data, fn);
}

}; // namespace astralix
