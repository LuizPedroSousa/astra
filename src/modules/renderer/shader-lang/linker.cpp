#include "shader-lang/linker.hpp"
#include "shader-lang/diagnostics.hpp"
#include "shader-lang/parser.hpp"
#include "shader-lang/tokenizer.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace astralix {

static std::pair<std::vector<Token>, std::vector<std::string>>
tokenize_source(std::string_view src, std::string_view filename = "") {
  Tokenizer tokenizer(src, filename);
  std::vector<Token> tokens;

  while (true) {
    Token token = tokenizer.next();
    tokens.push_back(token);
    if (token.kind == TokenKind::EoF)
      break;
  }

  return {std::move(tokens), tokenizer.errors()};
}

static void offset_node_ids(std::vector<ASTNode> &nodes, NodeID offset_id) {
  for (auto &node : nodes) {
    node.id += offset_id;
    std::visit([offset_id](auto &decl) { decl.visit_offset(offset_id); },
               node.data);
  }
}

static void offset_program_ids(Program &program, NodeID offset_id) {
  for (auto &global : program.globals) {
    global += offset_id;
  }

  for (auto &stage : program.stages) {
    stage += offset_id;
  }
}

static NodeID unwrap_decl_stmt(NodeID id, const std::vector<ASTNode> &nodes) {
  if (const auto *decl_stmt = std::get_if<DeclStmt>(&nodes[id].data)) {
    return decl_stmt->decl;
  }

  return id;
}

struct DeclarationRegistry {
  std::unordered_set<std::string> &declared;
  ScopeInfo &scopes;
  std::optional<StageKind> stage_kind;

  void register_uniform_name(const std::string &name) const {
    if (stage_kind) {
      scopes.stage_uniforms[*stage_kind].insert(name);
    } else {
      scopes.global_uniforms.insert(name);
    }

    declared.insert(name);
  }

  void operator()(const FuncDecl &declaration) const {
    declared.insert(declaration.name);
  }

  void operator()(const StructDecl &declaration) const {
    declared.insert(declaration.name);
  }

  void operator()(const VarDecl &declaration) const {
    if (!stage_kind) {
      declared.insert(declaration.name);
    }
  }

  void operator()(const InterfaceDecl &declaration) const {
    if (!stage_kind) {
      declared.insert(declaration.name);
    }
  }

  void operator()(const InlineInterfaceDecl &declaration) const {
    if (declaration.instance_name) {
      declared.insert(*declaration.instance_name);
    }
  }

  void operator()(const InterfaceRef &declaration) const {
    if (declaration.instance_name) {
      declared.insert(*declaration.instance_name);
    }
  }

  void operator()(const UniformDecl &declaration) const {
    register_uniform_name(declaration.name);
  }

  void operator()(const BufferDecl &declaration) const {
    if (declaration.instance_name) {
      register_uniform_name(*declaration.instance_name);
    }
  }

  template <class T> void operator()(const T &) const {}
};

static void
validate_base_identifier(LinkResult &result, const ASTNode &base_node,
                         const std::unordered_set<std::string> &declared,
                         const std::unordered_set<std::string> &glsl_builtins,
                         const std::unordered_set<std::string> &local_vars) {
  const auto *identifier_expr = std::get_if<IdentifierExpr>(&base_node.data);
  if (!identifier_expr) {
    return;
  }

  if (!declared.count(identifier_expr->name) &&
      !glsl_builtins.count(identifier_expr->name) &&
      !local_vars.count(identifier_expr->name)) {
    PUSH_UNDEFINED_IDENTIFIER(result, identifier_expr->name,
                              base_node.location);
  }
}

static const InterfaceDecl *
find_interface_decl(const Program &program, const std::vector<ASTNode> &nodes,
                    std::string_view name) {
  for (NodeID global : program.globals) {
    const auto *interface_decl =
        std::get_if<InterfaceDecl>(&nodes[global].data);
    if (interface_decl && interface_decl->name == name) {
      return interface_decl;
    }
  }

  return nullptr;
}

static const InterfaceDecl *
find_declared_output_interface(const Program &program,
                               const std::vector<ASTNode> &nodes,
                               const FuncDecl &func_decl) {
  if (func_decl.ret.kind != TokenKind::Identifier) {
    return nullptr;
  }

  return find_interface_decl(program, nodes, func_decl.ret.name);
}

