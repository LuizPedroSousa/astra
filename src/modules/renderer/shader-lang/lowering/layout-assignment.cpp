#include "shader-lang/lowering/layout-assignment.hpp"

#include "fnv1a.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

namespace astralix {

namespace {

bool is_sampler_type(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return true;
    default:
      return false;
  }
}

bool is_loose_global_value_resource(const ResourceReflection &resource) {
  return resource.kind == ShaderResourceKind::UniformValue &&
         !is_sampler_type(resource.type.kind);
}

void canonicalize_loose_global_value_bindings(ResourceReflection &resource) {
  if (!is_loose_global_value_resource(resource)) {
    return;
  }

  resource.binding_id = shader_binding_id("__globals." + resource.logical_name);
  for (auto &member : resource.members) {
    member.binding_id = shader_binding_id("__globals." + member.logical_name);
  }
}

uint32_t std140_base_alignment(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeBool:
    case TokenKind::TypeInt:
    case TokenKind::TypeFloat:
      return 4;
    case TokenKind::TypeVec2:
      return 8;
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
      return 16;
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return 16;
    default:
      return 4;
  }
}

uint32_t std140_scalar_size(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeBool:
    case TokenKind::TypeInt:
    case TokenKind::TypeFloat:
      return 4;
    case TokenKind::TypeVec2:
      return 8;
    case TokenKind::TypeVec3:
      return 12;
    case TokenKind::TypeVec4:
      return 16;
    case TokenKind::TypeMat3:
      return 48;
    case TokenKind::TypeMat4:
      return 64;
    default:
      return 4;
  }
}

uint32_t std140_matrix_stride(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return 16;
    default:
      return 0;
  }
}

uint32_t align_up(uint32_t offset, uint32_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}

uint32_t finalize_std140_block_size(uint32_t size) {
  return align_up(size, 16);
}

uint32_t max_field_end_offset(const std::vector<ShaderValueFieldDesc> &fields) {
  uint32_t end_offset = 0;

  for (const auto &field : fields) {
    end_offset = std::max(end_offset, field.offset + field.size);
  }

  return end_offset;
}

uint32_t value_field_alignment(const ShaderValueFieldDesc &field) {
  if (field.array_stride > 0 || field.matrix_stride > 0) {
    return 16;
  }

  return std140_base_alignment(field.type.kind);
}

TypeRef strip_array_type(TypeRef type) {
  type.array_size.reset();
  type.is_runtime_sized = false;
  return type;
}

std::string indexed_logical_name(std::string_view logical_name, uint32_t index) {
  return std::string(logical_name) + "[" + std::to_string(index) + "]";
}

std::string exact_child_logical_name(
    std::string_view parent_logical_name,
    std::string_view exact_parent_logical_name,
    std::string_view child_logical_name
) {
  if (child_logical_name.rfind(parent_logical_name, 0) != 0) {
    return std::string(child_logical_name);
  }

  return std::string(exact_parent_logical_name) +
         std::string(child_logical_name.substr(parent_logical_name.size()));
}

struct Std140LayoutInfo {
  uint32_t alignment = 4;
  uint32_t size = 0;
};

struct ValueFieldCollector {
  std::vector<ShaderValueFieldDesc> fields;
  uint32_t current_offset = 0;

  static Std140LayoutInfo
  measure_struct_fields(const std::vector<DeclaredFieldReflection> &declared_fields) {
    uint32_t struct_offset = 0;
    uint32_t max_alignment = 0;

    for (const auto &field : declared_fields) {
      if (is_sampler_type(field.type.kind)) {
        continue;
      }

      const auto field_layout = measure_field(field);
      struct_offset = align_up(struct_offset, field_layout.alignment);
      struct_offset += field_layout.size;
      max_alignment = std::max(max_alignment, field_layout.alignment);
    }

    const uint32_t struct_alignment = align_up(std::max(max_alignment, 1u), 16);
    return Std140LayoutInfo{
        .alignment = struct_alignment,
        .size = align_up(struct_offset, struct_alignment),
    };
  }

