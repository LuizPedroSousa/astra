#include "shader-lang/pipeline-layout-serializer.hpp"

#include "arena.hpp"
#include "context-proxy.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"
#include <cstring>

namespace astralix {

namespace {

std::string binding_kind_name(ShaderResourceBindingKind kind) {
  switch (kind) {
    case ShaderResourceBindingKind::Sampler:
      return "sampler";
    case ShaderResourceBindingKind::UniformBlock:
      return "uniform_block";
    case ShaderResourceBindingKind::StorageBuffer:
      return "storage_buffer";
  }
  return "sampler";
}

ShaderResourceBindingKind binding_kind_from_name(std::string_view name) {
  if (name == "uniform_block") {
    return ShaderResourceBindingKind::UniformBlock;
  }
  if (name == "storage_buffer") {
    return ShaderResourceBindingKind::StorageBuffer;
  }
  return ShaderResourceBindingKind::Sampler;
}

void serialize_type_ref(ContextProxy ctx, const TypeRef &type) {
  ctx["kind"] = static_cast<int>(type.kind);
  ctx["name"] = type.name;
  ctx["array_size"].assign_if_defined<int>(type.array_size);
  ctx["is_runtime_sized"] = type.is_runtime_sized;
}

TypeRef deserialize_type_ref(ContextProxy ctx) {
  TypeRef type{};
  type.kind = static_cast<TokenKind>(ctx["kind"].as<int>());
  type.name = ctx["name"].as<std::string>();
  if (ctx["array_size"].kind() == SerializationTypeKind::Int) {
    type.array_size = static_cast<uint32_t>(ctx["array_size"].as<int>());
  }
  type.is_runtime_sized =
      ctx["is_runtime_sized"].kind() == SerializationTypeKind::Bool
          ? ctx["is_runtime_sized"].as<bool>()
          : false;
  return type;
}

std::string u64_to_string(uint64_t value) { return std::to_string(value); }

uint64_t u64_from_context(ContextProxy ctx) {
  if (ctx.kind() != SerializationTypeKind::String) {
    return 0;
  }
  return std::stoull(ctx.as<std::string>());
}

void serialize_value_field(ContextProxy ctx,
                           const ShaderValueFieldDesc &field) {
  ctx["binding_id"] = u64_to_string(field.binding_id);
  ctx["logical_name"] = field.logical_name;
  serialize_type_ref(ctx["type"], field.type);
  ctx["stage_mask"] = static_cast<int>(field.stage_mask);
  ctx["offset"] = static_cast<int>(field.offset);
  ctx["size"] = static_cast<int>(field.size);
  ctx["array_stride"] = static_cast<int>(field.array_stride);
  ctx["matrix_stride"] = static_cast<int>(field.matrix_stride);
}

ShaderValueFieldDesc deserialize_value_field(ContextProxy ctx) {
  ShaderValueFieldDesc field;
  field.binding_id = u64_from_context(ctx["binding_id"]);
  field.logical_name = ctx["logical_name"].as<std::string>();
  field.type = deserialize_type_ref(ctx["type"]);
  field.stage_mask = static_cast<uint32_t>(ctx["stage_mask"].as<int>());
  field.offset = static_cast<uint32_t>(ctx["offset"].as<int>());
  field.size = static_cast<uint32_t>(ctx["size"].as<int>());
  field.array_stride = static_cast<uint32_t>(ctx["array_stride"].as<int>());
  field.matrix_stride = static_cast<uint32_t>(ctx["matrix_stride"].as<int>());
  return field;
}

void serialize_value_block(ContextProxy ctx,
                           const ShaderValueBlockDesc &block) {
  ctx["block_id"] = u64_to_string(block.block_id);
  ctx["logical_name"] = block.logical_name;
  ctx["descriptor_set"].assign_if_defined<int>(block.descriptor_set);
  ctx["binding"].assign_if_defined<int>(block.binding);
  ctx["size"] = static_cast<int>(block.size);

  for (size_t i = 0; i < block.fields.size(); ++i) {
    serialize_value_field(ctx["fields"][static_cast<int>(i)], block.fields[i]);
  }
}

ShaderValueBlockDesc deserialize_value_block(ContextProxy ctx) {
  ShaderValueBlockDesc block;
  block.block_id = u64_from_context(ctx["block_id"]);
  block.logical_name = ctx["logical_name"].as<std::string>();
  if (ctx["descriptor_set"].kind() == SerializationTypeKind::Int) {
    block.descriptor_set =
        static_cast<uint32_t>(ctx["descriptor_set"].as<int>());
  }
  if (ctx["binding"].kind() == SerializationTypeKind::Int) {
    block.binding = static_cast<uint32_t>(ctx["binding"].as<int>());
  }
  block.size = static_cast<uint32_t>(ctx["size"].as<int>());

  auto field_count = ctx["fields"].size();
  for (size_t i = 0; i < field_count; ++i) {
    block.fields.push_back(
        deserialize_value_field(ctx["fields"][static_cast<int>(i)]));
  }
  return block;
}

void serialize_resource_binding(ContextProxy ctx,
                                const ShaderResourceBindingDesc &resource) {
  ctx["binding_id"] = u64_to_string(resource.binding_id);
  ctx["logical_name"] = resource.logical_name;
  ctx["source_kind"] = binding_kind_name(resource.source_kind);
  serialize_type_ref(ctx["type"], resource.type);
  ctx["stage_mask"] = static_cast<int>(resource.stage_mask);
  ctx["descriptor_set"] = static_cast<int>(resource.descriptor_set);
  ctx["binding"] = static_cast<int>(resource.binding);
  ctx["array_size"].assign_if_defined<int>(resource.array_size);
}

ShaderResourceBindingDesc deserialize_resource_binding(ContextProxy ctx) {
  ShaderResourceBindingDesc resource;
  resource.binding_id = u64_from_context(ctx["binding_id"]);
  resource.logical_name = ctx["logical_name"].as<std::string>();
  resource.source_kind =
      binding_kind_from_name(ctx["source_kind"].as<std::string>());
  resource.type = deserialize_type_ref(ctx["type"]);
  resource.stage_mask = static_cast<uint32_t>(ctx["stage_mask"].as<int>());
  resource.descriptor_set =
      static_cast<uint32_t>(ctx["descriptor_set"].as<int>());
  resource.binding = static_cast<uint32_t>(ctx["binding"].as<int>());
  if (ctx["array_size"].kind() == SerializationTypeKind::Int) {
    resource.array_size = static_cast<uint32_t>(ctx["array_size"].as<int>());
  }
  return resource;
}

void serialize_vertex_attribute(ContextProxy ctx,
                                const VertexAttributeDesc &attribute) {
  ctx["logical_name"] = attribute.logical_name;
  ctx["location"] = static_cast<int>(attribute.location);
  serialize_type_ref(ctx["type"], attribute.type);
  ctx["offset"] = static_cast<int>(attribute.offset);
  ctx["stride"] = static_cast<int>(attribute.stride);
  ctx["per_instance"] = attribute.per_instance;
}

VertexAttributeDesc deserialize_vertex_attribute(ContextProxy ctx) {
  VertexAttributeDesc attribute;
  attribute.logical_name = ctx["logical_name"].as<std::string>();
  attribute.location = static_cast<uint32_t>(ctx["location"].as<int>());
  attribute.type = deserialize_type_ref(ctx["type"]);
  attribute.offset = static_cast<uint32_t>(ctx["offset"].as<int>());
  attribute.stride = static_cast<uint32_t>(ctx["stride"].as<int>());
  attribute.per_instance =
      ctx["per_instance"].kind() == SerializationTypeKind::Bool
          ? ctx["per_instance"].as<bool>()
          : false;
  return attribute;
}

} // namespace

std::filesystem::path
shader_layout_sidecar_path(const std::filesystem::path &source_path,
                           SerializationFormat format) {
  auto ctx = SerializationContext::create(format);
  return source_path.parent_path() /
         (source_path.stem().string() + ".layout" + ctx->extension());
}

std::optional<std::string>
serialize_shader_pipeline_layout(const ShaderPipelineLayout &layout,
                                 SerializationFormat format,
                                 std::string *error) {
  (void)error;

  auto ctx = SerializationContext::create(format);

  auto resource_ctx = (*ctx)["resource_layout"];
  resource_ctx["compatibility_hash"] =
      u64_to_string(layout.resource_layout.compatibility_hash);

  for (size_t i = 0; i < layout.resource_layout.value_blocks.size(); ++i) {
    serialize_value_block(resource_ctx["value_blocks"][static_cast<int>(i)],
                          layout.resource_layout.value_blocks[i]);
  }

  for (size_t i = 0; i < layout.resource_layout.resources.size(); ++i) {
    serialize_resource_binding(resource_ctx["resources"][static_cast<int>(i)],
                               layout.resource_layout.resources[i]);
  }

  auto vertex_ctx = (*ctx)["vertex_input"];
  vertex_ctx["compatibility_hash"] =
      u64_to_string(layout.vertex_input.compatibility_hash);

  for (size_t i = 0; i < layout.vertex_input.attributes.size(); ++i) {
    serialize_vertex_attribute(vertex_ctx["attributes"][static_cast<int>(i)],
                               layout.vertex_input.attributes[i]);
  }

  auto pipeline_ctx = (*ctx)["pipeline_layout"];
  pipeline_ctx["compatibility_hash"] =
      u64_to_string(layout.pipeline_layout.compatibility_hash);

  for (size_t i = 0; i < layout.pipeline_layout.descriptor_sets.size(); ++i) {
    const auto &set = layout.pipeline_layout.descriptor_sets[i];
    auto set_ctx = pipeline_ctx["descriptor_sets"][static_cast<int>(i)];
    set_ctx["set"] = static_cast<int>(set.set);

    for (size_t j = 0; j < set.bindings.size(); ++j) {
      serialize_resource_binding(set_ctx["bindings"][static_cast<int>(j)],
                                 set.bindings[j]);
    }
  }

  for (size_t i = 0; i < layout.pipeline_layout.value_blocks.size(); ++i) {
    serialize_value_block(
        pipeline_ctx["value_blocks"][static_cast<int>(i)],
        layout.pipeline_layout.value_blocks[i]);
  }

  ElasticArena arena;
  auto buffer = ctx->to_buffer(arena);
  return std::string(static_cast<const char *>(buffer->data), buffer->size);
}

std::optional<ShaderPipelineLayout>
deserialize_shader_pipeline_layout(std::string_view content,
                                   SerializationFormat format,
                                   std::string *error) {
  (void)error;

  auto buffer =
      create_scope<StreamBuffer>(content.size() == 0 ? 1 : content.size() + 1);
  if (!content.empty()) {
    std::memcpy(buffer->data(), content.data(), content.size());
  }
  buffer->data()[content.size()] = '\0';

  auto ctx = SerializationContext::create(format, std::move(buffer));

  ShaderPipelineLayout layout;

  auto resource_ctx = (*ctx)["resource_layout"];
  layout.resource_layout.compatibility_hash =
      u64_from_context(resource_ctx["compatibility_hash"]);

  auto block_count = resource_ctx["value_blocks"].size();
  for (size_t i = 0; i < block_count; ++i) {
    layout.resource_layout.value_blocks.push_back(
        deserialize_value_block(resource_ctx["value_blocks"][static_cast<int>(i)]));
  }

  auto resource_count = resource_ctx["resources"].size();
  for (size_t i = 0; i < resource_count; ++i) {
    layout.resource_layout.resources.push_back(
        deserialize_resource_binding(resource_ctx["resources"][static_cast<int>(i)]));
  }

  auto vertex_ctx = (*ctx)["vertex_input"];
  layout.vertex_input.compatibility_hash =
      u64_from_context(vertex_ctx["compatibility_hash"]);

  auto attribute_count = vertex_ctx["attributes"].size();
  for (size_t i = 0; i < attribute_count; ++i) {
    layout.vertex_input.attributes.push_back(
        deserialize_vertex_attribute(vertex_ctx["attributes"][static_cast<int>(i)]));
  }

  auto pipeline_ctx = (*ctx)["pipeline_layout"];
  layout.pipeline_layout.compatibility_hash =
      u64_from_context(pipeline_ctx["compatibility_hash"]);

  auto set_count = pipeline_ctx["descriptor_sets"].size();
  for (size_t i = 0; i < set_count; ++i) {
    auto set_ctx = pipeline_ctx["descriptor_sets"][static_cast<int>(i)];
    DescriptorSetLayoutDesc set_desc;
    set_desc.set = static_cast<uint32_t>(set_ctx["set"].as<int>());

    auto binding_count = set_ctx["bindings"].size();
    for (size_t j = 0; j < binding_count; ++j) {
      set_desc.bindings.push_back(
          deserialize_resource_binding(set_ctx["bindings"][static_cast<int>(j)]));
    }
    layout.pipeline_layout.descriptor_sets.push_back(std::move(set_desc));
  }

  auto value_block_count = pipeline_ctx["value_blocks"].size();
  for (size_t i = 0; i < value_block_count; ++i) {
    layout.pipeline_layout.value_blocks.push_back(
        deserialize_value_block(pipeline_ctx["value_blocks"][static_cast<int>(i)]));
  }

  return layout;
}

} // namespace astralix