static void validate_stage_entry_return_type(LinkResult &result,
                                             const ASTNode &func_node,
                                             const FuncDecl &func_decl) {
  if (!func_decl.stage_kind || !func_decl.ret.array_size) {
    return;
  }

  result.errors.push_back(format_located_error(
      "stage entry function '" + func_decl.name + "' cannot return arrays",
      func_node.location));
}

static bool is_output_interface_local(const ASTNode &node,
                                      std::string_view interface_name) {
  const auto *var_decl = std::get_if<VarDecl>(&node.data);
  return var_decl && var_decl->type.kind == TokenKind::Identifier &&
         var_decl->type.name == interface_name;
}

static void push_output_local_error(LinkResult &result, const ASTNode &node,
                                    std::string message) {
  result.errors.push_back(
      format_located_error(std::move(message), node.location));
}

template <class Fn>
static void visit_subtree(NodeID id, const std::vector<ASTNode> &nodes,
                          std::optional<NodeID> parent_id, Fn &fn) {
  fn(id, parent_id);
  visit_child_ids(nodes[id], [&](NodeID child_id) {
    visit_subtree(child_id, nodes, id, fn);
  });
}

static void validate_output_local_usage(LinkResult &result,
                                        const Program &program,
                                        const FuncDecl &func_decl,
                                        const std::vector<ASTNode> &nodes) {
  const InterfaceDecl *output_interface =
      find_declared_output_interface(program, nodes, func_decl);
  if (!output_interface) {
    return;
  }

  std::vector<NodeID> output_local_ids;
  auto collector = [&](NodeID id, std::optional<NodeID>) {
    if (is_output_interface_local(nodes[id], output_interface->name)) {
      output_local_ids.push_back(id);
    }
  };
  visit_subtree(func_decl.body, nodes, std::nullopt, collector);

  if (output_local_ids.empty()) {
    return;
  }

  std::unordered_set<std::string> output_local_names;
  for (NodeID output_local_id : output_local_ids) {
    const auto &var_decl = std::get<VarDecl>(nodes[output_local_id].data);
    output_local_names.insert(var_decl.name);

    if (var_decl.init) {
      push_output_local_error(result, nodes[output_local_id],
                              "stage entry output accumulator '" +
                                  var_decl.name +
                                  "' cannot have an initializer");
    }
  }

  if (output_local_ids.size() > 1) {
    push_output_local_error(
        result, nodes[output_local_ids[1]],
        "stage entry output accumulator supports only one local of type '" +
            output_interface->name + "'");
  }

  const auto &sink_decl =
      std::get<VarDecl>(nodes[output_local_ids.front()].data);
  const std::string &sink_name = sink_decl.name;

  auto validator = [&](NodeID id, std::optional<NodeID> parent_id) {
    const auto &current_node = nodes[id];

    if (const auto *assign_expr = std::get_if<AssignExpr>(&current_node.data)) {
      if (const auto *lhs_identifier =
              std::get_if<IdentifierExpr>(&nodes[assign_expr->lhs].data)) {
        if (lhs_identifier->name == sink_name) {
          push_output_local_error(result, current_node,
                                  "stage entry output accumulator '" +
                                      sink_name +
                                      "' cannot be assigned as a whole value");
        }
      }
    }

    if (const auto *return_stmt = std::get_if<ReturnStmt>(&current_node.data)) {
      if (!return_stmt->value) {
        return;
      }

      if (const auto *return_identifier =
              std::get_if<IdentifierExpr>(&nodes[*return_stmt->value].data)) {
        if (output_local_names.count(return_identifier->name) > 0 &&
            return_identifier->name != sink_name) {
          push_output_local_error(
              result, current_node,
              "stage entry output accumulator must return '" + sink_name +
                  "', not '" + return_identifier->name + "'");
        }
      }
    }

    const auto *identifier_expr =
        std::get_if<IdentifierExpr>(&current_node.data);
    if (!identifier_expr || identifier_expr->name != sink_name) {
      return;
    }

    if (!parent_id) {
      push_output_local_error(
          result, current_node,
          "stage entry output accumulator '" + sink_name +
              "' can only be used via field access or 'return " + sink_name +
              "'");
      return;
    }

    const auto &parent_node = nodes[*parent_id];
    if (const auto *field_expr = std::get_if<FieldExpr>(&parent_node.data)) {
      if (field_expr->object == id) {
        return;
      }
    }

    if (const auto *return_stmt = std::get_if<ReturnStmt>(&parent_node.data)) {
      if (return_stmt->value && *return_stmt->value == id) {
        return;
      }
    }

    if (const auto *assign_expr = std::get_if<AssignExpr>(&parent_node.data)) {
      if (assign_expr->lhs == id) {
        return;
      }
    }

    push_output_local_error(
        result, current_node,
        "stage entry output accumulator '" + sink_name +
            "' can only be used via field access or 'return " + sink_name +
            "'");
  };
  visit_subtree(func_decl.body, nodes, std::nullopt, validator);
}