  static Std140LayoutInfo measure_field(const DeclaredFieldReflection &field) {
    if (!field.fields.empty()) {
      const auto element_layout = measure_struct_fields(field.fields);
      if (!field.array_size.has_value()) {
        return element_layout;
      }

      const uint32_t stride = align_up(element_layout.size, 16);
  return Std140LayoutInfo{
      .alignment = 16,
      .size = stride * *field.array_size,
      };
    }

    const uint32_t size = std140_scalar_size(field.type.kind);
    if (!field.array_size.has_value()) {
      return Std140LayoutInfo{
          .alignment = std140_base_alignment(field.type.kind),
          .size = size,
      };
    }

    const uint32_t stride = align_up(size, 16);
    return Std140LayoutInfo{
        .alignment = 16,
        .size = stride * *field.array_size,
    };
  }

  void append_field(
      const DeclaredFieldReflection &field,
      uint32_t stage_mask,
      std::string_view exact_logical_name,
      std::string_view binding_logical_name,
      uint32_t base_offset
  ) {
    const uint32_t field_stage_mask = stage_mask | field.active_stage_mask;

    if (!field.fields.empty()) {
      append_struct_field(
          field, field_stage_mask, exact_logical_name, binding_logical_name,
          base_offset
      );
      return;
    }

    const TypeRef element_type = strip_array_type(field.type);
    const uint32_t element_size = std140_scalar_size(element_type.kind);
    const uint32_t matrix_stride = std140_matrix_stride(element_type.kind);

    if (field.array_size.has_value()) {
      const uint32_t stride = align_up(element_size, 16);
      for (uint32_t i = 0; i < *field.array_size; ++i) {
        const std::string indexed_exact_name =
            indexed_logical_name(exact_logical_name, i);
        const std::string indexed_binding_name =
            indexed_logical_name(binding_logical_name, i);
        fields.push_back(ShaderValueFieldDesc{
            .binding_id = shader_binding_id(indexed_binding_name),
            .logical_name = indexed_exact_name,
            .type = element_type,
            .stage_mask = field_stage_mask,
            .offset = base_offset + (i * stride),
            .size = element_size,
            .array_stride = stride,
            .matrix_stride = matrix_stride,
        });
      }
      return;
    }

    fields.push_back(ShaderValueFieldDesc{
        .binding_id = shader_binding_id(std::string(binding_logical_name)),
        .logical_name = std::string(exact_logical_name),
        .type = element_type,
        .stage_mask = field_stage_mask,
        .offset = base_offset,
        .size = element_size,
        .array_stride = 0,
        .matrix_stride = matrix_stride,
    });
  }

  void append_struct_field(
      const DeclaredFieldReflection &field,
      uint32_t stage_mask,
      std::string_view exact_logical_name,
      std::string_view binding_logical_name,
      uint32_t base_offset
  ) {
    const auto element_layout = measure_struct_fields(field.fields);

    if (field.array_size.has_value()) {
      const uint32_t stride = align_up(element_layout.size, 16);
      for (uint32_t i = 0; i < *field.array_size; ++i) {
        append_struct_instance(
            field, stage_mask, indexed_logical_name(exact_logical_name, i),
            indexed_logical_name(binding_logical_name, i),
            base_offset + (i * stride)
        );
      }
      return;
    }

    append_struct_instance(
        field, stage_mask, std::string(exact_logical_name),
        std::string(binding_logical_name), base_offset
    );
  }

  void append_struct_instance(
      const DeclaredFieldReflection &field,
      uint32_t stage_mask,
      const std::string &exact_logical_name,
      const std::string &binding_logical_name,
      uint32_t base_offset
  ) {
    uint32_t struct_offset = 0;

    for (const auto &child : field.fields) {
      if (is_sampler_type(child.type.kind)) {
        continue;
      }

      const auto child_layout = measure_field(child);
      struct_offset = align_up(struct_offset, child_layout.alignment);

      append_field(
          child, stage_mask,
          exact_child_logical_name(
              field.logical_name, exact_logical_name, child.logical_name
          ),
          exact_child_logical_name(
              field.logical_name, binding_logical_name, child.logical_name
          ),
          base_offset + struct_offset
      );

      struct_offset += child_layout.size;
    }
  }

