#include "shader-lang/reflection-serializer.hpp"

#include "arena.hpp"
#include "context-proxy.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <string_view>

namespace astralix {

namespace {

std::string stage_kind_name(StageKind stage) {
  switch (stage) {
    case StageKind::Vertex:
      return "vertex";
    case StageKind::Fragment:
      return "fragment";
    case StageKind::Geometry:
      return "geometry";
    case StageKind::Compute:
      return "compute";
  }

  return "vertex";
}

StageKind stage_kind_from_name(std::string_view name) {
  if (name == "fragment") {
    return StageKind::Fragment;
  }
  if (name == "geometry") {
    return StageKind::Geometry;
  }
  if (name == "compute") {
    return StageKind::Compute;
  }

  return StageKind::Vertex;
}

std::string resource_kind_name(ShaderResourceKind kind) {
  switch (kind) {
    case ShaderResourceKind::UniformValue:
      return "uniform_value";
    case ShaderResourceKind::Sampler:
      return "sampler";
    case ShaderResourceKind::UniformInterface:
      return "uniform_interface";
    case ShaderResourceKind::UniformBlock:
      return "uniform_block";
    case ShaderResourceKind::StorageBuffer:
      return "storage_buffer";
  }

  return "uniform_value";
}

ShaderResourceKind resource_kind_from_name(std::string_view name) {
  if (name == "sampler") {
    return ShaderResourceKind::Sampler;
  }
  if (name == "uniform_interface") {
    return ShaderResourceKind::UniformInterface;
  }
  if (name == "uniform_block") {
    return ShaderResourceKind::UniformBlock;
  }
  if (name == "storage_buffer") {
    return ShaderResourceKind::StorageBuffer;
  }

  return ShaderResourceKind::UniformValue;
}

std::string binding_id_to_string(uint64_t binding_id) {
  return std::to_string(binding_id);
}

std::optional<uint64_t> binding_id_from_context(ContextProxy ctx) {
  if (ctx.kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  return std::stoull(ctx.as<std::string>());
}

void serialize_type_ref(ContextProxy ctx, const TypeRef &type) {
  ctx["kind"] = static_cast<int>(type.kind);
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, type, name);
  ctx["array_size"].assign_if_defined<int>(type.array_size);
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, type, is_runtime_sized);
}

TypeRef deserialize_type_ref(ContextProxy ctx) {
  TypeRef type{};
  type.kind = static_cast<TokenKind>(ctx["kind"].as<int>());
  type.name = ctx["name"].as<std::string>();
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, type, array_size, int,
                                      SerializationTypeKind::Int);
  type.is_runtime_sized =
      ctx["is_runtime_sized"].kind() == SerializationTypeKind::Bool
          ? ctx["is_runtime_sized"].as<bool>()
          : false;
  return type;
}

void serialize_backend_layout(ContextProxy ctx,
                              const BackendLayoutReflection &layout) {
  ctx["descriptor_set"].assign_if_defined<int>(layout.descriptor_set);
  ctx["binding"].assign_if_defined<int>(layout.binding);
  ctx["location"].assign_if_defined<int>(layout.location);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, layout, emitted_name);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, layout, block_name);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, layout, instance_name);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, layout, storage);
}

BackendLayoutReflection deserialize_backend_layout(ContextProxy ctx) {
  BackendLayoutReflection layout;
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, descriptor_set, int,
                                      SerializationTypeKind::Int);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, binding, int,
                                      SerializationTypeKind::Int);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, location, int,
                                      SerializationTypeKind::Int);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, emitted_name, std::string,
                                      SerializationTypeKind::String);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, block_name, std::string,
                                      SerializationTypeKind::String);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, instance_name, std::string,
                                      SerializationTypeKind::String);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, layout, storage, std::string,
                                      SerializationTypeKind::String);
  return layout;
}

void serialize_default_value(ContextProxy ctx,
                             const std::optional<ShaderDefaultValue> &value) {
  if (!value) {
    return;
  }

  std::visit([&](const auto &typed_value) { ctx = typed_value; }, *value);
}

std::optional<ShaderDefaultValue> deserialize_default_value(ContextProxy ctx) {
  switch (ctx.kind()) {
    case SerializationTypeKind::Bool:
      return ShaderDefaultValue(ctx.as<bool>());
    case SerializationTypeKind::Int:
      return ShaderDefaultValue(ctx.as<int>());
    case SerializationTypeKind::Float:
      return ShaderDefaultValue(ctx.as<float>());
    default:
      return std::nullopt;
  }
}

