#include "shader-lang/lowering/canonical-lowering.hpp"

namespace astralix {

namespace {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <class T>
CanonicalExprPtr make_expr(SourceLocation location, const TypeRef &type,
                           T data) {
  auto expr = std::make_unique<CanonicalExpr>();
  expr->location = location;
  expr->type = type;
  expr->data = std::move(data);
  return expr;
}

template <class T> CanonicalStmtPtr make_stmt(SourceLocation location, T data) {
  auto stmt = std::make_unique<CanonicalStmt>();
  stmt->location = location;
  stmt->data = std::move(data);
  return stmt;
}

bool should_emit_interface(bool is_in, StageKind stage) {
  return (is_in && stage == StageKind::Fragment) ||
         (!is_in && stage == StageKind::Vertex);
}

bool emits_as_storage_block(const InlineInterfaceDecl &interface_decl) {
  return !interface_decl.annotations.empty();
}

std::optional<std::string>
find_output_local_name(const std::vector<ASTNode> &nodes, NodeID id,
                       std::string_view interface_name) {
  if (const auto *var_decl = std::get_if<VarDecl>(&nodes[id].data)) {
    if (var_decl->type.kind == TokenKind::Identifier &&
        var_decl->type.name == interface_name) {
      return var_decl->name;
    }
  }

  std::optional<std::string> output_local_name;
  visit_child_ids(nodes[id], [&](NodeID child_id) {
    if (!output_local_name) {
      output_local_name = find_output_local_name(nodes, child_id, interface_name);
    }
  });
  return output_local_name;
}

void collect_resource_field_usage(
    const std::vector<ASTNode> &nodes, NodeID id,
    const std::unordered_set<std::string> &resource_param_names,
    std::unordered_set<std::string> &used_resource_fields) {
  if (const auto *field_expr = std::get_if<FieldExpr>(&nodes[id].data)) {
    if (const auto *identifier_expr =
            std::get_if<IdentifierExpr>(&nodes[field_expr->object].data)) {
      if (resource_param_names.count(identifier_expr->name) > 0) {
        used_resource_fields.insert(identifier_expr->name + "." +
                                    field_expr->field);
      }
    }
  }

  visit_child_ids(nodes[id], [&](NodeID child_id) {
    collect_resource_field_usage(nodes, child_id, resource_param_names,
                                 used_resource_fields);
  });
}

std::unordered_map<std::string, NodeID>
collect_global_function_ids(const Program &program,
                            const std::vector<ASTNode> &nodes) {
  std::unordered_map<std::string, NodeID> function_ids;

  for (NodeID global_id : program.globals) {
    const auto *func_decl = std::get_if<FuncDecl>(&nodes[global_id].data);
    if (!func_decl) {
      continue;
    }

    function_ids.emplace(func_decl->name, global_id);
  }

  return function_ids;
}

void collect_reachable_function_calls(
    const std::vector<ASTNode> &nodes, NodeID id,
    const std::unordered_map<std::string, NodeID> &function_ids,
    std::unordered_set<NodeID> &reachable_function_ids) {
  if (const auto *call_expr = std::get_if<CallExpr>(&nodes[id].data)) {
    const ASTNode &callee_node = nodes[call_expr->callee];
    const std::string *callee_name = nullptr;

    if (const auto *identifier_expr =
            std::get_if<IdentifierExpr>(&callee_node.data)) {
      callee_name = &identifier_expr->name;
    } else if (const auto *unresolved_ref =
                   std::get_if<UnresolvedRef>(&callee_node.data)) {
      callee_name = &unresolved_ref->name;
    }

    if (callee_name) {
      auto function_it = function_ids.find(*callee_name);
      if (function_it != function_ids.end() &&
          reachable_function_ids.insert(function_it->second).second) {
        const auto &func_decl =
            std::get<FuncDecl>(nodes[function_it->second].data);
        collect_reachable_function_calls(nodes, func_decl.body, function_ids,
                                         reachable_function_ids);
      }
    }
  }

  visit_child_ids(nodes[id], [&](NodeID child_id) {
    collect_reachable_function_calls(nodes, child_id, function_ids,
                                     reachable_function_ids);
  });
}

std::unordered_set<NodeID>
collect_stage_function_ids(const Program &program,
                           const std::vector<ASTNode> &nodes,
                           const FuncDecl *entry) {
  std::unordered_set<NodeID> reachable_function_ids;
  auto function_ids = collect_global_function_ids(program, nodes);

  if (entry) {
    collect_reachable_function_calls(nodes, entry->body, function_ids,
                                     reachable_function_ids);
  }

  for (NodeID global_id : program.globals) {
    if (std::holds_alternative<FuncDecl>(nodes[global_id].data)) {
      continue;
    }

    collect_reachable_function_calls(nodes, global_id, function_ids,
                                     reachable_function_ids);
  }

  return reachable_function_ids;
}

std::unordered_map<std::string, NodeID>
collect_global_struct_ids(const Program &program,
                          const std::vector<ASTNode> &nodes) {
  std::unordered_map<std::string, NodeID> struct_ids;

  for (NodeID global_id : program.globals) {
    const auto *struct_decl = std::get_if<StructDecl>(&nodes[global_id].data);
    if (!struct_decl) {
      continue;
    }

    struct_ids.emplace(struct_decl->name, global_id);
  }

  return struct_ids;
}

void collect_struct_dependencies_from_node(
    NodeID id, const std::unordered_map<std::string, NodeID> &struct_ids,
    const std::vector<ASTNode> &nodes, std::unordered_set<NodeID> &struct_usage);

void collect_struct_dependencies_from_type(
    const TypeRef &type, const std::unordered_map<std::string, NodeID> &struct_ids,
    const std::vector<ASTNode> &nodes,
    std::unordered_set<NodeID> &struct_usage) {
  if (type.kind != TokenKind::Identifier) {
    return;
  }

  auto struct_it = struct_ids.find(type.name);
  if (struct_it == struct_ids.end() ||
      !struct_usage.insert(struct_it->second).second) {
    return;
  }

  const auto &struct_decl = std::get<StructDecl>(nodes[struct_it->second].data);
  for (NodeID field_id : struct_decl.fields) {
    const auto &field_decl = std::get<FieldDecl>(nodes[field_id].data);
    collect_struct_dependencies_from_type(field_decl.type, struct_ids, nodes,
                                          struct_usage);

    if (field_decl.init) {
      collect_struct_dependencies_from_node(*field_decl.init, struct_ids, nodes,
                                            struct_usage);
    }
  }
}

void collect_struct_dependencies_from_node(
    NodeID id, const std::unordered_map<std::string, NodeID> &struct_ids,
    const std::vector<ASTNode> &nodes,
    std::unordered_set<NodeID> &struct_usage) {
  const ASTNode &current_node = nodes[id];

  if (const auto *var_decl = std::get_if<VarDecl>(&current_node.data)) {
    collect_struct_dependencies_from_type(var_decl->type, struct_ids, nodes,
                                          struct_usage);
  } else if (const auto *param_decl =
                 std::get_if<ParamDecl>(&current_node.data)) {
    collect_struct_dependencies_from_type(param_decl->type, struct_ids, nodes,
                                          struct_usage);
  } else if (const auto *func_decl = std::get_if<FuncDecl>(&current_node.data)) {
    collect_struct_dependencies_from_type(func_decl->ret, struct_ids, nodes,
                                          struct_usage);
  } else if (const auto *field_decl =
                 std::get_if<FieldDecl>(&current_node.data)) {
    collect_struct_dependencies_from_type(field_decl->type, struct_ids, nodes,
                                          struct_usage);
  } else if (const auto *uniform_decl =
                 std::get_if<UniformDecl>(&current_node.data)) {
    collect_struct_dependencies_from_type(uniform_decl->type, struct_ids, nodes,
                                          struct_usage);
  } else if (const auto *construct_expr =
                 std::get_if<ConstructExpr>(&current_node.data)) {
    collect_struct_dependencies_from_type(construct_expr->type, struct_ids,
                                          nodes, struct_usage);
  } else if (const auto *call_expr = std::get_if<CallExpr>(&current_node.data)) {
    const ASTNode &callee_node = nodes[call_expr->callee];

    if (const auto *identifier_expr =
            std::get_if<IdentifierExpr>(&callee_node.data)) {
      collect_struct_dependencies_from_type(
          TypeRef{TokenKind::Identifier, identifier_expr->name}, struct_ids,
          nodes, struct_usage);
    } else if (const auto *unresolved_ref =
                   std::get_if<UnresolvedRef>(&callee_node.data)) {
      collect_struct_dependencies_from_type(
          TypeRef{TokenKind::Identifier, unresolved_ref->name}, struct_ids,
          nodes, struct_usage);
    }
  }

  visit_child_ids(current_node, [&](NodeID child_id) {
    collect_struct_dependencies_from_node(child_id, struct_ids, nodes,
                                          struct_usage);
  });
}

class CanonicalStageBuilder {
public:
  struct EntryContext {
    StageKind stage = StageKind::Vertex;
    const InterfaceDecl *output_interface = nullptr;
    std::optional<std::string> output_local_name;
    std::unordered_map<std::string, const InterfaceDecl *> varying_inputs;
    std::unordered_map<std::string, const InterfaceDecl *> resource_inputs;
    std::unordered_set<std::string> used_resource_fields;
  };