  void collect_field(
      const DeclaredFieldReflection &field,
      uint32_t stage_mask,
      std::string_view exact_logical_name,
      std::string_view binding_logical_name
  ) {
    const auto field_layout = measure_field(field);
    current_offset = align_up(current_offset, field_layout.alignment);
    append_field(
        field, stage_mask, exact_logical_name, binding_logical_name,
        current_offset
    );
    current_offset += field_layout.size;
  }

  void collect_from_declared_fields(
      const std::vector<DeclaredFieldReflection> &declared_fields,
      uint32_t stage_mask
  ) {
    for (const auto &field : declared_fields) {
      if (is_sampler_type(field.type.kind)) {
        continue;
      }

      collect_field(field, stage_mask, field.logical_name, field.logical_name);
    }
  }
};

struct ResourceCollector {
  std::vector<ShaderResourceBindingDesc> resources;

  void collect_from_declared_fields(
      const std::vector<DeclaredFieldReflection> &declared_fields,
      uint32_t stage_mask
  ) {
    for (const auto &field : declared_fields) {
      if (!is_sampler_type(field.type.kind)) {
        if (!field.fields.empty()) {
          collect_from_declared_fields(field.fields, stage_mask | field.active_stage_mask);
        }
        continue;
      }

      resources.push_back(ShaderResourceBindingDesc{
          .binding_id = field.binding_id,
          .logical_name = field.logical_name,
          .source_kind = ShaderResourceBindingKind::Sampler,
          .type = field.type,
          .stage_mask = stage_mask | field.active_stage_mask,
      });
    }
  }
};

void assign_bindings_deterministic(
    std::vector<ShaderResourceBindingDesc> &resources,
    std::set<std::pair<uint32_t, uint32_t>> &reserved_slots,
    const std::set<std::string> &pre_assigned
) {
  std::map<uint32_t, uint32_t> next_binding_per_set;

  for (const auto &[set, binding] : reserved_slots) {
    auto &next = next_binding_per_set[set];
    if (binding >= next) {
      next = binding + 1;
    }
  }

  std::sort(resources.begin(), resources.end(), [](const ShaderResourceBindingDesc &lhs, const ShaderResourceBindingDesc &rhs) {
    return lhs.logical_name < rhs.logical_name;
  });

  for (auto &resource : resources) {
    if (pre_assigned.count(resource.logical_name) > 0) {
      continue;
    }

    uint32_t set = resource.descriptor_set;
    auto &next = next_binding_per_set[set];

    while (reserved_slots.count({set, next}) > 0) {
      ++next;
    }

    resource.binding = next;
    reserved_slots.insert({set, next});
    ++next;
  }
}

