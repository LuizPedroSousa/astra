#include "shader-lang/emitters/opengl-glsl-emitter.hpp"
#include <cstdio>

namespace astralix {

namespace {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

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
    const std::vector<ASTNode> &nodes, std::unordered_set<NodeID> &struct_usage) {
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
    const std::vector<ASTNode> &nodes, std::unordered_set<NodeID> &struct_usage) {
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

} // namespace

OpenGLGLSLEmitter::OpenGLGLSLEmitter(const std::vector<ASTNode> &nodes)
    : m_nodes(nodes) {}

void OpenGLGLSLEmitter::write(std::string_view text) { m_out += text; }

void OpenGLGLSLEmitter::writeln(std::string_view text) {
  m_out += m_indent;
  m_out += text;
  m_out += '\n';
}

void OpenGLGLSLEmitter::push_indent() { m_indent += "  "; }

void OpenGLGLSLEmitter::pop_indent() {
  if (m_indent.size() >= 2)
    m_indent.resize(m_indent.size() - 2);
}

const ASTNode &OpenGLGLSLEmitter::node(NodeID id) const { return m_nodes[id]; }

std::string
OpenGLGLSLEmitter::emit(const Program &program, StageKind stage,
                        const std::unordered_set<std::string> &used_uniforms) {
  m_out.clear();
  m_indent.clear();

  writeln("#version 450 core");
  writeln();

  const FuncDecl *entry = find_stage_entry(program, stage);
  EntryContext ctx{stage};
  const EntryContext *ctx_ptr = nullptr;
  if (entry) {
    ctx = build_entry_context(program, stage, *entry);
    ctx_ptr = &ctx;
  }

  emit_globals(program, stage, used_uniforms, entry, ctx_ptr);

  const auto stage_resource_emitter = Overloaded{
      [&](const UniformDecl &decl) {
        emit_stage_resource_if_used(decl, stage, program, used_uniforms);
      },
      [&](const BufferDecl &decl) {
        emit_stage_resource_if_used(decl, stage, program, used_uniforms);
      },
      [&](const auto &) {}};

  for (NodeID gid : program.globals) {
    std::visit(stage_resource_emitter, node(gid).data);
  }

  if (!entry) {
    return m_out;
  }

  emit_entry_signature(ctx);
  emit_entry_main(*entry, ctx);

  return m_out;
}

void OpenGLGLSLEmitter::emit_globals(
    const Program &prog, StageKind stage,
    const std::unordered_set<std::string> &used_uniforms, const FuncDecl *entry,
    const EntryContext *ctx) {
  std::unordered_set<NodeID> stage_function_ids =
      collect_stage_function_ids(prog, m_nodes, entry);
  std::unordered_set<NodeID> stage_struct_ids;
  auto struct_ids = collect_global_struct_ids(prog, m_nodes);

  auto collect_interface_structs = [&](const InterfaceDecl *interface_decl,
                                       const std::string *param_name) {
    if (!interface_decl) {
      return;
    }

    for (NodeID field_id : interface_decl->fields) {
      if (ctx && param_name) {
        const auto &field_decl = std::get<FieldDecl>(m_nodes[field_id].data);
        std::string key = *param_name + "." + field_decl.name;
        if (ctx->resource_aliases.count(key) == 0 &&
            ctx->used_resource_fields.count(key) == 0) {
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

  if (ctx) {
    collect_interface_structs(ctx->output_interface, nullptr);

    for (const auto &[param_name, interface_decl] : ctx->varying_inputs) {
      (void)param_name;
      collect_interface_structs(interface_decl, nullptr);
    }

    for (const auto &[param_name, interface_decl] : ctx->resource_inputs) {
      collect_interface_structs(interface_decl, &param_name);
    }
  }

  for (NodeID function_id : stage_function_ids) {
    collect_struct_dependencies_from_node(function_id, struct_ids, m_nodes,
                                          stage_struct_ids);
  }

  for (NodeID global_id : prog.globals) {
    const ASTNode &global_node = node(global_id);

    if (std::holds_alternative<VarDecl>(global_node.data)) {
      collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                            stage_struct_ids);
      continue;
    }

    if (const auto *interface_decl =
            std::get_if<InlineInterfaceDecl>(&global_node.data)) {
      if (emits_as_storage_block(*interface_decl) ||
          should_emit_interface(interface_decl->is_in, stage)) {
        collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                              stage_struct_ids);
      }
      continue;
    }

    if (const auto *interface_ref =
            std::get_if<InterfaceRef>(&global_node.data)) {
      if (!should_emit_interface(interface_ref->is_in, stage)) {
        continue;
      }

      collect_interface_structs(find_interface(prog, interface_ref->block_name),
                                nullptr);
      continue;
    }

    if (const auto *uniform_decl = std::get_if<UniformDecl>(&global_node.data)) {
      if (used_uniforms.count(uniform_decl->name) &&
          !is_shadowed(uniform_decl->name, stage, prog)) {
        collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                              stage_struct_ids);
      }
      continue;
    }

    if (const auto *buffer_decl = std::get_if<BufferDecl>(&global_node.data)) {
      if (buffer_decl->instance_name &&
          used_uniforms.count(*buffer_decl->instance_name) &&
          !is_shadowed(*buffer_decl->instance_name, stage, prog)) {
        collect_struct_dependencies_from_node(global_id, struct_ids, m_nodes,
                                              stage_struct_ids);
      }
    }
  }

  const auto struct_emitter = Overloaded{
      [&](const StructDecl &decl, NodeID global_id) {
        if (stage_struct_ids.count(global_id) == 0) {
          return;
        }

        emit_struct(decl);
      },
      [&](const auto &, NodeID) {}};

  bool emitted_function_prototype = false;
  const auto function_prototype_emitter = Overloaded{
      [&](const FuncDecl &decl, NodeID global_id) {
        if (stage_function_ids.count(global_id) == 0) {
          return;
        }

        emit_function_prototype(decl);
        emitted_function_prototype = true;
      },
      [&](const auto &, NodeID) {}};

  const auto non_function_decl_emitter =
      Overloaded{[&](const VarDecl &decl) { emit_global_var(decl); },
                 [&](const InlineInterfaceDecl &decl) {
                   emit_global_inline_interface(decl, stage);
                 },
                 [&](const InterfaceRef &decl) {
                   emit_global_interface_ref(decl, prog, stage);
                 },
                 [&](const auto &) {}};

  const auto function_definition_emitter = Overloaded{
      [&](const FuncDecl &decl, NodeID global_id) {
        if (stage_function_ids.count(global_id) == 0) {
          return;
        }

        emit_function(decl);
      },
      [&](const auto &, NodeID) {}};

  for (NodeID global_id : prog.globals) {
    std::visit([&](const auto &decl) { struct_emitter(decl, global_id); },
               node(global_id).data);
  }

  for (NodeID global_id : prog.globals) {
    std::visit(
        [&](const auto &decl) { function_prototype_emitter(decl, global_id); },
        node(global_id).data);
  }

  if (emitted_function_prototype) {
    writeln();
  }

  for (NodeID global_id : prog.globals) {
    std::visit(non_function_decl_emitter, node(global_id).data);
  }

  for (NodeID global_id : prog.globals) {
    std::visit(
        [&](const auto &decl) { function_definition_emitter(decl, global_id); },
        node(global_id).data);
  }
}

void OpenGLGLSLEmitter::emit_global_var(const VarDecl &var_decl) {
  write(m_indent + "const " + type_str(var_decl.type) + " " + var_decl.name);
  if (var_decl.init) {
    write(" = ");
    emit_expr(*var_decl.init);
  }
  write(";\n");
}

void OpenGLGLSLEmitter::emit_global_inline_interface(
    const InlineInterfaceDecl &interface_decl, StageKind stage) {
  if (emits_as_storage_block(interface_decl)) {
    emit_buffer(BufferDecl{interface_decl.name, interface_decl.fields,
                           interface_decl.annotations,
                           interface_decl.instance_name, false});
    return;
  }

  if (should_emit_interface(interface_decl.is_in, stage)) {
    emit_interface_block(interface_decl.is_in, interface_decl.name,
                         interface_decl.fields, interface_decl.instance_name);
  }
}

void OpenGLGLSLEmitter::emit_global_interface_ref(
    const InterfaceRef &interface_ref, const Program &program,
    StageKind stage) {
  if (!should_emit_interface(interface_ref.is_in, stage)) {
    return;
  }

  if (const InterfaceDecl *interface =
          find_interface(program, interface_ref.block_name)) {
    emit_interface_block(interface_ref.is_in, interface->name,
                         interface->fields, interface_ref.instance_name);
  }
}

void OpenGLGLSLEmitter::emit_stage_resource_if_used(
    const UniformDecl &uniform_decl, StageKind stage, const Program &program,
    const std::unordered_set<std::string> &used_uniforms) {

  if (used_uniforms.count(uniform_decl.name) &&
      !is_shadowed(uniform_decl.name, stage, program)) {
    emit_uniform(uniform_decl);
  }
}

void OpenGLGLSLEmitter::emit_stage_resource_if_used(
    const BufferDecl &buffer_decl, StageKind stage, const Program &program,
    const std::unordered_set<std::string> &used_uniforms) {
  if (buffer_decl.instance_name &&
      used_uniforms.count(*buffer_decl.instance_name) &&
      !is_shadowed(*buffer_decl.instance_name, stage, program)) {
    emit_buffer(buffer_decl);
  }
}

const FuncDecl *OpenGLGLSLEmitter::find_stage_entry(const Program &program,
                                                    StageKind stage) const {
  for (NodeID stage_id : program.stages) {
    const auto *func_decl = std::get_if<FuncDecl>(&node(stage_id).data);
    if (!func_decl || !func_decl->stage_kind ||
        *func_decl->stage_kind != stage) {
      continue;
    }

    return func_decl;
  }

  return nullptr;
}

bool OpenGLGLSLEmitter::has_uniform_annotation(const Annotations &annotations) {
  for (const auto &annotation : annotations) {
    if (annotation.kind == AnnotationKind::Uniform) {
      return true;
    }
  }

  return false;
}

bool OpenGLGLSLEmitter::interface_has_uniform_fields(
    const InterfaceDecl &interface) const {
  if (interface.role == InterfaceRole::Uniform) {
    return true;
  }

  for (NodeID field_id : interface.fields) {
    const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
    if (has_uniform_annotation(field_decl.annotations)) {
      return true;
    }
  }

  return false;
}

OpenGLGLSLEmitter::EntryContext
OpenGLGLSLEmitter::build_entry_context(const Program &program, StageKind stage,
                                       const FuncDecl &entry) const {
  EntryContext ctx{stage};
  const InterfaceDecl *declared_output_interface = nullptr;
  std::unordered_set<std::string> resource_param_names;
  std::unordered_set<std::string> used_resource_names;
  std::unordered_set<std::string> used_input_names;

  if (entry.ret.kind == TokenKind::Identifier) {
    declared_output_interface = find_interface(program, entry.ret.name);
    ctx.output_interface = declared_output_interface;
  }

  if (!ctx.output_interface) {
    if (stage == StageKind::Vertex) {
      ctx.output_interface = find_interface(program, "VertexOutput");
    } else if (stage == StageKind::Fragment) {
      ctx.output_interface = find_interface(program, "FragmentOutput");
    }
  }

  if (ctx.output_interface && stage == StageKind::Fragment) {
    std::unordered_set<std::string> used_output_names;

    for (NodeID field_id : ctx.output_interface->fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      std::string alias = "_out_" + field_decl.name;

      if (!used_output_names.insert(alias).second) {
        int suffix = 0;
        while (!used_output_names.insert(alias).second) {
          alias = "_out_" + field_decl.name + "_" + std::to_string(++suffix);
        }
      }

      ctx.output_aliases[field_decl.name] = alias;
    }
  }

  if (declared_output_interface) {
    ctx.output_local_name = find_output_local_name(
        m_nodes, entry.body, declared_output_interface->name);

    if (ctx.output_local_name && stage == StageKind::Vertex) {
      ctx.output_instance_name = *ctx.output_local_name;
    }

    if (ctx.output_local_name) {
      for (NodeID field_id : ctx.output_interface->fields) {
        const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
        auto alias_it = ctx.output_aliases.find(field_decl.name);
        std::string alias =
            alias_it != ctx.output_aliases.end()
                ? alias_it->second
                : ctx.output_instance_name + "." + field_decl.name;
        ctx.field_aliases[*ctx.output_local_name + "." + field_decl.name] =
            alias;
      }
    }
  }

  for (NodeID param_id : entry.params) {
    const auto &param_decl = std::get<ParamDecl>(node(param_id).data);
    const InterfaceDecl *interface =
        find_interface(program, param_decl.type.name);
    if (!interface) {
      continue;
    }

    if (interface_has_uniform_fields(*interface)) {
      ctx.resource_inputs.push_back({param_decl.name, interface});
      resource_param_names.insert(param_decl.name);
    } else {
      ctx.varying_inputs.push_back({param_decl.name, interface});
    }
  }

  if (!resource_param_names.empty()) {
    collect_resource_field_usage(m_nodes, entry.body, resource_param_names,
                                 ctx.used_resource_fields);
  }

  for (const auto &[param_name, interface] : ctx.resource_inputs) {
    for (NodeID field_id : interface->fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      std::string key = param_name + "." + field_decl.name;
      if (ctx.used_resource_fields.count(key) == 0) {
        continue;
      }

      std::string alias = "_" + field_decl.name;

      if (!used_resource_names.insert(alias).second) {
        alias = "_" + param_name + "_" + field_decl.name;
        int suffix = 0;
        while (!used_resource_names.insert(alias).second) {
          alias = "_" + param_name + "_" + field_decl.name + "_" +
                  std::to_string(++suffix);
        }
      }

      ctx.field_aliases[key] = alias;
      ctx.resource_aliases[key] = alias;
    }
  }

  if (stage == StageKind::Vertex) {
    for (const auto &[param_name, interface] : ctx.varying_inputs) {
      for (NodeID field_id : interface->fields) {
        const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
        std::string alias = field_decl.name;

        if (!used_input_names.insert(alias).second) {
          alias = param_name + "_" + field_decl.name;
          int suffix = 0;
          while (!used_input_names.insert(alias).second) {
            alias = param_name + "_" + field_decl.name + "_" +
                    std::to_string(++suffix);
          }
        }

        ctx.field_aliases[param_name + "." + field_decl.name] = alias;
      }
    }
  }

  return ctx;
}

void OpenGLGLSLEmitter::emit_entry_signature(const EntryContext &ctx) {
  for (const auto &[param_name, interface] : ctx.varying_inputs) {
    if (ctx.stage == StageKind::Vertex) {
      for (NodeID field_id : interface->fields) {
        const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
        auto alias_it =
            ctx.field_aliases.find(param_name + "." + field_decl.name);
        if (alias_it == ctx.field_aliases.end()) {
          continue;
        }

        std::string layout = layout_quals(field_decl.annotations);
        std::string text = (layout.empty() ? "" : layout + " ") + "in " +
                           type_str(field_decl.type) + " " + alias_it->second;

        if (field_decl.array_size) {
          text += *field_decl.array_size == 0
                      ? "[]"
                      : "[" + std::to_string(*field_decl.array_size) + "]";
        }

        writeln(text + ";");
      }
      writeln();
    } else {
      emit_interface_block(true, interface->name, interface->fields,
                           param_name);
    }
  }

  if (ctx.output_interface && ctx.stage == StageKind::Fragment) {
    for (NodeID field_id : ctx.output_interface->fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      auto alias_it = ctx.output_aliases.find(field_decl.name);
      if (alias_it == ctx.output_aliases.end()) {
        continue;
      }

      std::string layout = layout_quals(field_decl.annotations);
      std::string text = (layout.empty() ? "" : layout + " ") + "out " +
                         type_str(field_decl.type) + " " + alias_it->second;

      if (field_decl.array_size) {
        text += *field_decl.array_size == 0
                    ? "[]"
                    : "[" + std::to_string(*field_decl.array_size) + "]";
      }

      writeln(text + ";");
    }
    writeln();
  } else if (ctx.output_interface) {
    emit_interface_block(false, ctx.output_interface->name,
                         ctx.output_interface->fields,
                         ctx.output_instance_name);
  }

  for (const auto &[param_name, interface] : ctx.resource_inputs) {
    for (NodeID field_id : interface->fields) {
      const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
      std::string key = param_name + "." + field_decl.name;
      if (ctx.used_resource_fields.count(key) == 0) {
        continue;
      }

      auto alias_it =
          ctx.resource_aliases.find(key);
      if (alias_it == ctx.resource_aliases.end()) {
        continue;
      }

      emit_uniform(UniformDecl{field_decl.type, alias_it->second,
                               field_decl.annotations, field_decl.init,
                               field_decl.array_size});
    }
  }
}

void OpenGLGLSLEmitter::emit_entry_main(const FuncDecl &entry,
                                        const EntryContext &ctx) {
  write(m_indent + "void main() ");

  const auto *body_block = std::get_if<BlockStmt>(&node(entry.body).data);
  if (!body_block) {
    emit_stmt(entry.body, &ctx);
    writeln();
    return;
  }

  write("{\n");
  push_indent();
  emit_output_field_initializers(ctx);

  for (NodeID stmt_id : body_block->stmts) {
    emit_stmt(stmt_id, &ctx);
  }

  pop_indent();
  writeln("}");
  writeln();
}

void OpenGLGLSLEmitter::emit_struct(const StructDecl &struct_decl) {
  writeln("struct " + struct_decl.name + " {");
  push_indent();
  for (NodeID fid : struct_decl.fields)
    emit_field(std::get<FieldDecl>(node(fid).data));
  pop_indent();
  writeln("};");
  writeln();
}

void OpenGLGLSLEmitter::emit_uniform(const UniformDecl &uniform_decl) {
  std::string layout = layout_quals(uniform_decl.annotations);
  std::string prefix = layout.empty() ? "" : layout + " ";

  std::string array = uniform_decl.array_size
                          ? "[" + std::to_string(*uniform_decl.array_size) + "]"
                          : "";

  write(m_indent + prefix + "uniform " + type_str(uniform_decl.type) + " " +
        uniform_decl.name + array);

  if (uniform_decl.default_val) {
    write(" = ");
    emit_expr(*uniform_decl.default_val);
  }

  write(";\n");
}

void OpenGLGLSLEmitter::emit_buffer(const BufferDecl &buffer_decl) {
  std::string layout = layout_quals(buffer_decl.annotations);
  std::string prefix = layout.empty() ? "" : layout + " ";
  std::string keyword = buffer_decl.is_uniform ? "uniform" : "buffer";
  writeln(prefix + keyword + " " + buffer_decl.name + " {");
  push_indent();

  for (NodeID fid : buffer_decl.fields) {
    emit_field(std::get<FieldDecl>(node(fid).data), false);
  }

  pop_indent();
  writeln("} " + buffer_decl.instance_name.value_or("") + ";");
  writeln();
}

void OpenGLGLSLEmitter::emit_interface_block(
    bool is_in, const std::string &block_name,
    const std::vector<NodeID> &fields,
    const std::optional<std::string> &instance) {
  writeln(std::string(is_in ? "in " : "out ") + block_name + " {");
  push_indent();

  for (NodeID fid : fields) {
    emit_field(std::get<FieldDecl>(node(fid).data), false);
  }

  pop_indent();
  writeln("} " + instance.value_or("") + ";");
  writeln();
}

void OpenGLGLSLEmitter::emit_field(const FieldDecl &field_decl,
                                   bool emit_initializer) {
  std::string layout = layout_quals(field_decl.annotations);
  write(m_indent);

  if (!layout.empty()) {
    write(layout + " ");
  }

  write(type_str(field_decl.type) + " " + field_decl.name);

  if (field_decl.array_size) {
    write(*field_decl.array_size == 0
              ? "[]"
              : "[" + std::to_string(*field_decl.array_size) + "]");
  }

  if (emit_initializer && field_decl.init.has_value()) {
    write(" = ");
    emit_expr(*field_decl.init);
  }

  write(";\n");
}

void OpenGLGLSLEmitter::emit_function_prototype(const FuncDecl &func_decl) {
  write(m_indent + type_str(func_decl.ret) + " " + func_decl.name + "(");

  for (size_t i = 0; i < func_decl.params.size(); ++i) {
    if (i) {
      write(", ");
    }

    emit_param(func_decl.params[i]);
  }

  write(");\n");
}

void OpenGLGLSLEmitter::emit_function(const FuncDecl &func_decl) {
  write(m_indent + type_str(func_decl.ret) + " " + func_decl.name + "(");

  for (size_t i = 0; i < func_decl.params.size(); ++i) {
    if (i) {
      write(", ");
    }

    emit_param(func_decl.params[i]);
  }

  write(") ");
  emit_stmt(func_decl.body, nullptr);
  writeln();
}

void OpenGLGLSLEmitter::emit_param(NodeID id) {
  const auto &param_decl = std::get<ParamDecl>(node(id).data);

  if (param_decl.qual != ParamQualifier::None) {
    write(std::string(param_qual_str(param_decl.qual)) + " ");
  }

  write(type_str(param_decl.type) + " " + param_decl.name);
}

void OpenGLGLSLEmitter::emit_var_decl_stmt(const VarDecl &var_decl,
                                           const EntryContext *ctx) {
  if (ctx && ctx->output_local_name && ctx->output_interface &&
      var_decl.type.kind == TokenKind::Identifier &&
      var_decl.type.name == ctx->output_interface->name &&
      var_decl.name == *ctx->output_local_name) {
    return;
  }

  write(m_indent);
  if (var_decl.is_const)
    write("const ");
  write(type_str(var_decl.type) + " " + var_decl.name);
  if (var_decl.init) {
    write(" = ");
    emit_expr(*var_decl.init, ctx);
  }
  write(";\n");
}

void OpenGLGLSLEmitter::emit_var_decl_for_init(const VarDecl &var_decl,
                                               const EntryContext *ctx) {
  if (ctx && ctx->output_local_name && ctx->output_interface &&
      var_decl.type.kind == TokenKind::Identifier &&
      var_decl.type.name == ctx->output_interface->name &&
      var_decl.name == *ctx->output_local_name) {
    return;
  }

  write(type_str(var_decl.type) + " " + var_decl.name);
  if (var_decl.init) {
    write(" = ");
    emit_expr(*var_decl.init, ctx);
  }
}

void OpenGLGLSLEmitter::emit_output_field_initializers(
    const EntryContext &ctx) {
  if (!ctx.output_interface) {
    return;
  }

  for (NodeID field_id : ctx.output_interface->fields) {
    const auto &field_decl = std::get<FieldDecl>(node(field_id).data);
    if (!field_decl.init) {
      continue;
    }

    auto alias_it = ctx.output_aliases.find(field_decl.name);
    std::string output_target =
        alias_it != ctx.output_aliases.end()
            ? alias_it->second
            : ctx.output_instance_name + "." + field_decl.name;

    write(m_indent + output_target + " = ");
    emit_expr(*field_decl.init, &ctx);
    write(";\n");
  }
}

const VarDecl *OpenGLGLSLEmitter::find_var_decl(NodeID id) const {
  const ASTNode *current_node = &node(id);
  if (const auto *decl_stmt = std::get_if<DeclStmt>(&current_node->data)) {
    current_node = &node(decl_stmt->decl);
  }

  return std::get_if<VarDecl>(&current_node->data);
}

bool OpenGLGLSLEmitter::try_emit_output_fields(const std::vector<NodeID> &args,
                                               const EntryContext &ctx) {
  if (!ctx.output_interface ||
      args.size() > ctx.output_interface->fields.size()) {
    return false;
  }

  for (size_t i = 0; i < ctx.output_interface->fields.size(); ++i) {
    const auto &field_decl =
        std::get<FieldDecl>(node(ctx.output_interface->fields[i]).data);
    if (i < args.size()) {
      auto alias_it = ctx.output_aliases.find(field_decl.name);
      std::string output_target =
          alias_it != ctx.output_aliases.end()
              ? alias_it->second
              : ctx.output_instance_name + "." + field_decl.name;

      write(m_indent + output_target + " = ");
      emit_expr(args[i], &ctx);
      write(";\n");
      continue;
    }

    if (!field_decl.init) {
      return false;
    }
  }

  write(m_indent + "return;\n");
  return true;
}

bool OpenGLGLSLEmitter::try_emit_structured_return(const ReturnStmt &stmt,
                                                   const EntryContext *ctx) {
  if (!ctx || !ctx->output_interface || !stmt.value) {
    return false;
  }

  const auto &return_node = node(*stmt.value);

  if (const auto *identifier_expr = std::get_if<IdentifierExpr>(&return_node.data)) {
    if (ctx->output_local_name &&
        identifier_expr->name == *ctx->output_local_name) {
      write(m_indent + "return;\n");
      return true;
    }
  }

  if (const auto *construct_expr =
          std::get_if<ConstructExpr>(&return_node.data)) {
    return ctx->output_interface->name == construct_expr->type.name &&
           try_emit_output_fields(construct_expr->args, *ctx);
  }

  if (const auto *call_expr = std::get_if<CallExpr>(&return_node.data)) {
    const auto &callee_node = node(call_expr->callee);
    if (const auto *callee_ident =
            std::get_if<IdentifierExpr>(&callee_node.data)) {
      return callee_ident->name == ctx->output_interface->name &&
             try_emit_output_fields(call_expr->args, *ctx);
    }

    if (const auto *callee_ref =
            std::get_if<UnresolvedRef>(&callee_node.data)) {
      return callee_ref->name == ctx->output_interface->name &&
             try_emit_output_fields(call_expr->args, *ctx);
    }
  }

  return false;
}

void OpenGLGLSLEmitter::emit_stmt_node(const BlockStmt &stmt,
                                       const EntryContext *ctx) {
  write("{\n");
  push_indent();

  for (NodeID sid : stmt.stmts) {
    emit_stmt(sid, ctx);
  }

  pop_indent();
  write(m_indent + "}\n");
}

void OpenGLGLSLEmitter::emit_stmt_node(const IfStmt &stmt,
                                       const EntryContext *ctx) {
  write(m_indent + "if (");
  emit_expr(stmt.cond, ctx);
  write(") ");
  emit_body_stmt(stmt.then_br, ctx);

  if (stmt.else_br) {
    write(m_indent + "else ");
    emit_body_stmt(*stmt.else_br, ctx);
  }
}

void OpenGLGLSLEmitter::emit_stmt_node(const ForStmt &stmt,
                                       const EntryContext *ctx) {
  write(m_indent + "for (");
  emit_for_init(stmt.init, ctx);
  write(" ");

  if (stmt.cond) {
    emit_expr(*stmt.cond, ctx);
  }

  write("; ");

  if (stmt.step) {
    emit_expr(*stmt.step, ctx);
  }

  write(") ");
  emit_body_stmt(stmt.body, ctx);
}

void OpenGLGLSLEmitter::emit_stmt_node(const WhileStmt &stmt,
                                       const EntryContext *ctx) {
  write(m_indent + "while (");
  emit_expr(stmt.cond, ctx);
  write(") ");
  emit_body_stmt(stmt.body, ctx);
}

void OpenGLGLSLEmitter::emit_stmt_node(const ReturnStmt &stmt,
                                       const EntryContext *ctx) {
  if (try_emit_structured_return(stmt, ctx)) {
    return;
  }

  write(m_indent + "return");
  if (stmt.value) {
    write(" ");
    emit_expr(*stmt.value, ctx);
  }
  write(";\n");
}

void OpenGLGLSLEmitter::emit_stmt_node(const ExprStmt &stmt,
                                       const EntryContext *ctx) {
  write(m_indent);
  emit_expr(stmt.expr, ctx);
  write(";\n");
}

void OpenGLGLSLEmitter::emit_stmt_node(const DeclStmt &stmt,
                                       const EntryContext *ctx) {
  emit_var_decl_stmt(std::get<VarDecl>(node(stmt.decl).data), ctx);
}

void OpenGLGLSLEmitter::emit_stmt_node(const VarDecl &stmt,
                                       const EntryContext *ctx) {
  emit_var_decl_stmt(stmt, ctx);
}

void OpenGLGLSLEmitter::emit_stmt_node(const BreakStmt &,
                                       const EntryContext *) {
  writeln("break;");
}

void OpenGLGLSLEmitter::emit_stmt_node(const ContinueStmt &,
                                       const EntryContext *) {
  writeln("continue;");
}

void OpenGLGLSLEmitter::emit_stmt_node(const DiscardStmt &,
                                       const EntryContext *) {
  writeln("discard;");
}

void OpenGLGLSLEmitter::emit_body_stmt(NodeID id, const EntryContext *ctx) {
  if (std::holds_alternative<BlockStmt>(node(id).data)) {
    emit_stmt(id, ctx);
  } else {
    write("{\n");
    push_indent();
    emit_stmt(id, ctx);
    pop_indent();
    write(m_indent + "}\n");
  }
}

void OpenGLGLSLEmitter::emit_for_init(NodeID id, const EntryContext *ctx) {
  if (const VarDecl *var_decl = find_var_decl(id)) {
    emit_var_decl_for_init(*var_decl, ctx);
    write(";");
    return;
  }

  if (const auto *expr_stmt = std::get_if<ExprStmt>(&node(id).data)) {
    emit_expr(expr_stmt->expr, ctx);
    write(";");
    return;
  }

  write(";");
}

void OpenGLGLSLEmitter::emit_stmt(NodeID id, const EntryContext *ctx) {
  const auto stmt_emitter =
      Overloaded{[&](const BlockStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const IfStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const ForStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const WhileStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const ReturnStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const ExprStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const DeclStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const VarDecl &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const BreakStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const ContinueStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const DiscardStmt &stmt) { emit_stmt_node(stmt, ctx); },
                 [&](const auto &) {}};

  std::visit(stmt_emitter, node(id).data);
}

void OpenGLGLSLEmitter::emit_expr_paren(NodeID id, const EntryContext *ctx) {
  bool needs = std::holds_alternative<BinaryExpr>(node(id).data) ||
               std::holds_alternative<TernaryExpr>(node(id).data);
  if (needs)
    write("(");
  emit_expr(id, ctx);
  if (needs)
    write(")");
}

void OpenGLGLSLEmitter::emit_literal(const LiteralExpr &expr) {
  const auto literal_emitter = Overloaded{
      [&](bool value) { write(value ? "true" : "false"); },
      [&](int64_t value) { write(std::to_string(value)); },
      [&](double value) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
        write(buf);
      }};

  std::visit(literal_emitter, expr.value);
}

void OpenGLGLSLEmitter::emit_call_args(const std::vector<NodeID> &args,
                                       const EntryContext *ctx) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (i)
      write(", ");
    emit_expr(args[i], ctx);
  }
}

