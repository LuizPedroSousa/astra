#include "shader-lang/lowering/glsl-lowering.hpp"

#include <unordered_map>
#include <unordered_set>

namespace astralix {

namespace {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <class T>
GLSLExprPtr make_expr(SourceLocation location, const TypeRef &type, T data) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->location = location;
  expr->type = type;
  expr->data = std::move(data);
  return expr;
}

template <class T> GLSLStmtPtr make_stmt(SourceLocation location, T data) {
  auto stmt = std::make_unique<GLSLStmt>();
  stmt->location = location;
  stmt->data = std::move(data);
  return stmt;
}

class GLSLStageBuilder {
public:
  struct EntryContext {
    StageKind stage = StageKind::Vertex;
    std::string output_instance_name = "_stage_out";
    const CanonicalStageOutput *output = nullptr;
    std::unordered_map<std::string, std::string> field_aliases;
    std::unordered_map<std::string, std::string> output_aliases;
    std::unordered_map<std::string, std::string> resource_aliases;
  };

  GLSLStageBuilder(const CanonicalStage &stage,
                   const StageReflection &reflection)
      : m_stage(stage), m_reflection(reflection) {}

  GlslLoweringResult lower() const {
    GlslLoweringResult result;
    result.stage.version = m_stage.version;
    result.stage.stage = m_stage.stage;
    result.stage.local_size = m_stage.local_size;

    EntryContext entry_ctx = build_entry_context();

    for (const auto &struct_decl : m_stage.structs) {
      result.stage.declarations.emplace_back(lower_struct_decl(struct_decl));
    }

    for (const auto &decl : m_stage.declarations) {
      if (const auto *function_decl =
              std::get_if<CanonicalFunctionDecl>(&decl)) {
        result.stage.declarations.emplace_back(
            lower_function_decl(*function_decl, nullptr, true));
      }
    }

    for (const auto &decl : m_stage.declarations) {
      if (std::holds_alternative<CanonicalFunctionDecl>(decl)) {
        continue;
      }

      result.stage.declarations.push_back(lower_global_decl(decl));
    }

    for (const auto &decl : m_stage.declarations) {
      if (const auto *function_decl =
              std::get_if<CanonicalFunctionDecl>(&decl)) {
        result.stage.declarations.emplace_back(
            lower_function_decl(*function_decl, nullptr, false));
      }
    }

    append_entry_signature(result.stage.declarations, entry_ctx);
    result.stage.declarations.emplace_back(
        lower_entry_function(m_stage.entry, entry_ctx));
    result.reflection = lower_reflection(entry_ctx);
    return result;
  }

private:
  StageReflection lower_reflection(const EntryContext &ctx) const {
    StageReflection reflection = m_reflection;

    for (auto &input : reflection.stage_inputs) {
      if (ctx.stage == StageKind::Vertex) {
        auto alias_it = ctx.field_aliases.find(input.logical_name);
        input.glsl.emitted_name =
            alias_it != ctx.field_aliases.end()
                ? std::optional<std::string>(alias_it->second)
                : std::optional<std::string>(input.logical_name);
      } else {
        input.glsl.emitted_name = input.logical_name;
      }

      input.glsl.storage = "in";
    }

    for (auto &output : reflection.stage_outputs) {
      if (ctx.stage == StageKind::Fragment) {
        auto alias_it = ctx.output_aliases.find(output.logical_name);
        if (alias_it != ctx.output_aliases.end()) {
          output.glsl.emitted_name = alias_it->second;
        }
      } else {
        output.glsl.emitted_name =
            ctx.output_instance_name + "." + output.logical_name;
      }

      output.glsl.storage = "out";
    }

    for (auto &resource : reflection.resources) {
      lower_resource_reflection(resource, ctx);
    }

    return reflection;
  }