void serialize_declared_field(ContextProxy ctx,
                              const DeclaredFieldReflection &field) {
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, field, name);
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, field, logical_name);
  serialize_type_ref(ctx["type"], field.type);
  ctx["array_size"].assign_if_defined<int>(field.array_size);
  serialize_default_value(ctx["default_value"], field.default_value);
  ctx["active_stage_mask"] = static_cast<int>(field.active_stage_mask);
  ctx["binding_id"] = binding_id_to_string(field.binding_id);

  for (size_t i = 0; i < field.fields.size(); ++i) {
    serialize_declared_field(ctx["fields"][static_cast<int>(i)],
                             field.fields[i]);
  }
}

DeclaredFieldReflection deserialize_declared_field(ContextProxy ctx) {
  DeclaredFieldReflection field;
  field.name = ctx["name"].as<std::string>();
  field.logical_name = ctx["logical_name"].as<std::string>();
  field.type = deserialize_type_ref(ctx["type"]);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, field, array_size, int,
                                      SerializationTypeKind::Int);
  field.default_value = deserialize_default_value(ctx["default_value"]);
  field.active_stage_mask =
      ctx["active_stage_mask"].kind() == SerializationTypeKind::Int
          ? static_cast<uint32_t>(ctx["active_stage_mask"].as<int>())
          : 0u;
  if (auto binding_id = binding_id_from_context(ctx["binding_id"])) {
    field.binding_id = *binding_id;
  }

  auto child_count = ctx["fields"].size();
  for (size_t i = 0; i < child_count; ++i) {
    field.fields.push_back(
        deserialize_declared_field(ctx["fields"][static_cast<int>(i)]));
  }

  return field;
}

void serialize_member(ContextProxy ctx, const MemberReflection &member) {
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, member, logical_name);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, member, compatibility_alias);
  serialize_type_ref(ctx["type"], member.type);
  ctx["array_size"].assign_if_defined<int>(member.array_size);
  ctx["binding_id"] = binding_id_to_string(member.binding_id);
  serialize_backend_layout(ctx["glsl"], member.glsl);
}

MemberReflection deserialize_member(ContextProxy ctx) {
  MemberReflection member;
  member.logical_name = ctx["logical_name"].as<std::string>();
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, member, compatibility_alias,
                                      std::string,
                                      SerializationTypeKind::String);
  member.type = deserialize_type_ref(ctx["type"]);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, member, array_size, int,
                                      SerializationTypeKind::Int);
  if (auto binding_id = binding_id_from_context(ctx["binding_id"])) {
    member.binding_id = *binding_id;
  }
  member.glsl = deserialize_backend_layout(ctx["glsl"]);
  return member;
}

void serialize_resource(ContextProxy ctx, const ResourceReflection &resource) {
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, resource, logical_name);
  ctx["kind"] = resource_kind_name(resource.kind);
  ctx["stage"] = stage_kind_name(resource.stage);
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, resource, declared_name);
  serialize_type_ref(ctx["type"], resource.type);
  ctx["array_size"].assign_if_defined<int>(resource.array_size);
  serialize_backend_layout(ctx["glsl"], resource.glsl);

  for (size_t i = 0; i < resource.members.size(); ++i) {
    serialize_member(ctx["members"][static_cast<int>(i)], resource.members[i]);
  }

  for (size_t i = 0; i < resource.declared_fields.size(); ++i) {
    serialize_declared_field(ctx["declared_fields"][static_cast<int>(i)],
                             resource.declared_fields[i]);
  }
}

ResourceReflection deserialize_resource(ContextProxy ctx) {
  ResourceReflection resource;
  resource.logical_name = ctx["logical_name"].as<std::string>();
  resource.kind = resource_kind_from_name(ctx["kind"].as<std::string>());
  resource.stage = stage_kind_from_name(ctx["stage"].as<std::string>());
  resource.declared_name = ctx["declared_name"].as<std::string>();
  resource.type = deserialize_type_ref(ctx["type"]);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, resource, array_size, int,
                                      SerializationTypeKind::Int);
  resource.glsl = deserialize_backend_layout(ctx["glsl"]);

  auto member_count = ctx["members"].size();
  for (size_t i = 0; i < member_count; ++i) {
    resource.members.push_back(
        deserialize_member(ctx["members"][static_cast<int>(i)]));
  }

  auto declared_field_count = ctx["declared_fields"].size();
  for (size_t i = 0; i < declared_field_count; ++i) {
    resource.declared_fields.push_back(deserialize_declared_field(
        ctx["declared_fields"][static_cast<int>(i)]));
  }
  return resource;
}

void serialize_stage_field(ContextProxy ctx,
                           const StageFieldReflection &field) {
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, field, logical_name);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, field, compatibility_alias);
  ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, field, interface_name);
  serialize_type_ref(ctx["type"], field.type);
  ctx["array_size"].assign_if_defined<int>(field.array_size);
  serialize_backend_layout(ctx["glsl"], field.glsl);
}