bool OpenGLGLSLEmitter::try_emit_field_alias(const FieldExpr &expr,
                                             const EntryContext *ctx) {
  if (!ctx) {
    return false;
  }

  if (const auto *identifier_expr =
          std::get_if<IdentifierExpr>(&node(expr.object).data)) {
    auto alias_it =
        ctx->field_aliases.find(identifier_expr->name + "." + expr.field);
    if (alias_it != ctx->field_aliases.end()) {
      write(alias_it->second);
      return true;
    }
  }

  return false;
}

void OpenGLGLSLEmitter::emit_expr_node(const LiteralExpr &expr,
                                       const EntryContext *) {
  emit_literal(expr);
}

void OpenGLGLSLEmitter::emit_expr_node(const IdentifierExpr &expr,
                                       const EntryContext *) {
  write(expr.name);
}

void OpenGLGLSLEmitter::emit_expr_node(const UnresolvedRef &expr,
                                       const EntryContext *) {
  write(expr.name);
}

void OpenGLGLSLEmitter::emit_expr_node(const BinaryExpr &expr,
                                       const EntryContext *ctx) {
  emit_expr_paren(expr.lhs, ctx);
  write(" ");
  write(op_str(expr.op));
  write(" ");
  emit_expr_paren(expr.rhs, ctx);
}