  void lower_resource_reflection(ResourceReflection &resource,
                                 const EntryContext &ctx) const {
    switch (resource.kind) {
      case ShaderResourceKind::UniformInterface:
        resource.glsl.storage = "uniform";
        for (auto &member : resource.members) {
          member.glsl.storage = "uniform";
          member.glsl.emitted_name = lower_resource_member_name(
              resource.logical_name, member.logical_name, ctx);
        }
        return;

      case ShaderResourceKind::UniformBlock:
        resource.glsl.storage = "uniform";
        if (!resource.glsl.emitted_name) {
          resource.glsl.emitted_name = resource.logical_name;
        }
        for (auto &member : resource.members) {
          member.glsl.storage = "uniform";
          member.glsl.emitted_name = member.logical_name;
        }
        return;

      case ShaderResourceKind::StorageBuffer:
        resource.glsl.storage = "buffer";
        if (!resource.glsl.emitted_name) {
          resource.glsl.emitted_name = resource.logical_name;
        }
        for (auto &member : resource.members) {
          member.glsl.storage = "buffer";
          member.glsl.emitted_name = member.logical_name;
        }
        return;

      case ShaderResourceKind::Sampler:
      case ShaderResourceKind::UniformValue:
        resource.glsl.storage = "uniform";
        if (!resource.glsl.emitted_name) {
          resource.glsl.emitted_name = resource.logical_name;
        }
        for (auto &member : resource.members) {
          member.glsl.storage = "uniform";
          member.glsl.emitted_name = member.logical_name;
        }
        return;
    }
  }

  std::string lower_resource_member_name(const std::string &resource_name,
                                         const std::string &member_name,
                                         const EntryContext &ctx) const {
    const std::string prefix = resource_name + ".";
    if (member_name.rfind(prefix, 0) != 0) {
      return member_name;
    }

    const std::string tail = member_name.substr(prefix.size());
    const size_t boundary = tail.find_first_of(".[");
    const std::string direct_field =
        boundary == std::string::npos ? tail : tail.substr(0, boundary);
    const auto alias_it =
        ctx.resource_aliases.find(resource_name + "." + direct_field);
    if (alias_it == ctx.resource_aliases.end()) {
      return member_name;
    }

    const std::string suffix =
        boundary == std::string::npos ? std::string{} : tail.substr(boundary);
    return alias_it->second + suffix;
  }

  EntryContext build_entry_context() const {
    EntryContext ctx;
    ctx.stage = m_stage.stage;

    if (m_stage.entry.output) {
      ctx.output = &*m_stage.entry.output;

      if (ctx.stage == StageKind::Vertex && ctx.output->sink_name) {
        ctx.output_instance_name = *ctx.output->sink_name;
      }

      if (ctx.stage == StageKind::Fragment) {
        std::unordered_set<std::string> used_output_names;

        for (const auto &field : ctx.output->fields) {
          std::string alias = "_out_" + field.name;
          if (!used_output_names.insert(alias).second) {
            int suffix = 0;
            while (!used_output_names.insert(alias).second) {
              alias = "_out_" + field.name + "_" + std::to_string(++suffix);
            }
          }
          ctx.output_aliases.emplace(field.name, alias);
        }
      }

      if (ctx.output->sink_name) {
        for (const auto &field : ctx.output->fields) {
          auto output_it = ctx.output_aliases.find(field.name);
          std::string alias = output_it != ctx.output_aliases.end()
                                  ? output_it->second
                                  : ctx.output_instance_name + "." + field.name;
          ctx.field_aliases.emplace(*ctx.output->sink_name + "." + field.name,
                                    alias);
        }
      }
    }

    std::unordered_set<std::string> used_input_names;
    std::unordered_set<std::string> used_resource_names;

    if (ctx.stage == StageKind::Vertex) {
      for (const auto &binding : m_stage.entry.varying_inputs) {
        for (const auto &field : binding.fields) {
          std::string alias = field.name;
          if (!used_input_names.insert(alias).second) {
            alias = binding.param_name + "_" + field.name;
            int suffix = 0;
            while (!used_input_names.insert(alias).second) {
              alias = binding.param_name + "_" + field.name + "_" +
                      std::to_string(++suffix);
            }
          }

          ctx.field_aliases.emplace(binding.param_name + "." + field.name,
                                    alias);
        }
      }
    }

    for (const auto &binding : m_stage.entry.resource_inputs) {
      for (const auto &field : binding.fields) {
        std::string alias = "_" + field.name;
        if (!used_resource_names.insert(alias).second) {
          alias = "_" + binding.param_name + "_" + field.name;
          int suffix = 0;
          while (!used_resource_names.insert(alias).second) {
            alias = "_" + binding.param_name + "_" + field.name + "_" +
                    std::to_string(++suffix);
          }
        }

        const std::string key = binding.param_name + "." + field.name;
        ctx.field_aliases.emplace(key, alias);
        ctx.resource_aliases.emplace(key, alias);
      }
    }

    return ctx;
  }