StageFieldReflection deserialize_stage_field(ContextProxy ctx) {
  StageFieldReflection field;
  field.logical_name = ctx["logical_name"].as<std::string>();
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, field, compatibility_alias,
                                      std::string,
                                      SerializationTypeKind::String);
  field.interface_name = ctx["interface_name"].as<std::string>();
  field.type = deserialize_type_ref(ctx["type"]);
  ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, field, array_size, int,
                                      SerializationTypeKind::Int);
  field.glsl = deserialize_backend_layout(ctx["glsl"]);
  return field;
}

} // namespace

std::filesystem::path
shader_reflection_sidecar_path(const std::filesystem::path &source_path,
                               SerializationFormat format) {
  auto ctx = SerializationContext::create(format);
  return source_path.parent_path() /
         (source_path.stem().string() + ".reflection" + ctx->extension());
}

std::optional<std::string>
serialize_shader_reflection(const ShaderReflection &reflection,
                            SerializationFormat format, std::string *error) {
  (void)error;

  auto ctx = SerializationContext::create(format);
  (*ctx)["version"] = reflection.version;

  size_t stage_index = 0;
  for (const auto &[stage_kind, stage] : reflection.stages) {
    auto stage_ctx = (*ctx)["stages"][static_cast<int>(stage_index++)];
    stage_ctx["stage"] = stage_kind_name(stage_kind);

    for (size_t i = 0; i < stage.stage_inputs.size(); ++i) {
      serialize_stage_field(stage_ctx["stage_inputs"][static_cast<int>(i)],
                            stage.stage_inputs[i]);
    }

    for (size_t i = 0; i < stage.stage_outputs.size(); ++i) {
      serialize_stage_field(stage_ctx["stage_outputs"][static_cast<int>(i)],
                            stage.stage_outputs[i]);
    }

    for (size_t i = 0; i < stage.resources.size(); ++i) {
      serialize_resource(stage_ctx["resources"][static_cast<int>(i)],
                         stage.resources[i]);
    }
  }

  ElasticArena arena;
  auto buffer = ctx->to_buffer(arena);
  return std::string(static_cast<const char *>(buffer->data), buffer->size);
}

std::optional<ShaderReflection>
deserialize_shader_reflection(std::string_view content,
                              SerializationFormat format, std::string *error) {
  (void)error;

  auto buffer =
      create_scope<StreamBuffer>(content.size() == 0 ? 1 : content.size() + 1);
  if (!content.empty()) {
    std::memcpy(buffer->data(), content.data(), content.size());
  }
  buffer->data()[content.size()] = '\0';

  auto ctx = SerializationContext::create(format, std::move(buffer));

  ShaderReflection reflection;
  reflection.version =
      ctx->operator[]("version").kind() == SerializationTypeKind::Int
          ? ctx->operator[]("version").as<int>()
          : 1;

  auto stage_count = ctx->operator[]("stages").size();
  for (size_t i = 0; i < stage_count; ++i) {
    auto stage_ctx = ctx->operator[]("stages")[static_cast<int>(i)];

    StageReflection stage;
    stage.stage = stage_kind_from_name(stage_ctx["stage"].as<std::string>());

    auto input_count = stage_ctx["stage_inputs"].size();
    for (size_t j = 0; j < input_count; ++j) {
      stage.stage_inputs.push_back(deserialize_stage_field(
          stage_ctx["stage_inputs"][static_cast<int>(j)]));
    }

    auto output_count = stage_ctx["stage_outputs"].size();
    for (size_t j = 0; j < output_count; ++j) {
      stage.stage_outputs.push_back(deserialize_stage_field(
          stage_ctx["stage_outputs"][static_cast<int>(j)]));
    }

    auto resource_count = stage_ctx["resources"].size();
    for (size_t j = 0; j < resource_count; ++j) {
      stage.resources.push_back(
          deserialize_resource(stage_ctx["resources"][static_cast<int>(j)]));
    }

    reflection.stages.emplace(stage.stage, std::move(stage));
  }

  return reflection;
}

std::optional<ShaderReflection>
read_shader_reflection(const std::filesystem::path &path,
                       SerializationFormat format, std::string *error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (error) {
      *error = "cannot open reflection sidecar '" + path.string() + "'";
    }
    return std::nullopt;
  }

  std::ostringstream buffer_stream;
  buffer_stream << file.rdbuf();
  std::string content = buffer_stream.str();
  return deserialize_shader_reflection(content, format, error);
}

} // namespace astralix