static std::filesystem::path
resolve_include_path(std::string_view include_path, std::string_view base_path) {
  std::filesystem::path resolved(include_path);
  if (resolved.is_absolute() || base_path.empty()) {
    return resolved.lexically_normal();
  }

  return (std::filesystem::path(base_path) / resolved).lexically_normal();
}

static bool load_program_includes(
    Program &program, LinkResult &result, std::string_view base_path,
    std::vector<std::filesystem::path> &include_stack,
    std::unordered_set<std::string> &seen_dependencies) {
  for (const std::string &include_path : program.includes) {
    const auto resolved_path = resolve_include_path(include_path, base_path);
    const auto dependency_key = resolved_path.generic_string();

    if (seen_dependencies.insert(dependency_key).second) {
      result.dependencies.push_back(resolved_path);
    }

    if (std::find(include_stack.begin(), include_stack.end(), resolved_path) !=
        include_stack.end()) {
      result.errors.push_back("circular include: '" + dependency_key + "'");
      return false;
    }

    std::ifstream include_file(resolved_path);

    if (!include_file) {
      PUSH_CANNOT_OPEN_INCLUDE(result, dependency_key);
      return false;
    }

    std::ostringstream include_buffer;
    include_buffer << include_file.rdbuf();
    std::string include_src = include_buffer.str();

    auto [include_tokens, include_token_errors] =
        tokenize_source(include_src, dependency_key);

    if (!include_token_errors.empty()) {
      for (auto &e : include_token_errors) {
        PUSH_PREFIXED_INCLUDE_ERROR(result, dependency_key, e);
      }
      return false;
    }

    Parser include_parser(std::move(include_tokens), include_src);
    Program include_program = include_parser.parse();
    if (!include_parser.errors().empty()) {
      for (auto &e : include_parser.errors()) {
        PUSH_PREFIXED_INCLUDE_ERROR(result, dependency_key, e);
      }
      return false;
    }

    NodeID offset = static_cast<NodeID>(result.all_nodes.size());
    auto include_nodes = include_parser.nodes();
    offset_node_ids(include_nodes, offset);
    offset_program_ids(include_program, offset);
    result.all_nodes.insert(result.all_nodes.end(), include_nodes.begin(),
                            include_nodes.end());

    include_stack.push_back(resolved_path);
    if (!load_program_includes(include_program, result, base_path,
                               include_stack, seen_dependencies)) {
      include_stack.pop_back();
      return false;
    }
    include_stack.pop_back();

    program.globals.insert(program.globals.end(), include_program.globals.begin(),
                           include_program.globals.end());
  }

  return true;
}