  GLSLFieldDecl lower_field_decl(const CanonicalFieldDecl &field_decl,
                                 const EntryContext *ctx = nullptr) const {
    GLSLFieldDecl lowered;
    lowered.location = field_decl.location;
    lowered.type = field_decl.type;
    lowered.name = field_decl.name;
    lowered.array_size = field_decl.array_size;
    lowered.annotations = field_decl.annotations;
    if (field_decl.init) {
      lowered.init = lower_expr(*field_decl.init, ctx);
    }
    return lowered;
  }

  GLSLParamDecl lower_param_decl(const CanonicalParamDecl &param_decl) const {
    GLSLParamDecl lowered;
    lowered.location = param_decl.location;
    lowered.type = param_decl.type;
    lowered.name = param_decl.name;
    lowered.qual = param_decl.qual;
    return lowered;
  }

  GLSLStructDecl
  lower_struct_decl(const CanonicalStructDecl &struct_decl) const {
    GLSLStructDecl lowered;
    lowered.location = struct_decl.location;
    lowered.name = struct_decl.name;
    for (const auto &field : struct_decl.fields) {
      lowered.fields.push_back(lower_field_decl(field));
    }
    return lowered;
  }

  GLSLDecl lower_global_decl(const CanonicalDecl &decl) const {
    return std::visit(
        Overloaded{
            [&](const CanonicalGlobalConstDecl &global_decl) -> GLSLDecl {
              GLSLGlobalVarDecl lowered;
              lowered.location = global_decl.location;
              lowered.type = global_decl.type;
              lowered.name = global_decl.name;
              lowered.is_const = global_decl.is_const;
              if (global_decl.init) {
                lowered.init = lower_expr(*global_decl.init, nullptr);
              }
              return lowered;
            },
            [&](const CanonicalUniformDecl &uniform_decl) -> GLSLDecl {
              GLSLGlobalVarDecl lowered;
              lowered.location = uniform_decl.location;
              lowered.type = uniform_decl.type;
              lowered.name = uniform_decl.name;
              lowered.array_size = uniform_decl.array_size;
              lowered.annotations = uniform_decl.annotations;
              lowered.storage = "uniform";
              if (uniform_decl.default_val) {
                lowered.init = lower_expr(*uniform_decl.default_val, nullptr);
              }
              return lowered;
            },
            [&](const CanonicalBufferDecl &buffer_decl) -> GLSLDecl {
              GLSLInterfaceBlockDecl lowered;
              lowered.location = buffer_decl.location;
              lowered.storage = buffer_decl.is_uniform ? "uniform" : "buffer";
              lowered.block_name = buffer_decl.name;
              lowered.instance_name = buffer_decl.instance_name;
              lowered.annotations = buffer_decl.annotations;
              for (const auto &field : buffer_decl.fields) {
                lowered.fields.push_back(lower_field_decl(field));
              }
              return lowered;
            },
            [&](const CanonicalInterfaceBlockDecl &interface_decl) -> GLSLDecl {
              GLSLInterfaceBlockDecl lowered;
              lowered.location = interface_decl.location;
              lowered.storage = interface_decl.is_storage_block
                                    ? "buffer"
                                    : (interface_decl.is_in ? "in" : "out");
              lowered.block_name = interface_decl.name;
              lowered.instance_name = interface_decl.instance_name;
              lowered.annotations = interface_decl.annotations;
              for (const auto &field : interface_decl.fields) {
                lowered.fields.push_back(lower_field_decl(field));
              }
              return lowered;
            },
            [&](const CanonicalFunctionDecl &function_decl) -> GLSLDecl {
              return lower_function_decl(function_decl, nullptr, false);
            }},
        decl);
  }