uint64_t compute_resource_layout_hash(const ShaderResourceLayout &layout) {
  uint64_t hash = k_fnv1a64_offset_basis;

  auto append_u32 = [&](uint32_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  auto append_u64 = [&](uint64_t value) {
    hash = fnv1a64_append_value(hash, value);
  };

  auto append_str = [&](const std::string &value) {
    hash = fnv1a64_append_string(value, hash);
  };

  for (const auto &block : layout.value_blocks) {
    append_str(block.logical_name);
    append_u64(block.block_id);
    append_u32(block.size);

    for (const auto &field : block.fields) {
      append_u64(field.binding_id);
      append_u32(field.offset);
      append_u32(field.size);
      append_u32(static_cast<uint32_t>(field.type.kind));
    }
  }

  for (const auto &resource : layout.resources) {
    append_u64(resource.binding_id);
    append_u32(resource.descriptor_set);
    append_u32(resource.binding);
    append_u32(static_cast<uint32_t>(resource.source_kind));
    append_u32(static_cast<uint32_t>(resource.type.kind));
    append_u32(resource.stage_mask);
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

bool apply_block_descriptor_set(
    ShaderValueBlockDesc &block,
    const ResourceReflection &resource,
    LayoutAssignmentResult &result
) {
  if (!resource.glsl.descriptor_set.has_value()) {
    return true;
  }

  if (block.descriptor_set.has_value() &&
      block.descriptor_set != resource.glsl.descriptor_set) {
    result.errors.push_back(
        "Conflicting descriptor sets for value block '" + block.logical_name +
        "': existing set " + std::to_string(*block.descriptor_set) +
        " conflicts with resource '" + resource.logical_name + "' set " +
        std::to_string(*resource.glsl.descriptor_set)
    );
    return false;
  }

  block.descriptor_set = resource.glsl.descriptor_set;
  return true;
}

void apply_cross_stage_value_block_layout(
    ShaderValueBlockDesc &block,
    std::vector<ShaderValueFieldDesc> &cross_stage_fields
) {
  if (cross_stage_fields.empty()) {
    cross_stage_fields = block.fields;
    return;
  }

  std::vector<ShaderValueFieldDesc *> new_fields;

  for (auto &field : block.fields) {
    auto field_it = std::find_if(
        cross_stage_fields.begin(),
        cross_stage_fields.end(),
        [&](const ShaderValueFieldDesc &cross_stage_field) {
          return cross_stage_field.logical_name == field.logical_name;
        }
    );

    if (field_it != cross_stage_fields.end()) {
      field.offset = field_it->offset;
      field_it->stage_mask |= field.stage_mask;
      continue;
    }

    new_fields.push_back(&field);
  }

  if (!new_fields.empty()) {
    const auto first_new_field_it = std::min_element(
        new_fields.begin(), new_fields.end(),
        [](const ShaderValueFieldDesc *lhs, const ShaderValueFieldDesc *rhs) {
          return lhs->offset < rhs->offset;
        }
    );
    const uint32_t local_base_offset = (*first_new_field_it)->offset;
    uint32_t cross_stage_base_offset =
        max_field_end_offset(cross_stage_fields);
    while (!std::all_of(
        new_fields.begin(), new_fields.end(),
        [&](const ShaderValueFieldDesc *field) {
          const uint32_t shifted_offset =
              cross_stage_base_offset + (field->offset - local_base_offset);
          return shifted_offset % value_field_alignment(*field) == 0;
        }
    )) {
      ++cross_stage_base_offset;
    }

    for (auto *field : new_fields) {
      field->offset =
          cross_stage_base_offset + (field->offset - local_base_offset);
    }
  }

  for (const auto *field : new_fields) {
    cross_stage_fields.push_back(*field);
  }

  std::sort(
      block.fields.begin(),
      block.fields.end(),
      [](const ShaderValueFieldDesc &lhs, const ShaderValueFieldDesc &rhs) {
        if (lhs.offset != rhs.offset) {
          return lhs.offset < rhs.offset;
        }
        return lhs.logical_name < rhs.logical_name;
      }
  );
  std::sort(
      cross_stage_fields.begin(),
      cross_stage_fields.end(),
      [](const ShaderValueFieldDesc &lhs, const ShaderValueFieldDesc &rhs) {
        if (lhs.offset != rhs.offset) {
          return lhs.offset < rhs.offset;
        }
        return lhs.logical_name < rhs.logical_name;
      }
  );

  block.size =
      finalize_std140_block_size(max_field_end_offset(cross_stage_fields));
}

} // namespace

LayoutAssignmentResult
LayoutAssignment::assign(const StageReflection &reflection) {
  LayoutAssignmentResult result;
  result.reflection = reflection;

  std::set<std::pair<uint32_t, uint32_t>> reserved_slots = m_cross_stage_reserved_slots;

  for (auto &resource : result.reflection.resources) {
    if (!resource.glsl.descriptor_set) {
      resource.glsl.descriptor_set = 0;
    }

    if (resource.binding_id == 0 && !resource.logical_name.empty()) {
      resource.binding_id = shader_binding_id(resource.logical_name);
    }

    canonicalize_loose_global_value_bindings(resource);

    if (resource.glsl.descriptor_set && resource.glsl.binding) {
      reserved_slots.insert(
          {*resource.glsl.descriptor_set, *resource.glsl.binding}
      );
    }
  }

  std::unordered_map<std::string, ShaderValueBlockDesc> value_blocks_by_name;
  std::vector<ShaderResourceBindingDesc> all_resources;

  for (const auto &resource : result.reflection.resources) {
    if (resource.kind == ShaderResourceKind::UniformInterface) {
      ValueFieldCollector value_collector;
      value_collector.collect_from_declared_fields(
          resource.declared_fields,
          shader_stage_mask(reflection.stage)
      );

      if (!value_collector.fields.empty()) {
        auto &block = value_blocks_by_name[resource.logical_name];
        if (block.logical_name.empty()) {
          block.logical_name = resource.logical_name;
          block.block_id = shader_binding_id(resource.logical_name + ".__block");
        }
        if (!apply_block_descriptor_set(block, resource, result)) {
          return result;
        }
        block.fields = std::move(value_collector.fields);
        block.size = finalize_std140_block_size(value_collector.current_offset);
      }

      ResourceCollector resource_collector;
      resource_collector.collect_from_declared_fields(
          resource.declared_fields,
          shader_stage_mask(reflection.stage)
      );

      for (auto &resource_binding : resource_collector.resources) {
        resource_binding.descriptor_set =
            resource.glsl.descriptor_set.value_or(0);
        all_resources.push_back(std::move(resource_binding));
      }

      continue;
    }

    if (resource.kind == ShaderResourceKind::Sampler) {
      all_resources.push_back(ShaderResourceBindingDesc{
          .binding_id = resource.binding_id,
          .logical_name = resource.logical_name,
          .source_kind = ShaderResourceBindingKind::Sampler,
          .type = resource.type,
          .stage_mask = shader_stage_mask(reflection.stage),
          .descriptor_set = resource.glsl.descriptor_set.value_or(0),
      });
      continue;
    }

    if (resource.kind == ShaderResourceKind::UniformValue) {
      if (is_sampler_type(resource.type.kind)) {
        all_resources.push_back(ShaderResourceBindingDesc{
            .binding_id = resource.binding_id,
            .logical_name = resource.logical_name,
            .source_kind = ShaderResourceBindingKind::Sampler,
            .type = resource.type,
            .stage_mask = shader_stage_mask(reflection.stage),
            .descriptor_set = resource.glsl.descriptor_set.value_or(0),
        });
      } else {
        auto &block = value_blocks_by_name["__globals"];
        if (block.logical_name.empty()) {
          block.logical_name = "__globals";
          block.block_id = shader_binding_id("__globals.__block");
        }
        if (!apply_block_descriptor_set(block, resource, result)) {
          return result;
        }

        const std::string globals_field_binding_name =
            "__globals." + resource.logical_name;
        DeclaredFieldReflection field;
        field.name = resource.declared_name.empty() ? resource.logical_name
                                                    : resource.declared_name;
        field.logical_name = resource.logical_name;
        field.type = resource.type;
        field.array_size = resource.array_size;
        field.active_stage_mask = shader_stage_mask(reflection.stage);
        field.binding_id = shader_binding_id(globals_field_binding_name);

        ValueFieldCollector value_collector;
        value_collector.current_offset = max_field_end_offset(block.fields);
        value_collector.collect_field(
            field, shader_stage_mask(reflection.stage), resource.logical_name,
            globals_field_binding_name
        );

        block.fields.insert(
            block.fields.end(), value_collector.fields.begin(),
            value_collector.fields.end()
        );
        block.size = finalize_std140_block_size(value_collector.current_offset);
      }
      continue;
    }

    if (resource.kind == ShaderResourceKind::UniformBlock) {
      all_resources.push_back(ShaderResourceBindingDesc{
          .binding_id = resource.binding_id,
          .logical_name = resource.logical_name,
          .source_kind = ShaderResourceBindingKind::UniformBlock,
          .type = resource.type,
          .stage_mask = shader_stage_mask(reflection.stage),
          .descriptor_set = resource.glsl.descriptor_set.value_or(0),
          .binding = resource.glsl.binding.value_or(0),
          .array_size = resource.array_size,
      });
      continue;
    }

    if (resource.kind == ShaderResourceKind::StorageBuffer) {
      all_resources.push_back(ShaderResourceBindingDesc{
          .binding_id = resource.binding_id,
          .logical_name = resource.logical_name,
          .source_kind = ShaderResourceBindingKind::StorageBuffer,
          .type = resource.type,
          .stage_mask = shader_stage_mask(reflection.stage),
          .descriptor_set = resource.glsl.descriptor_set.value_or(0),
          .binding = resource.glsl.binding.value_or(0),
          .array_size = resource.array_size,
      });
      continue;
    }
  }

  std::set<std::string> cross_stage_pre_assigned;
  for (auto &resource : all_resources) {
    auto it = m_cross_stage_resource_bindings.find(resource.logical_name);
    if (it != m_cross_stage_resource_bindings.end()) {
      resource.binding = it->second;
      reserved_slots.insert({resource.descriptor_set, resource.binding});
      cross_stage_pre_assigned.insert(resource.logical_name);
    }
  }

  assign_bindings_deterministic(all_resources, reserved_slots, cross_stage_pre_assigned);

  for (const auto &resource : all_resources) {
    m_cross_stage_resource_bindings[resource.logical_name] = resource.binding;
  }

  std::vector<ShaderValueBlockDesc> ordered_value_blocks;
  for (auto &[name, block] : value_blocks_by_name) {
    ordered_value_blocks.push_back(std::move(block));
  }
  std::sort(ordered_value_blocks.begin(), ordered_value_blocks.end(), [](const ShaderValueBlockDesc &lhs, const ShaderValueBlockDesc &rhs) {
    return lhs.logical_name < rhs.logical_name;
  });

  for (auto &block : ordered_value_blocks) {
    apply_cross_stage_value_block_layout(
        block, m_cross_stage_value_block_fields[block.logical_name]
    );
  }

  {
    std::map<uint32_t, uint32_t> next_binding_per_set;
    for (const auto &[set, binding] : reserved_slots) {
      auto &next = next_binding_per_set[set];
      if (binding >= next)
        next = binding + 1;
    }
    for (auto &block : ordered_value_blocks) {
      auto it = m_cross_stage_block_bindings.find(block.logical_name);
      if (it != m_cross_stage_block_bindings.end()) {
        block.descriptor_set = block.descriptor_set.value_or(0);
        block.binding = it->second;
        reserved_slots.insert(
            {block.descriptor_set.value_or(0), block.binding.value()}
        );
      } else {
        uint32_t set = block.descriptor_set.value_or(0);
        auto &next = next_binding_per_set[set];
        while (reserved_slots.count({set, next}) > 0)
          ++next;
        block.descriptor_set = set;
        block.binding = next;
        reserved_slots.insert({set, next});
        ++next;
      }
      m_cross_stage_block_bindings[block.logical_name] = block.binding.value();
    }
  }

  m_cross_stage_reserved_slots = reserved_slots;

  result.layout.resource_layout.value_blocks = ordered_value_blocks;
  result.layout.resource_layout.resources = all_resources;
  result.layout.resource_layout.compatibility_hash =
      compute_resource_layout_hash(result.layout.resource_layout);

  if (reflection.stage == StageKind::Vertex) {
    uint32_t vertex_location = 0;
    for (const auto &input : reflection.stage_inputs) {
      uint32_t location =
          input.glsl.location.has_value() ? *input.glsl.location : vertex_location;

      result.layout.vertex_input.attributes.push_back(VertexAttributeDesc{
          .logical_name = input.logical_name,
          .location = location,
          .type = input.type,
      });

      uint32_t location_slots = 1;
      if (input.type.kind == TokenKind::TypeMat3) {
        location_slots = 3;
      } else if (input.type.kind == TokenKind::TypeMat4) {
        location_slots = 4;
      }

      vertex_location = location + location_slots;
    }
  }

  result.layout.vertex_input.compatibility_hash =
      compute_vertex_input_hash(result.layout.vertex_input);

  std::map<uint32_t, DescriptorSetLayoutDesc> descriptor_sets;
  for (const auto &resource : all_resources) {
    auto &set_desc = descriptor_sets[resource.descriptor_set];
    set_desc.set = resource.descriptor_set;
    set_desc.bindings.push_back(resource);
  }

  for (auto &[set_index, set_desc] : descriptor_sets) {
    result.layout.pipeline_layout.descriptor_sets.push_back(
        std::move(set_desc)
    );
  }
  result.layout.pipeline_layout.value_blocks = ordered_value_blocks;
  result.layout.pipeline_layout.compatibility_hash =
      compute_pipeline_layout_hash(result.layout.pipeline_layout);

  return result;
}

} // namespace astralix