  CanonicalStageBuilder(const Program &program, const LinkResult &link_result,
                        StageKind stage)
      : m_program(program), m_link_result(link_result), m_stage(stage),
        m_nodes(link_result.all_nodes) {}

  CanonicalLoweringResult lower() const {
    CanonicalLoweringResult result;
    result.stage.version = m_program.version;
    result.stage.stage = m_stage;

    const FuncDecl *entry = find_stage_entry();
    if (!entry) {
      result.errors.push_back("missing stage entry");
      return result;
    }

    EntryContext entry_ctx = build_entry_context(*entry);
    std::unordered_set<NodeID> stage_function_ids =
        collect_stage_function_ids(m_program, m_nodes, entry);
    std::unordered_set<NodeID> stage_struct_ids =
        collect_stage_struct_ids(entry, entry_ctx, stage_function_ids);

    for (NodeID global_id : m_program.globals) {
      if (stage_struct_ids.count(global_id) == 0) {
        continue;
      }

      const auto *struct_decl = std::get_if<StructDecl>(&node(global_id).data);
      if (!struct_decl) {
        continue;
      }

      result.stage.structs.push_back(lower_struct_decl(global_id));
    }

    const auto used_uniforms_it = m_link_result.uniform_usage.find(m_stage);
    const std::unordered_set<std::string> empty_uniforms;
    const auto &used_uniforms =
        used_uniforms_it != m_link_result.uniform_usage.end()
            ? used_uniforms_it->second
            : empty_uniforms;

    for (NodeID global_id : m_program.globals) {
      const ASTNode &global_node = node(global_id);

      if (std::holds_alternative<StructDecl>(global_node.data)) {
        continue;
      }

      if (const auto *func_decl = std::get_if<FuncDecl>(&global_node.data)) {
        if (stage_function_ids.count(global_id) > 0) {
          result.stage.declarations.emplace_back(lower_function_decl(global_id));
        }
        continue;
      }

      if (const auto *var_decl = std::get_if<VarDecl>(&global_node.data)) {
        result.stage.declarations.emplace_back(lower_global_const_decl(
            global_node.location, *var_decl));
        continue;
      }

      if (const auto *interface_decl =
              std::get_if<InlineInterfaceDecl>(&global_node.data)) {
        if (emits_as_storage_block(*interface_decl) ||
            should_emit_interface(interface_decl->is_in, m_stage)) {
          result.stage.declarations.emplace_back(
              lower_inline_interface_decl(global_id));
        }
        continue;
      }

      if (const auto *interface_ref =
              std::get_if<InterfaceRef>(&global_node.data)) {
        if (should_emit_interface(interface_ref->is_in, m_stage)) {
          auto lowered = lower_interface_ref_decl(global_id);
          if (lowered) {
            result.stage.declarations.emplace_back(std::move(*lowered));
          }
        }
        continue;
      }

      if (const auto *uniform_decl = std::get_if<UniformDecl>(&global_node.data)) {
        if (used_uniforms.count(uniform_decl->name) > 0 &&
            !is_shadowed(uniform_decl->name)) {
          result.stage.declarations.emplace_back(
              lower_uniform_decl(global_id, *uniform_decl));
        }
        continue;
      }

      if (const auto *buffer_decl = std::get_if<BufferDecl>(&global_node.data)) {
        if (buffer_decl->instance_name &&
            used_uniforms.count(*buffer_decl->instance_name) > 0 &&
            !is_shadowed(*buffer_decl->instance_name)) {
          result.stage.declarations.emplace_back(
              lower_buffer_decl(global_id, *buffer_decl));
        }
      }
    }

    result.stage.entry = lower_entry(*entry, entry_ctx);
    return result;
  }

private:
  const ASTNode &node(NodeID id) const { return m_nodes[id]; }