  void append_entry_signature(std::vector<GLSLDecl> &decls,
                              const EntryContext &ctx) const {
    for (const auto &binding : m_stage.entry.varying_inputs) {
      if (ctx.stage == StageKind::Vertex) {
        for (const auto &field : binding.fields) {
          auto alias_it =
              ctx.field_aliases.find(binding.param_name + "." + field.name);
          if (alias_it == ctx.field_aliases.end()) {
            continue;
          }

          GLSLGlobalVarDecl lowered;
          lowered.location = field.location;
          lowered.type = field.type;
          lowered.name = alias_it->second;
          lowered.array_size = field.array_size;
          lowered.annotations = field.annotations;
          lowered.storage = "in";
          decls.emplace_back(std::move(lowered));
        }
      } else {
        GLSLInterfaceBlockDecl lowered;
        lowered.location = binding.location;
        lowered.storage = "in";
        lowered.block_name = binding.interface_name;
        lowered.instance_name = binding.param_name;
        for (const auto &field : binding.fields) {
          lowered.fields.push_back(lower_field_decl(field));
        }
        decls.emplace_back(std::move(lowered));
      }
    }

    if (ctx.output) {
      if (ctx.stage == StageKind::Fragment) {
        for (const auto &field : ctx.output->fields) {
          auto alias_it = ctx.output_aliases.find(field.name);
          if (alias_it == ctx.output_aliases.end()) {
            continue;
          }

          GLSLGlobalVarDecl lowered;
          lowered.location = field.location;
          lowered.type = field.type;
          lowered.name = alias_it->second;
          lowered.array_size = field.array_size;
          lowered.annotations = field.annotations;
          lowered.storage = "out";
          decls.emplace_back(std::move(lowered));
        }
      } else {
        GLSLInterfaceBlockDecl lowered;
        lowered.location = ctx.output->location;
        lowered.storage = "out";
        lowered.block_name = ctx.output->interface_name;
        lowered.instance_name = ctx.output_instance_name;
        for (const auto &field : ctx.output->fields) {
          lowered.fields.push_back(lower_field_decl(field));
        }
        decls.emplace_back(std::move(lowered));
      }
    }

    for (const auto &binding : m_stage.entry.resource_inputs) {
      for (const auto &field : binding.fields) {
        auto alias_it =
            ctx.resource_aliases.find(binding.param_name + "." + field.name);
        if (alias_it == ctx.resource_aliases.end()) {
          continue;
        }

        GLSLGlobalVarDecl lowered;
        lowered.location = field.location;
        lowered.type = field.type;
        lowered.name = alias_it->second;
        lowered.array_size = field.array_size;
        lowered.annotations = field.annotations;
        lowered.storage = "uniform";
        if (field.init) {
          lowered.init = lower_expr(*field.init, &ctx);
        }
        decls.emplace_back(std::move(lowered));
      }
    }
  }

  GLSLFunctionDecl
  lower_function_decl(const CanonicalFunctionDecl &function_decl,
                      const EntryContext *ctx, bool prototype_only) const {
    GLSLFunctionDecl lowered;
    lowered.location = function_decl.location;
    lowered.ret = function_decl.ret;
    lowered.name = function_decl.name;
    lowered.prototype_only = prototype_only;

    for (const auto &param : function_decl.params) {
      lowered.params.push_back(lower_param_decl(param));
    }

    if (!prototype_only && function_decl.body) {
      lowered.body = lower_stmt(*function_decl.body, ctx);
    }

    return lowered;
  }

  GLSLFunctionDecl lower_entry_function(const CanonicalEntryPoint &entry,
                                        const EntryContext &ctx) const {
    GLSLFunctionDecl lowered;
    lowered.location = entry.location;
    lowered.ret = TypeRef{TokenKind::KeywordVoid, "void"};
    lowered.name = "main";
    lowered.body = lower_stmt(*entry.body, &ctx);
    return lowered;
  }