void OpenGLGLSLEmitter::emit_expr_node(const UnaryExpr &expr,
                                       const EntryContext *ctx) {
  if (expr.prefix) {
    write(op_str(expr.op));
    emit_expr(expr.operand, ctx);
  } else {
    emit_expr(expr.operand, ctx);
    write(op_str(expr.op));
  }
}

void OpenGLGLSLEmitter::emit_expr_node(const TernaryExpr &expr,
                                       const EntryContext *ctx) {
  emit_expr(expr.cond, ctx);
  write(" ? ");
  emit_expr(expr.then_expr, ctx);
  write(" : ");
  emit_expr(expr.else_expr, ctx);
}

void OpenGLGLSLEmitter::emit_expr_node(const CallExpr &expr,
                                       const EntryContext *ctx) {
  emit_expr(expr.callee, ctx);
  write("(");
  emit_call_args(expr.args, ctx);
  write(")");
}

void OpenGLGLSLEmitter::emit_expr_node(const IndexExpr &expr,
                                       const EntryContext *ctx) {
  emit_expr(expr.array, ctx);
  write("[");
  emit_expr(expr.index, ctx);
  write("]");
}

void OpenGLGLSLEmitter::emit_expr_node(const FieldExpr &expr,
                                       const EntryContext *ctx) {
  if (try_emit_field_alias(expr, ctx)) {
    return;
  }

  emit_expr(expr.object, ctx);
  write(".");
  write(expr.field);
}