LinkResult Linker::link(Program &program, const std::vector<ASTNode> &nodes,
                        std::string_view base_path) {
  LinkResult result;

  result.all_nodes = nodes;

  std::vector<std::filesystem::path> include_stack;
  std::unordered_set<std::string> seen_dependencies;
  if (!load_program_includes(program, result, base_path, include_stack,
                             seen_dependencies)) {
    return result;
  }

  static const std::unordered_set<std::string> glsl_builtins = {
      "gl_Position",           "gl_PointSize",
      "gl_ClipDistance",       "gl_VertexID",
      "gl_InstanceID",         "gl_DrawID",
      "gl_FragCoord",          "gl_FrontFacing",
      "gl_PointCoord",         "gl_FragDepth",
      "gl_SampleID",           "gl_SamplePosition",
      "gl_SampleMaskIn",       "gl_SampleMask",
      "gl_NumWorkGroups",      "gl_WorkGroupSize",
      "gl_WorkGroupID",        "gl_LocalInvocationID",
      "gl_GlobalInvocationID", "gl_LocalInvocationIndex"};

  ScopeInfo scopes;
  std::unordered_set<std::string> declared;

  for (NodeID global : program.globals) {
    std::visit(DeclarationRegistry{declared, scopes, std::nullopt},
               result.all_nodes[global].data);
  }

  for (NodeID stage : program.stages) {
    const auto &func_decl = std::get<FuncDecl>(result.all_nodes[stage].data);
    if (!func_decl.stage_kind.has_value()) {
      continue;
    }
    StageKind stage_kind = func_decl.stage_kind.value();
    const auto *body_block =
        std::get_if<BlockStmt>(&result.all_nodes[func_decl.body].data);

    if (!body_block) {
      continue;
    }

    for (NodeID item : body_block->stmts) {
      NodeID item_node = unwrap_decl_stmt(item, result.all_nodes);
      std::visit(DeclarationRegistry{declared, scopes, stage_kind},
                 result.all_nodes[item_node].data);
    }
  }

  for (NodeID stage : program.stages) {
    const auto &func_node = result.all_nodes[stage];
    const auto &func_decl = std::get<FuncDecl>(func_node.data);
    if (!func_decl.stage_kind) {
      continue;
    }

    validate_stage_entry_return_type(result, func_node, func_decl);
    validate_output_local_usage(result, program, func_decl, result.all_nodes);

    scan_uniform_usage(func_decl.body, *func_decl.stage_kind, result.all_nodes,
                       scopes, result.uniform_usage);
  }

  for (const auto &node : result.all_nodes) {
    if (const auto *unresolved_ref = std::get_if<UnresolvedRef>(&node.data)) {
      if (!declared.count(unresolved_ref->name)) {
        PUSH_UNRESOLVED_SYMBOL(result, unresolved_ref->name, node.location);
      }
    }
  }

  std::unordered_set<std::string> local_vars;

  auto collect_function_locals = [&](NodeID function_id) {
    if (std::holds_alternative<FuncDecl>(result.all_nodes[function_id].data)) {
      collect_locals(function_id, result.all_nodes, local_vars);
    }
  };

  for (NodeID global : program.globals) {
    collect_function_locals(global);
  }

  for (NodeID stage : program.stages) {
    collect_function_locals(stage);
  }

  for (const auto &node : result.all_nodes) {
    if (const auto *field_expr = std::get_if<FieldExpr>(&node.data)) {
      validate_base_identifier(result, result.all_nodes[field_expr->object],
                               declared, glsl_builtins, local_vars);
    }

    if (const auto *idx = std::get_if<IndexExpr>(&node.data)) {
      validate_base_identifier(result, result.all_nodes[idx->array], declared,
                               glsl_builtins, local_vars);
    }
  }

  return result;
}

void Linker::scan_uniform_usage(
    NodeID nid, StageKind stage, const std::vector<ASTNode> &nodes,
    const ScopeInfo &scopes,
    std::unordered_map<StageKind, std::unordered_set<std::string>>
        &uniform_usage) {
  const auto &node = nodes[nid];

  if (const auto *ie = std::get_if<IdentifierExpr>(&node.data)) {
    bool is_global = scopes.global_uniforms.count(ie->name) > 0;
    bool is_stage_uniform = scopes.stage_uniforms.count(stage) > 0 &&
                            scopes.stage_uniforms.at(stage).count(ie->name) > 0;
    if (is_global || is_stage_uniform) {
      uniform_usage[stage].insert(ie->name);
    }
  }

  visit_child_ids(node.data, [&](NodeID child) {
    scan_uniform_usage(child, stage, nodes, scopes, uniform_usage);
  });
}

void Linker::collect_locals(NodeID nid, const std::vector<ASTNode> &nodes,
                            std::unordered_set<std::string> &local_vars) {
  const auto &node = nodes[nid];
  if (const auto *vd = std::get_if<VarDecl>(&node.data)) {
    local_vars.insert(vd->name);
  } else if (const auto *pd = std::get_if<ParamDecl>(&node.data)) {
    local_vars.insert(pd->name);
  }

  visit_child_ids(node.data, [&](NodeID child) {
    collect_locals(child, nodes, local_vars);
  });
}

} // namespace astralix