  GLSLExprPtr lower_output_target(const std::string &field,
                                  const EntryContext &ctx,
                                  SourceLocation location,
                                  const TypeRef &type) const {
    if (ctx.stage == StageKind::Fragment) {
      auto alias_it = ctx.output_aliases.find(field);
      if (alias_it != ctx.output_aliases.end()) {
        return make_expr(location, type, GLSLIdentifierExpr{alias_it->second});
      }
    }

    return make_expr(
        location, type,
        GLSLFieldExpr{
            make_expr(location,
                      TypeRef{TokenKind::Identifier, ctx.output_instance_name},
                      GLSLIdentifierExpr{ctx.output_instance_name}),
            field});
  }

  GLSLExprPtr lower_expr(const CanonicalExpr &expr,
                         const EntryContext *ctx) const {
    const auto expr_emitter = Overloaded{
        [&](const CanonicalLiteralExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLLiteralExpr{data.value});
        },
        [&](const CanonicalIdentifierExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLIdentifierExpr{data.name});
        },
        [&](const CanonicalStageInputFieldRef &data) -> GLSLExprPtr {
          if (ctx && ctx->stage == StageKind::Vertex) {
            auto alias_it =
                ctx->field_aliases.find(data.param_name + "." + data.field);
            if (alias_it != ctx->field_aliases.end()) {
              return make_expr(expr.location, expr.type,
                               GLSLIdentifierExpr{alias_it->second});
            }
          }

          return make_expr(
              expr.location, expr.type,
              GLSLFieldExpr{
                  make_expr(expr.location,
                            TypeRef{TokenKind::Identifier, data.param_name},
                            GLSLIdentifierExpr{data.param_name}),
                  data.field});
        },
        [&](const CanonicalStageResourceFieldRef &data) -> GLSLExprPtr {
          if (ctx) {
            auto alias_it =
                ctx->resource_aliases.find(data.param_name + "." + data.field);
            if (alias_it != ctx->resource_aliases.end()) {
              return make_expr(expr.location, expr.type,
                               GLSLIdentifierExpr{alias_it->second});
            }
          }

          return make_expr(expr.location, expr.type,
                           GLSLIdentifierExpr{"_" + data.field});
        },
        [&](const CanonicalOutputFieldRef &data) -> GLSLExprPtr {
          if (ctx) {
            return lower_output_target(data.field, *ctx, expr.location,
                                       expr.type);
          }

          return make_expr(expr.location, expr.type,
                           GLSLIdentifierExpr{data.field});
        },
        [&](const CanonicalBinaryExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLBinaryExpr{lower_expr(*data.lhs, ctx),
                                          lower_expr(*data.rhs, ctx), data.op});
        },
        [&](const CanonicalUnaryExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLUnaryExpr{lower_expr(*data.operand, ctx),
                                         data.op, data.prefix});
        },
        [&](const CanonicalTernaryExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLTernaryExpr{lower_expr(*data.cond, ctx),
                                           lower_expr(*data.then_expr, ctx),
                                           lower_expr(*data.else_expr, ctx)});
        },
        [&](const CanonicalCallExpr &data) -> GLSLExprPtr {
          GLSLCallExpr lowered;
          lowered.callee = lower_expr(*data.callee, ctx);
          for (const auto &arg : data.args) {
            lowered.args.push_back(lower_expr(*arg, ctx));
          }
          return make_expr(expr.location, expr.type, std::move(lowered));
        },
        [&](const CanonicalIndexExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLIndexExpr{lower_expr(*data.array, ctx),
                                         lower_expr(*data.index, ctx)});
        },
        [&](const CanonicalFieldExpr &data) -> GLSLExprPtr {
          return make_expr(
              expr.location, expr.type,
              GLSLFieldExpr{lower_expr(*data.object, ctx), data.field});
        },
        [&](const CanonicalConstructExpr &data) -> GLSLExprPtr {
          GLSLConstructExpr lowered;
          lowered.type = data.type;
          for (const auto &arg : data.args) {
            lowered.args.push_back(lower_expr(*arg, ctx));
          }
          return make_expr(expr.location, expr.type, std::move(lowered));
        },
        [&](const CanonicalAssignExpr &data) -> GLSLExprPtr {
          return make_expr(expr.location, expr.type,
                           GLSLAssignExpr{lower_expr(*data.lhs, ctx),
                                          lower_expr(*data.rhs, ctx), data.op});
        }};

    return std::visit(expr_emitter, expr.data);
  }

  GLSLStmtPtr lower_stmt(const CanonicalStmt &stmt,
                         const EntryContext *ctx) const {
    const auto stmt_emitter = Overloaded{
        [&](const CanonicalBlockStmt &data) -> GLSLStmtPtr {
          std::vector<GLSLStmtPtr> stmts;
          for (const auto &child : data.stmts) {
            if (child) {
              stmts.push_back(lower_stmt(*child, ctx));
            }
          }
          return make_stmt(stmt.location, GLSLBlockStmt{std::move(stmts)});
        },
        [&](const CanonicalIfStmt &data) -> GLSLStmtPtr {
          return make_stmt(
              stmt.location,
              GLSLIfStmt{
                  lower_expr(*data.cond, ctx), lower_stmt(*data.then_br, ctx),
                  data.else_br ? lower_stmt(*data.else_br, ctx) : nullptr});
        },
        [&](const CanonicalForStmt &data) -> GLSLStmtPtr {
          return make_stmt(
              stmt.location,
              GLSLForStmt{data.init ? lower_stmt(*data.init, ctx) : nullptr,
                          data.cond ? lower_expr(*data.cond, ctx) : nullptr,
                          data.step ? lower_expr(*data.step, ctx) : nullptr,
                          lower_stmt(*data.body, ctx)});
        },
        [&](const CanonicalWhileStmt &data) -> GLSLStmtPtr {
          return make_stmt(stmt.location,
                           GLSLWhileStmt{lower_expr(*data.cond, ctx),
                                         lower_stmt(*data.body, ctx)});
        },
        [&](const CanonicalReturnStmt &data) -> GLSLStmtPtr {
          return make_stmt(stmt.location,
                           GLSLReturnStmt{data.value
                                              ? lower_expr(*data.value, ctx)
                                              : nullptr});
        },
        [&](const CanonicalExprStmt &data) -> GLSLStmtPtr {
          return make_stmt(stmt.location,
                           GLSLExprStmt{lower_expr(*data.expr, ctx)});
        },
        [&](const CanonicalVarDeclStmt &data) -> GLSLStmtPtr {
          return make_stmt(
              stmt.location,
              GLSLVarDeclStmt{data.type, data.name,
                              data.init ? lower_expr(*data.init, ctx) : nullptr,
                              data.is_const});
        },
        [&](const CanonicalOutputAssignStmt &data) -> GLSLStmtPtr {
          return make_stmt(stmt.location,
                           GLSLOutputAssignStmt{
                               lower_output_target(
                                   data.field, *ctx, stmt.location,
                                   TypeRef{TokenKind::Identifier, data.field}),
                               lower_expr(*data.value, ctx), data.op});
        },
        [&](const CanonicalBreakStmt &) -> GLSLStmtPtr {
          return make_stmt(stmt.location, GLSLBreakStmt{});
        },
        [&](const CanonicalContinueStmt &) -> GLSLStmtPtr {
          return make_stmt(stmt.location, GLSLContinueStmt{});
        },
        [&](const CanonicalDiscardStmt &) -> GLSLStmtPtr {
          return make_stmt(stmt.location, GLSLDiscardStmt{});
        }};

    return std::visit(stmt_emitter, stmt.data);
  }

  const CanonicalStage &m_stage;
  const StageReflection &m_reflection;
};

} // namespace

GlslLoweringResult
GLSLLowering::lower(const CanonicalStage &stage,
                    const StageReflection &reflection) const {
  GLSLStageBuilder builder(stage, reflection);
  return builder.lower();
}

} // namespace astralix