void OpenGLGLSLEmitter::emit_expr_node(const ConstructExpr &expr,
                                       const EntryContext *ctx) {
  write(type_str(expr.type));
  write("(");
  emit_call_args(expr.args, ctx);
  write(")");
}

void OpenGLGLSLEmitter::emit_expr_node(const AssignExpr &expr,
                                       const EntryContext *ctx) {
  emit_expr(expr.lhs, ctx);
  write(" ");
  write(op_str(expr.op));
  write(" ");
  emit_expr(expr.rhs, ctx);
}

void OpenGLGLSLEmitter::emit_expr(NodeID id, const EntryContext *ctx) {
  const auto expr_emitter =
      Overloaded{[&](const LiteralExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const IdentifierExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const UnresolvedRef &expr) { emit_expr_node(expr, ctx); },
                 [&](const BinaryExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const UnaryExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const TernaryExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const CallExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const IndexExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const FieldExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const ConstructExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const AssignExpr &expr) { emit_expr_node(expr, ctx); },
                 [&](const auto &) {}};

  std::visit(expr_emitter, node(id).data);
}

std::string
OpenGLGLSLEmitter::layout_quals(const Annotations &annotations) const {
  std::string parts;
  auto append = [&](std::string s) {
    if (!parts.empty())
      parts += ", ";
    parts += std::move(s);
  };

  for (const auto &annotation : annotations) {
    switch (annotation.kind) {
      case AnnotationKind::Location:
        if (annotation.slot >= 0)
          append("location = " + std::to_string(annotation.slot));
        break;
      case AnnotationKind::Binding:
        if (annotation.slot >= 0)
          append("binding = " + std::to_string(annotation.slot));
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

const InterfaceDecl *
OpenGLGLSLEmitter::find_interface(const Program &program,
                                  const std::string &name) const {
  for (NodeID gid : program.globals) {
    if (const auto *id = std::get_if<InterfaceDecl>(&node(gid).data)) {
      if (id->name == name) {
        return id;
      }
    }
  }

  return nullptr;
}

bool OpenGLGLSLEmitter::is_shadowed(const std::string &name, StageKind kind,
                                    const Program &program) const {
  for (NodeID stage_id : program.stages) {
    const auto *func_decl = std::get_if<FuncDecl>(&node(stage_id).data);
    if (!func_decl || !func_decl->stage_kind ||
        *func_decl->stage_kind != kind) {
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

} // namespace astralix
