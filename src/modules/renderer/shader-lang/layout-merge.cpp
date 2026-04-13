#include "shader-lang/layout-merge.hpp"

#include "fnv1a.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>

namespace astralix {

namespace {

std::string stage_mask_string(uint32_t stage_mask) {
  return std::to_string(stage_mask);
}

std::string type_ref_string(const TypeRef &type) {
  switch (type.kind) {
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
      return type.name.empty() ? "identifier" : type.name;
    default:
      return type.name.empty() ? "unknown" : type.name;
  }
}

std::string type_signature(const TypeRef &type) {
  std::string signature = type_ref_string(type);
  if (type.array_size.has_value()) {
    signature += "[" + std::to_string(*type.array_size) + "]";
  } else if (type.is_runtime_sized) {
    signature += "[]";
  }
  return signature;
}

bool same_type_ref(const TypeRef &lhs, const TypeRef &rhs) {
  return lhs.kind == rhs.kind && lhs.name == rhs.name &&
         lhs.array_size == rhs.array_size &&
         lhs.is_runtime_sized == rhs.is_runtime_sized;
}

std::string optional_u32_string(const std::optional<uint32_t> &value) {
  return value.has_value() ? std::to_string(*value) : "unset";
}

std::string field_source_key(
    std::string_view block_logical_name, std::string_view field_logical_name
) {
  return std::string(block_logical_name) + "::" + std::string(field_logical_name);
}

bool ranges_overlap(
    uint32_t lhs_offset, uint32_t lhs_size, uint32_t rhs_offset, uint32_t rhs_size
) {
  const uint32_t lhs_end = lhs_offset + lhs_size;
  const uint32_t rhs_end = rhs_offset + rhs_size;
  return lhs_offset < rhs_end && rhs_offset < lhs_end;
}

void push_conflict(
    std::vector<std::string> &errors,
    std::string_view category,
    std::string_view logical_name,
    std::string_view existing_source,
    std::string_view incoming_source,
    std::string_view detail
) {
  std::ostringstream message;
  message << "Shader layout conflict for " << category << " '" << logical_name
          << "' between " << existing_source << " and " << incoming_source
          << ": " << detail;
  errors.push_back(message.str());
}

uint64_t compute_resource_layout_hash(const ShaderResourceLayout &layout) {
  uint64_t hash = k_fnv1a64_offset_basis;

  auto append_u32 = [&](uint32_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  auto append_u64 = [&](uint64_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  for (const auto &block : layout.value_blocks) {
    append_u64(block.block_id);
    hash = fnv1a64_append_string(block.logical_name, hash);
    append_u32(block.descriptor_set.value_or(0));
    append_u32(block.binding.value_or(0));
    append_u32(block.size);

    for (const auto &field : block.fields) {
      append_u64(field.binding_id);
      hash = fnv1a64_append_string(field.logical_name, hash);
      append_u32(static_cast<uint32_t>(field.type.kind));
      append_u32(field.stage_mask);
      append_u32(field.offset);
      append_u32(field.size);
      append_u32(field.array_stride);
      append_u32(field.matrix_stride);
    }
  }

  for (const auto &resource : layout.resources) {
    append_u64(resource.binding_id);
    hash = fnv1a64_append_string(resource.logical_name, hash);
    append_u32(static_cast<uint32_t>(resource.source_kind));
    append_u32(static_cast<uint32_t>(resource.type.kind));
    append_u32(resource.stage_mask);
    append_u32(resource.descriptor_set);
    append_u32(resource.binding);
    append_u32(resource.array_size.value_or(0));
  }

  return hash;
}

uint64_t compute_vertex_input_hash(const VertexInputLayoutDesc &layout) {
  uint64_t hash = k_fnv1a64_offset_basis;

  auto append_u32 = [&](uint32_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  for (const auto &attribute : layout.attributes) {
    hash = fnv1a64_append_string(attribute.logical_name, hash);
    append_u32(attribute.location);
    append_u32(static_cast<uint32_t>(attribute.type.kind));
    append_u32(attribute.offset);
    append_u32(attribute.stride);
    append_u32(attribute.per_instance ? 1u : 0u);
  }

  return hash;
}

uint64_t compute_pipeline_layout_hash(const PipelineLayoutDesc &layout) {
  uint64_t hash = k_fnv1a64_offset_basis;

  auto append_u32 = [&](uint32_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  auto append_u64 = [&](uint64_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  append_u32(static_cast<uint32_t>(layout.descriptor_sets.size()));

  for (const auto &set : layout.descriptor_sets) {
    append_u32(set.set);
    append_u32(static_cast<uint32_t>(set.bindings.size()));

    for (const auto &binding : set.bindings) {
      append_u64(binding.binding_id);
      append_u32(binding.descriptor_set);
      append_u32(binding.binding);
      append_u32(static_cast<uint32_t>(binding.source_kind));
      append_u32(static_cast<uint32_t>(binding.type.kind));
      append_u32(binding.stage_mask);
    }
  }

  for (const auto &block : layout.value_blocks) {
    append_u64(block.block_id);
    append_u32(block.size);
  }

  return hash;
}

void rebuild_pipeline_layout(ShaderPipelineLayout &layout) {
  layout.resource_layout.compatibility_hash =
      compute_resource_layout_hash(layout.resource_layout);
  layout.vertex_input.compatibility_hash =
      compute_vertex_input_hash(layout.vertex_input);

  std::map<uint32_t, DescriptorSetLayoutDesc> descriptor_sets;
  for (const auto &resource : layout.resource_layout.resources) {
    auto &set_desc = descriptor_sets[resource.descriptor_set];
    set_desc.set = resource.descriptor_set;
    set_desc.bindings.push_back(resource);
  }

  layout.pipeline_layout.descriptor_sets.clear();
  for (auto &[set_index, set_desc] : descriptor_sets) {
    (void)set_index;
    std::sort(
        set_desc.bindings.begin(),
        set_desc.bindings.end(),
        [](const ShaderResourceBindingDesc &lhs,
           const ShaderResourceBindingDesc &rhs) {
          if (lhs.binding != rhs.binding) {
            return lhs.binding < rhs.binding;
          }
          return lhs.logical_name < rhs.logical_name;
        }
    );
    layout.pipeline_layout.descriptor_sets.push_back(std::move(set_desc));
  }

  layout.pipeline_layout.value_blocks = layout.resource_layout.value_blocks;
  layout.pipeline_layout.compatibility_hash =
      compute_pipeline_layout_hash(layout.pipeline_layout);
}

} // namespace

bool merge_pipeline_layout_checked(
    ShaderPipelineLayout &merged,
    const ShaderPipelineLayout &incoming,
    std::string_view incoming_source,
    LayoutMergeState &state,
    std::vector<std::string> &errors
) {
  for (const auto &incoming_block : incoming.resource_layout.value_blocks) {
    auto existing_block_it = std::find_if(
        merged.resource_layout.value_blocks.begin(),
        merged.resource_layout.value_blocks.end(),
        [&](const ShaderValueBlockDesc &block) {
          return block.logical_name == incoming_block.logical_name;
        }
    );

    if (existing_block_it == merged.resource_layout.value_blocks.end()) {
      merged.resource_layout.value_blocks.push_back(incoming_block);
      state.value_block_sources[incoming_block.logical_name] =
          std::string(incoming_source);
      for (const auto &field : incoming_block.fields) {
        state.value_field_sources[field_source_key(
            incoming_block.logical_name, field.logical_name
        )] = std::string(incoming_source);
      }
      continue;
    }

    auto &existing_block = *existing_block_it;
    const auto existing_source_it =
        state.value_block_sources.find(existing_block.logical_name);
    const std::string existing_source =
        existing_source_it != state.value_block_sources.end()
            ? existing_source_it->second
            : std::string("<unknown>");

    if (existing_block.block_id != incoming_block.block_id) {
      push_conflict(
          errors,
          "value block",
          incoming_block.logical_name,
          existing_source,
          incoming_source,
          "block ids differ (" + std::to_string(existing_block.block_id) +
              " vs " + std::to_string(incoming_block.block_id) + ")"
      );
      return false;
    }

    if (existing_block.descriptor_set.has_value() &&
        incoming_block.descriptor_set.has_value() &&
        existing_block.descriptor_set != incoming_block.descriptor_set) {
      push_conflict(
          errors,
          "value block",
          incoming_block.logical_name,
          existing_source,
          incoming_source,
          "descriptor sets differ (" +
              optional_u32_string(existing_block.descriptor_set) + " vs " +
              optional_u32_string(incoming_block.descriptor_set) + ")"
      );
      return false;
    }
    if (!existing_block.descriptor_set.has_value()) {
      existing_block.descriptor_set = incoming_block.descriptor_set;
    }

    if (existing_block.binding.has_value() && incoming_block.binding.has_value() &&
        existing_block.binding != incoming_block.binding) {
      push_conflict(
          errors,
          "value block",
          incoming_block.logical_name,
          existing_source,
          incoming_source,
          "bindings differ (" + optional_u32_string(existing_block.binding) +
              " vs " + optional_u32_string(incoming_block.binding) + ")"
      );
      return false;
    }
    if (!existing_block.binding.has_value()) {
      existing_block.binding = incoming_block.binding;
    }

    for (const auto &incoming_field : incoming_block.fields) {
      auto existing_field_it = std::find_if(
          existing_block.fields.begin(),
          existing_block.fields.end(),
          [&](const ShaderValueFieldDesc &field) {
            return field.logical_name == incoming_field.logical_name;
          }
      );

      if (existing_field_it == existing_block.fields.end()) {
        for (const auto &existing_field : existing_block.fields) {
          if (ranges_overlap(
                  existing_field.offset,
                  existing_field.size,
                  incoming_field.offset,
                  incoming_field.size
              )) {
            push_conflict(
                errors,
                "value field",
                incoming_block.logical_name + "." + incoming_field.logical_name,
                existing_source,
                incoming_source,
                "overlaps with existing field '" + existing_field.logical_name +
                    "' at offsets " + std::to_string(existing_field.offset) +
                    " and " + std::to_string(incoming_field.offset)
            );
            return false;
          }
        }

        existing_block.fields.push_back(incoming_field);
        state.value_field_sources[field_source_key(
            existing_block.logical_name, incoming_field.logical_name
        )] = std::string(incoming_source);
        continue;
      }

      auto &existing_field = *existing_field_it;
      const auto field_source_it = state.value_field_sources.find(
          field_source_key(existing_block.logical_name, existing_field.logical_name)
      );
      const std::string field_source =
          field_source_it != state.value_field_sources.end()
              ? field_source_it->second
              : existing_source;

      if (existing_field.binding_id != incoming_field.binding_id ||
          !same_type_ref(existing_field.type, incoming_field.type) ||
          existing_field.offset != incoming_field.offset ||
          existing_field.size != incoming_field.size ||
          existing_field.array_stride != incoming_field.array_stride ||
          existing_field.matrix_stride != incoming_field.matrix_stride) {
        std::ostringstream detail;
        detail << "field metadata differs (binding_id "
               << existing_field.binding_id << " vs "
               << incoming_field.binding_id << ", type "
               << type_signature(existing_field.type) << " vs "
               << type_signature(incoming_field.type) << ", offset "
               << existing_field.offset << " vs " << incoming_field.offset
               << ", size " << existing_field.size << " vs "
               << incoming_field.size << ", array_stride "
               << existing_field.array_stride << " vs "
               << incoming_field.array_stride << ", matrix_stride "
               << existing_field.matrix_stride << " vs "
               << incoming_field.matrix_stride << ")";
        push_conflict(
            errors,
            "value field",
            incoming_block.logical_name + "." + incoming_field.logical_name,
            field_source,
            incoming_source,
            detail.str()
        );
        return false;
      }

      existing_field.stage_mask |= incoming_field.stage_mask;
    }

    existing_block.size = std::max(existing_block.size, incoming_block.size);
  }

  for (const auto &incoming_resource : incoming.resource_layout.resources) {
    auto existing_resource_it = std::find_if(
        merged.resource_layout.resources.begin(),
        merged.resource_layout.resources.end(),
        [&](const ShaderResourceBindingDesc &resource) {
          return resource.logical_name == incoming_resource.logical_name;
        }
    );

    if (existing_resource_it == merged.resource_layout.resources.end()) {
      merged.resource_layout.resources.push_back(incoming_resource);
      state.resource_sources[incoming_resource.logical_name] =
          std::string(incoming_source);
      continue;
    }

    auto &existing_resource = *existing_resource_it;
    const auto existing_source_it =
        state.resource_sources.find(existing_resource.logical_name);
    const std::string existing_source =
        existing_source_it != state.resource_sources.end()
            ? existing_source_it->second
            : std::string("<unknown>");

    if (existing_resource.binding_id != incoming_resource.binding_id ||
        existing_resource.source_kind != incoming_resource.source_kind ||
        !same_type_ref(existing_resource.type, incoming_resource.type) ||
        existing_resource.descriptor_set != incoming_resource.descriptor_set ||
        existing_resource.binding != incoming_resource.binding ||
        existing_resource.array_size != incoming_resource.array_size) {
      std::ostringstream detail;
      detail << "resource metadata differs (binding_id "
             << existing_resource.binding_id << " vs "
             << incoming_resource.binding_id << ", kind "
             << static_cast<uint32_t>(existing_resource.source_kind) << " vs "
             << static_cast<uint32_t>(incoming_resource.source_kind)
             << ", type " << type_signature(existing_resource.type) << " vs "
             << type_signature(incoming_resource.type) << ", set "
             << existing_resource.descriptor_set << " vs "
             << incoming_resource.descriptor_set << ", binding "
             << existing_resource.binding << " vs "
             << incoming_resource.binding << ", array_size "
             << optional_u32_string(existing_resource.array_size) << " vs "
             << optional_u32_string(incoming_resource.array_size) << ")";
      push_conflict(
          errors,
          "resource",
          incoming_resource.logical_name,
          existing_source,
          incoming_source,
          detail.str()
      );
      return false;
    }

    existing_resource.stage_mask |= incoming_resource.stage_mask;
  }

  for (const auto &incoming_attribute : incoming.vertex_input.attributes) {
    auto existing_attribute_it = std::find_if(
        merged.vertex_input.attributes.begin(),
        merged.vertex_input.attributes.end(),
        [&](const VertexAttributeDesc &attribute) {
          return attribute.logical_name == incoming_attribute.logical_name;
        }
    );

    if (existing_attribute_it == merged.vertex_input.attributes.end()) {
      merged.vertex_input.attributes.push_back(incoming_attribute);
      state.attribute_sources[incoming_attribute.logical_name] =
          std::string(incoming_source);
      continue;
    }

    const auto &existing_attribute = *existing_attribute_it;
    const auto existing_source_it =
        state.attribute_sources.find(existing_attribute.logical_name);
    const std::string existing_source =
        existing_source_it != state.attribute_sources.end()
            ? existing_source_it->second
            : std::string("<unknown>");

    if (existing_attribute.location != incoming_attribute.location ||
        !same_type_ref(existing_attribute.type, incoming_attribute.type) ||
        existing_attribute.offset != incoming_attribute.offset ||
        existing_attribute.stride != incoming_attribute.stride ||
        existing_attribute.per_instance != incoming_attribute.per_instance) {
      std::ostringstream detail;
      detail << "attribute metadata differs (location "
             << existing_attribute.location << " vs "
             << incoming_attribute.location << ", type "
             << type_signature(existing_attribute.type) << " vs "
             << type_signature(incoming_attribute.type) << ", offset "
             << existing_attribute.offset << " vs " << incoming_attribute.offset
             << ", stride " << existing_attribute.stride << " vs "
             << incoming_attribute.stride << ", per_instance "
             << (existing_attribute.per_instance ? "true" : "false") << " vs "
             << (incoming_attribute.per_instance ? "true" : "false") << ")";
      push_conflict(
          errors,
          "vertex attribute",
          incoming_attribute.logical_name,
          existing_source,
          incoming_source,
          detail.str()
      );
      return false;
    }
  }

  rebuild_pipeline_layout(merged);
  return true;
}

} // namespace astralix