  const FuncDecl *find_stage_entry() const {
    for (NodeID stage_id : m_program.stages) {
      const auto *func_decl = std::get_if<FuncDecl>(&node(stage_id).data);
      if (!func_decl || !func_decl->stage_kind ||
          *func_decl->stage_kind != m_stage) {
        continue;
      }

      return func_decl;
    }

    return nullptr;
  }

  const InterfaceDecl *find_interface(const std::string &name) const {
    for (NodeID global_id : m_program.globals) {
      if (const auto *interface_decl =
              std::get_if<InterfaceDecl>(&node(global_id).data)) {
        if (interface_decl->name == name) {
          return interface_decl;
        }
      }
    }

    return nullptr;
  }

  bool has_uniform_annotation(const Annotations &annotations) const {
    for (const auto &annotation : annotations) {
      if (annotation.kind == AnnotationKind::Uniform) {
        return true;
      }
    }

    return false;
  }

  bool interface_has_uniform_fields(const InterfaceDecl &interface_decl) const {
    if (interface_decl.role == InterfaceRole::Uniform) {
      return true;
    }

    for (NodeID field_id : interface_decl.fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      if (has_uniform_annotation(field_decl.annotations)) {
        return true;
      }
    }

    return false;
  }

  bool is_shadowed(const std::string &name) const {
    for (NodeID stage_id : m_program.stages) {
      const auto *func_decl = std::get_if<FuncDecl>(&node(stage_id).data);
      if (!func_decl || !func_decl->stage_kind ||
          *func_decl->stage_kind != m_stage) {
        continue;
      }

      for (NodeID param_id : func_decl->params) {
        const auto &param_decl = std::get<ParamDecl>(node(param_id).data);
        if (param_decl.name == name) {
          return true;
        }
      }
    }

    return false;
  }

  EntryContext build_entry_context(const FuncDecl &entry) const {
    EntryContext ctx;
    ctx.stage = m_stage;

    const InterfaceDecl *declared_output_interface = nullptr;
    if (entry.ret.kind == TokenKind::Identifier) {
      declared_output_interface = find_interface(entry.ret.name);
      ctx.output_interface = declared_output_interface;
    }

    if (!ctx.output_interface) {
      if (m_stage == StageKind::Vertex) {
        ctx.output_interface = find_interface("VertexOutput");
      } else if (m_stage == StageKind::Fragment) {
        ctx.output_interface = find_interface("FragmentOutput");
      }
    }

    if (declared_output_interface) {
      ctx.output_local_name =
          find_output_local_name(m_nodes, entry.body, declared_output_interface->name);
    }

    std::unordered_set<std::string> resource_param_names;
    for (NodeID param_id : entry.params) {
      const auto &param_decl = std::get<ParamDecl>(node(param_id).data);
      const InterfaceDecl *interface_decl = find_interface(param_decl.type.name);
      if (!interface_decl) {
        continue;
      }

      if (interface_has_uniform_fields(*interface_decl)) {
        ctx.resource_inputs.emplace(param_decl.name, interface_decl);
        resource_param_names.insert(param_decl.name);
      } else {
        ctx.varying_inputs.emplace(param_decl.name, interface_decl);
      }
    }

    if (!resource_param_names.empty()) {
      collect_resource_field_usage(m_nodes, entry.body, resource_param_names,
                                   ctx.used_resource_fields);
    }

    return ctx;
  }

  std::unordered_set<NodeID>
  collect_stage_struct_ids(const FuncDecl *entry, const EntryContext &ctx,
                           const std::unordered_set<NodeID> &stage_function_ids) const {
    std::unordered_set<NodeID> stage_struct_ids;
    auto struct_ids = collect_global_struct_ids(m_program, m_nodes);

    auto collect_interface_structs = [&](const InterfaceDecl *interface_decl,
                                         const std::string *param_name) {
      if (!interface_decl) {
        return;
      }

      for (NodeID field_id : interface_decl->fields) {
        if (param_name) {
          const auto &field_decl = std::get<FieldDecl>(m_nodes[field_id].data);
          if (ctx.used_resource_fields.count(*param_name + "." + field_decl.name) ==
              0) {
            continue;
          }
        }

        collect_struct_dependencies_from_node(field_id, struct_ids, m_nodes,
                                              stage_struct_ids);
      }
    };

    if (entry) {
      collect_struct_dependencies_from_type(entry->ret, struct_ids, m_nodes,
                                            stage_struct_ids);

      for (NodeID param_id : entry->params) {
        const auto &param_decl = std::get<ParamDecl>(node(param_id).data);
        collect_struct_dependencies_from_type(param_decl.type, struct_ids, m_nodes,
                                              stage_struct_ids);
      }

      collect_struct_dependencies_from_node(entry->body, struct_ids, m_nodes,
                                            stage_struct_ids);
    }

    collect_interface_structs(ctx.output_interface, nullptr);

    for (const auto &[param_name, interface_decl] : ctx.varying_inputs) {
      (void)param_name;
      collect_interface_structs(interface_decl, nullptr);
    }

    for (const auto &[param_name, interface_decl] : ctx.resource_inputs) {
      collect_interface_structs(interface_decl, &param_name);
    }

    for (NodeID function_id : stage_function_ids) {
      collect_struct_dependencies_from_node(function_id, struct_ids, m_nodes,
                                            stage_struct_ids);
    }

    const auto used_uniforms_it = m_link_result.uniform_usage.find(m_stage);
    const std::unordered_set<std::string> empty_uniforms;
    const auto &used_uniforms =
        used_uniforms_it != m_link_result.uniform_usage.end()
            ? used_uniforms_it->second
            : empty_uniforms;

    for (NodeID global_id : m_program.globals) {
      const ASTNode &global_node = node(global_id);

      if (std::holds_alternative<VarDecl>(global_node.data)) {
        collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                              stage_struct_ids);
        continue;
      }

      if (const auto *interface_decl =
              std::get_if<InlineInterfaceDecl>(&global_node.data)) {
        if (emits_as_storage_block(*interface_decl) ||
            should_emit_interface(interface_decl->is_in, m_stage)) {
          collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                                stage_struct_ids);
        }
        continue;
      }

      if (const auto *interface_ref =
              std::get_if<InterfaceRef>(&global_node.data)) {
        if (!should_emit_interface(interface_ref->is_in, m_stage)) {
          continue;
        }

        collect_interface_structs(find_interface(interface_ref->block_name),
                                  nullptr);
        continue;
      }

      if (const auto *uniform_decl = std::get_if<UniformDecl>(&global_node.data)) {
        if (used_uniforms.count(uniform_decl->name) > 0 &&
            !is_shadowed(uniform_decl->name)) {
          collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                                stage_struct_ids);
        }
        continue;
      }

      if (const auto *buffer_decl = std::get_if<BufferDecl>(&global_node.data)) {
        if (buffer_decl->instance_name &&
            used_uniforms.count(*buffer_decl->instance_name) > 0 &&
            !is_shadowed(*buffer_decl->instance_name)) {
          collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                                stage_struct_ids);
        }
      }
    }

    return stage_struct_ids;
  }

  CanonicalFieldDecl lower_field_decl(NodeID field_id,
                                      const EntryContext *ctx = nullptr) const {
    const ASTNode &field_node = node(field_id);
    const auto &field_decl = std::get<FieldDecl>(field_node.data);

    CanonicalFieldDecl lowered;
    lowered.location = field_node.location;
    lowered.type = field_decl.type;
    lowered.name = field_decl.name;
    lowered.array_size = field_decl.array_size;
    lowered.annotations = field_decl.annotations;

    if (field_decl.init) {
      lowered.init = lower_expr(*field_decl.init, ctx);
    }

    return lowered;
  }

  std::vector<CanonicalFieldDecl>
  lower_interface_fields(const InterfaceDecl &interface_decl,
                         const EntryContext *ctx = nullptr,
                         std::string_view param_name = {}) const {
    std::vector<CanonicalFieldDecl> fields;

    for (NodeID field_id : interface_decl.fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      if (!param_name.empty() &&
          ctx->used_resource_fields.count(std::string(param_name) + "." +
                                          field_decl.name) == 0) {
        continue;
      }

      fields.push_back(lower_field_decl(field_id, ctx));
    }

    return fields;
  }

  CanonicalStructDecl lower_struct_decl(NodeID id) const {
    const ASTNode &struct_node = node(id);
    const auto &struct_decl = std::get<StructDecl>(struct_node.data);

    CanonicalStructDecl lowered;
    lowered.location = struct_node.location;
    lowered.name = struct_decl.name;

    for (NodeID field_id : struct_decl.fields) {
      lowered.fields.push_back(lower_field_decl(field_id));
    }

    return lowered;
  }

  CanonicalGlobalConstDecl lower_global_const_decl(SourceLocation location,
                                                   const VarDecl &var_decl) const {
    CanonicalGlobalConstDecl lowered;
    lowered.location = location;
    lowered.type = var_decl.type;
    lowered.name = var_decl.name;
    lowered.is_const = var_decl.is_const;
    if (var_decl.init) {
      lowered.init = lower_expr(*var_decl.init, nullptr);
    }
    return lowered;
  }

  CanonicalUniformDecl lower_uniform_decl(NodeID id,
                                          const UniformDecl &uniform_decl) const {
    CanonicalUniformDecl lowered;
    lowered.location = node(id).location;
    lowered.type = uniform_decl.type;
    lowered.name = uniform_decl.name;
    lowered.annotations = uniform_decl.annotations;
    lowered.array_size = uniform_decl.array_size;
    if (uniform_decl.default_val) {
      lowered.default_val = lower_expr(*uniform_decl.default_val, nullptr);
    }
    return lowered;
  }

  CanonicalBufferDecl lower_buffer_decl(NodeID id,
                                        const BufferDecl &buffer_decl) const {
    CanonicalBufferDecl lowered;
    lowered.location = node(id).location;
    lowered.name = buffer_decl.name;
    lowered.annotations = buffer_decl.annotations;
    lowered.instance_name = buffer_decl.instance_name;
    lowered.is_uniform = buffer_decl.is_uniform;

    for (NodeID field_id : buffer_decl.fields) {
      lowered.fields.push_back(lower_field_decl(field_id));
    }

    return lowered;
  }

  CanonicalInterfaceBlockDecl lower_inline_interface_decl(NodeID id) const {
    const ASTNode &interface_node = node(id);
    const auto &interface_decl = std::get<InlineInterfaceDecl>(interface_node.data);

    CanonicalInterfaceBlockDecl lowered;
    lowered.location = interface_node.location;
    lowered.is_in = interface_decl.is_in;
    lowered.is_storage_block = emits_as_storage_block(interface_decl);
    lowered.name = interface_decl.name;
    lowered.instance_name = interface_decl.instance_name;
    lowered.annotations = interface_decl.annotations;

    for (NodeID field_id : interface_decl.fields) {
      lowered.fields.push_back(lower_field_decl(field_id));
    }

    return lowered;
  }

  std::optional<CanonicalInterfaceBlockDecl>
  lower_interface_ref_decl(NodeID id) const {
    const ASTNode &interface_node = node(id);
    const auto &interface_ref = std::get<InterfaceRef>(interface_node.data);
    const InterfaceDecl *interface_decl = find_interface(interface_ref.block_name);
    if (!interface_decl) {
      return std::nullopt;
    }

    CanonicalInterfaceBlockDecl lowered;
    lowered.location = interface_node.location;
    lowered.is_in = interface_ref.is_in;
    lowered.is_storage_block = false;
    lowered.name = interface_decl->name;
    lowered.instance_name = interface_ref.instance_name;
    lowered.annotations = {};
    lowered.fields = lower_interface_fields(*interface_decl);
    return lowered;
  }

  CanonicalParamDecl lower_param_decl(NodeID id) const {
    const ASTNode &param_node = node(id);
    const auto &param_decl = std::get<ParamDecl>(param_node.data);

    CanonicalParamDecl lowered;
    lowered.location = param_node.location;
    lowered.type = param_decl.type;
    lowered.name = param_decl.name;
    lowered.qual = param_decl.qual;
    return lowered;
  }

  CanonicalFunctionDecl lower_function_decl(NodeID id) const {
    const ASTNode &func_node = node(id);
    const auto &func_decl = std::get<FuncDecl>(func_node.data);

    CanonicalFunctionDecl lowered;
    lowered.location = func_node.location;
    lowered.ret = func_decl.ret;
    lowered.name = func_decl.name;

    for (NodeID param_id : func_decl.params) {
      lowered.params.push_back(lower_param_decl(param_id));
    }

    lowered.body = lower_stmt(func_decl.body, nullptr);
    return lowered;
  }

  CanonicalEntryPoint lower_entry(const FuncDecl &entry,
                                  const EntryContext &ctx) const {
    CanonicalEntryPoint lowered;
    lowered.location = node(entry.body).location;
    lowered.stage = m_stage;
    lowered.ret = entry.ret;
    lowered.name = entry.name;

    for (NodeID param_id : entry.params) {
      const auto &param_decl = std::get<ParamDecl>(node(param_id).data);
      auto varying_it = ctx.varying_inputs.find(param_decl.name);
      if (varying_it != ctx.varying_inputs.end()) {
        lowered.varying_inputs.push_back(CanonicalInterfaceBinding{
            node(param_id).location, param_decl.name, varying_it->second->name,
            lower_interface_fields(*varying_it->second, &ctx),
            varying_it->second->role});
        continue;
      }

      auto resource_it = ctx.resource_inputs.find(param_decl.name);
      if (resource_it != ctx.resource_inputs.end()) {
        auto fields = lower_interface_fields(*resource_it->second, &ctx,
                                             param_decl.name);
        if (!fields.empty()) {
          lowered.resource_inputs.push_back(CanonicalInterfaceBinding{
              node(param_id).location, param_decl.name, resource_it->second->name,
              std::move(fields), resource_it->second->role});
        }
      }
    }

    if (ctx.output_interface) {
      lowered.output = CanonicalStageOutput{
          node(entry.body).location, ctx.output_interface->name,
          lower_interface_fields(*ctx.output_interface, &ctx),
          ctx.output_local_name};
    }

    std::vector<CanonicalStmtPtr> body_stmts;
    if (lowered.output) {
      for (const auto &field : lowered.output->fields) {
        if (!field.init) {
          continue;
        }

        body_stmts.push_back(make_stmt(
            field.location,
            CanonicalOutputAssignStmt{
                field.name,
                lower_expr_from_canonical(*field.init),
                TokenKind::Eq}));
      }
    }

    const ASTNode &body_node = node(entry.body);
    if (const auto *block_stmt = std::get_if<BlockStmt>(&body_node.data)) {
      for (NodeID stmt_id : block_stmt->stmts) {
        if (auto stmt = lower_stmt(stmt_id, &ctx)) {
          body_stmts.push_back(std::move(stmt));
        }
      }
    } else if (auto stmt = lower_stmt(entry.body, &ctx)) {
      body_stmts.push_back(std::move(stmt));
    }

    lowered.body =
        make_stmt(body_node.location, CanonicalBlockStmt{std::move(body_stmts)});
    return lowered;
  }

  CanonicalExprPtr lower_expr(NodeID id, const EntryContext *ctx) const {
    const ASTNode &expr_node = node(id);
    const auto expr_emitter = Overloaded{
        [&](const LiteralExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalLiteralExpr{expr.value});
        },
        [&](const IdentifierExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalIdentifierExpr{expr.name});
        },
        [&](const UnresolvedRef &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalIdentifierExpr{expr.name});
        },
        [&](const BinaryExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalBinaryExpr{lower_expr(expr.lhs, ctx),
                                               lower_expr(expr.rhs, ctx), expr.op});
        },
        [&](const UnaryExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalUnaryExpr{lower_expr(expr.operand, ctx),
                                              expr.op, expr.prefix});
        },
        [&](const TernaryExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalTernaryExpr{lower_expr(expr.cond, ctx),
                                                lower_expr(expr.then_expr, ctx),
                                                lower_expr(expr.else_expr, ctx)});
        },
        [&](const CallExpr &expr) -> CanonicalExprPtr {
          CanonicalCallExpr lowered;
          lowered.callee = lower_expr(expr.callee, ctx);
          for (NodeID arg_id : expr.args) {
            lowered.args.push_back(lower_expr(arg_id, ctx));
          }
          return make_expr(expr_node.location, expr_node.type, std::move(lowered));
        },
        [&](const IndexExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalIndexExpr{lower_expr(expr.array, ctx),
                                              lower_expr(expr.index, ctx)});
        },
        [&](const FieldExpr &expr) -> CanonicalExprPtr {
          if (ctx) {
            if (const auto *identifier_expr =
                    std::get_if<IdentifierExpr>(&node(expr.object).data)) {
              if (ctx->output_local_name &&
                  identifier_expr->name == *ctx->output_local_name) {
                return make_expr(expr_node.location, expr_node.type,
                                 CanonicalOutputFieldRef{expr.field});
              }

              if (ctx->resource_inputs.count(identifier_expr->name) > 0) {
                return make_expr(expr_node.location, expr_node.type,
                                 CanonicalStageResourceFieldRef{
                                     identifier_expr->name, expr.field});
              }

              if (ctx->varying_inputs.count(identifier_expr->name) > 0) {
                return make_expr(expr_node.location, expr_node.type,
                                 CanonicalStageInputFieldRef{
                                     identifier_expr->name, expr.field});
              }
            }
          }

          return make_expr(expr_node.location, expr_node.type,
                           CanonicalFieldExpr{lower_expr(expr.object, ctx),
                                              expr.field});
        },
        [&](const ConstructExpr &expr) -> CanonicalExprPtr {
          CanonicalConstructExpr lowered;
          lowered.type = expr.type;
          for (NodeID arg_id : expr.args) {
            lowered.args.push_back(lower_expr(arg_id, ctx));
          }
          return make_expr(expr_node.location, expr_node.type, std::move(lowered));
        },
        [&](const AssignExpr &expr) -> CanonicalExprPtr {
          return make_expr(expr_node.location, expr_node.type,
                           CanonicalAssignExpr{lower_expr(expr.lhs, ctx),
                                               lower_expr(expr.rhs, ctx), expr.op});
        },
        [&](const auto &) -> CanonicalExprPtr { return {}; }};

    return std::visit(expr_emitter, expr_node.data);
  }

  CanonicalExprPtr lower_expr_from_canonical(const CanonicalExpr &expr) const {
    const auto expr_emitter = Overloaded{
        [&](const CanonicalLiteralExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type, CanonicalLiteralExpr{data.value});
        },
        [&](const CanonicalIdentifierExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalIdentifierExpr{data.name});
        },
        [&](const CanonicalStageInputFieldRef &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalStageInputFieldRef{data.param_name, data.field});
        },
        [&](const CanonicalStageResourceFieldRef &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalStageResourceFieldRef{data.param_name,
                                                          data.field});
        },
        [&](const CanonicalOutputFieldRef &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalOutputFieldRef{data.field});
        },
        [&](const CanonicalBinaryExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalBinaryExpr{lower_expr_from_canonical(*data.lhs),
                                               lower_expr_from_canonical(*data.rhs),
                                               data.op});
        },
        [&](const CanonicalUnaryExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalUnaryExpr{
                               lower_expr_from_canonical(*data.operand), data.op,
                               data.prefix});
        },
        [&](const CanonicalTernaryExpr &data) -> CanonicalExprPtr {
          return make_expr(
              expr.location, expr.type,
              CanonicalTernaryExpr{lower_expr_from_canonical(*data.cond),
                                   lower_expr_from_canonical(*data.then_expr),
                                   lower_expr_from_canonical(*data.else_expr)});
        },
        [&](const CanonicalCallExpr &data) -> CanonicalExprPtr {
          CanonicalCallExpr lowered;
          lowered.callee = lower_expr_from_canonical(*data.callee);
          for (const auto &arg : data.args) {
            lowered.args.push_back(lower_expr_from_canonical(*arg));
          }
          return make_expr(expr.location, expr.type, std::move(lowered));
        },
        [&](const CanonicalIndexExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalIndexExpr{
                               lower_expr_from_canonical(*data.array),
                               lower_expr_from_canonical(*data.index)});
        },
        [&](const CanonicalFieldExpr &data) -> CanonicalExprPtr {
          return make_expr(expr.location, expr.type,
                           CanonicalFieldExpr{
                               lower_expr_from_canonical(*data.object), data.field});
        },
        [&](const CanonicalConstructExpr &data) -> CanonicalExprPtr {
          CanonicalConstructExpr lowered;
          lowered.type = data.type;
          for (const auto &arg : data.args) {
            lowered.args.push_back(lower_expr_from_canonical(*arg));
          }
          return make_expr(expr.location, expr.type, std::move(lowered));
        },
        [&](const CanonicalAssignExpr &data) -> CanonicalExprPtr {
          return make_expr(
              expr.location, expr.type,
              CanonicalAssignExpr{lower_expr_from_canonical(*data.lhs),
                                  lower_expr_from_canonical(*data.rhs), data.op});
        }};

    return std::visit(expr_emitter, expr.data);
  }

  CanonicalStmtPtr lower_body_stmt(NodeID id, const EntryContext *ctx) const {
    if (auto stmt = lower_stmt(id, ctx)) {
      return stmt;
    }

    return make_stmt(node(id).location, CanonicalBlockStmt{});
  }

  CanonicalStmtPtr lower_for_init(NodeID id, const EntryContext *ctx) const {
    const ASTNode &init_node = node(id);
    if (const auto *decl_stmt = std::get_if<DeclStmt>(&init_node.data)) {
      return lower_stmt(decl_stmt->decl, ctx);
    }

    return lower_stmt(id, ctx);
  }

  CanonicalStmtPtr try_lower_structured_return(const ReturnStmt &stmt,
                                               SourceLocation location,
                                               const EntryContext *ctx) const {
    if (!ctx || !ctx->output_interface || !stmt.value) {
      return nullptr;
    }

    const ASTNode &return_node = node(*stmt.value);

    if (const auto *identifier_expr = std::get_if<IdentifierExpr>(&return_node.data)) {
      if (ctx->output_local_name &&
          identifier_expr->name == *ctx->output_local_name) {
        return make_stmt(location, CanonicalReturnStmt{});
      }
    }

    const std::vector<NodeID> *args = nullptr;

    if (const auto *construct_expr = std::get_if<ConstructExpr>(&return_node.data)) {
      if (construct_expr->type.name == ctx->output_interface->name) {
        args = &construct_expr->args;
      }
    } else if (const auto *call_expr = std::get_if<CallExpr>(&return_node.data)) {
      const ASTNode &callee_node = node(call_expr->callee);

      if (const auto *identifier_expr =
              std::get_if<IdentifierExpr>(&callee_node.data)) {
        if (identifier_expr->name == ctx->output_interface->name) {
          args = &call_expr->args;
        }
      } else if (const auto *unresolved_ref =
                     std::get_if<UnresolvedRef>(&callee_node.data)) {
        if (unresolved_ref->name == ctx->output_interface->name) {
          args = &call_expr->args;
        }
      }
    }

    if (!args || args->size() > ctx->output_interface->fields.size()) {
      return nullptr;
    }

    std::vector<CanonicalStmtPtr> stmts;
    for (size_t i = 0; i < args->size(); ++i) {
      const auto &field_decl =
          std::get<FieldDecl>(node(ctx->output_interface->fields[i]).data);
      stmts.push_back(make_stmt(
          location, CanonicalOutputAssignStmt{
                        field_decl.name, lower_expr((*args)[i], ctx),
                        TokenKind::Eq}));
    }
    stmts.push_back(make_stmt(location, CanonicalReturnStmt{}));

    if (stmts.size() == 1) {
      return std::move(stmts.front());
    }

    return make_stmt(location, CanonicalBlockStmt{std::move(stmts)});
  }

  CanonicalStmtPtr lower_stmt(NodeID id, const EntryContext *ctx) const {
    const ASTNode &stmt_node = node(id);
    const auto stmt_emitter = Overloaded{
        [&](const BlockStmt &stmt) -> CanonicalStmtPtr {
          std::vector<CanonicalStmtPtr> stmts;
          for (NodeID stmt_id : stmt.stmts) {
            if (auto lowered = lower_stmt(stmt_id, ctx)) {
              stmts.push_back(std::move(lowered));
            }
          }
          return make_stmt(stmt_node.location,
                           CanonicalBlockStmt{std::move(stmts)});
        },
        [&](const IfStmt &stmt) -> CanonicalStmtPtr {
          return make_stmt(stmt_node.location,
                           CanonicalIfStmt{lower_expr(stmt.cond, ctx),
                                           lower_body_stmt(stmt.then_br, ctx),
                                           stmt.else_br
                                               ? lower_body_stmt(*stmt.else_br, ctx)
                                               : nullptr});
        },
        [&](const ForStmt &stmt) -> CanonicalStmtPtr {
          return make_stmt(
              stmt_node.location,
              CanonicalForStmt{lower_for_init(stmt.init, ctx),
                               stmt.cond ? lower_expr(*stmt.cond, ctx) : nullptr,
                               stmt.step ? lower_expr(*stmt.step, ctx) : nullptr,
                               lower_body_stmt(stmt.body, ctx)});
        },
        [&](const WhileStmt &stmt) -> CanonicalStmtPtr {
          return make_stmt(stmt_node.location,
                           CanonicalWhileStmt{lower_expr(stmt.cond, ctx),
                                              lower_body_stmt(stmt.body, ctx)});
        },
        [&](const ReturnStmt &stmt) -> CanonicalStmtPtr {
          if (auto lowered = try_lower_structured_return(stmt, stmt_node.location,
                                                         ctx)) {
            return lowered;
          }

          return make_stmt(stmt_node.location,
                           CanonicalReturnStmt{
                               stmt.value ? lower_expr(*stmt.value, ctx) : nullptr});
        },
        [&](const ExprStmt &stmt) -> CanonicalStmtPtr {
          CanonicalExprPtr expr = lower_expr(stmt.expr, ctx);
          if (!expr) {
            return nullptr;
          }

          if (auto *assign_expr = std::get_if<CanonicalAssignExpr>(&expr->data)) {
            if (assign_expr->lhs) {
              if (const auto *output_field =
                      std::get_if<CanonicalOutputFieldRef>(
                          &assign_expr->lhs->data)) {
                return make_stmt(stmt_node.location,
                                 CanonicalOutputAssignStmt{
                                     output_field->field,
                                     std::move(assign_expr->rhs),
                                     assign_expr->op});
              }
            }
          }

          return make_stmt(stmt_node.location,
                           CanonicalExprStmt{std::move(expr)});
        },
        [&](const DeclStmt &stmt) -> CanonicalStmtPtr {
          return lower_stmt(stmt.decl, ctx);
        },
        [&](const VarDecl &stmt) -> CanonicalStmtPtr {
          if (ctx && ctx->output_interface && ctx->output_local_name &&
              stmt.type.kind == TokenKind::Identifier &&
              stmt.type.name == ctx->output_interface->name &&
              stmt.name == *ctx->output_local_name) {
            return nullptr;
          }

          return make_stmt(stmt_node.location,
                           CanonicalVarDeclStmt{
                               stmt.type, stmt.name,
                               stmt.init ? lower_expr(*stmt.init, ctx) : nullptr,
                               stmt.is_const});
        },
        [&](const BreakStmt &) -> CanonicalStmtPtr {
          return make_stmt(stmt_node.location, CanonicalBreakStmt{});
        },
        [&](const ContinueStmt &) -> CanonicalStmtPtr {
          return make_stmt(stmt_node.location, CanonicalContinueStmt{});
        },
        [&](const DiscardStmt &) -> CanonicalStmtPtr {
          return make_stmt(stmt_node.location, CanonicalDiscardStmt{});
        },
        [&](const auto &) -> CanonicalStmtPtr { return nullptr; }};

    return std::visit(stmt_emitter, stmt_node.data);
  }

  const Program &m_program;
  const LinkResult &m_link_result;
  StageKind m_stage;
  const std::vector<ASTNode> &m_nodes;
};

} // namespace

CanonicalLowering::CanonicalLowering(const std::vector<ASTNode> &nodes)
    : m_nodes(nodes) {}

CanonicalLoweringResult CanonicalLowering::lower(const Program &program,
                                                 const LinkResult &link_result,
                                                 StageKind stage) const {
  CanonicalStageBuilder builder(program, link_result, stage);
  return builder.lower();
}

} // namespace astralix
