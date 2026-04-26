#include "shader-lang/emitters/vulkan-spirv-emitter.hpp"

#include <set>
#include <spirv/unified1/GLSL.std.450.h>

namespace astralix {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

uint32_t base_alignment_for_type(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeFloat:
    case TokenKind::TypeInt:
    case TokenKind::TypeUint:
    case TokenKind::TypeBool:
      return 4;
    case TokenKind::TypeVec2:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeUvec2:
      return 8;
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeIvec4:
    case TokenKind::TypeUvec3:
    case TokenKind::TypeUvec4:
    case TokenKind::TypeMat2:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return 16;
    default:
      return 4;
  }
}

uint32_t scalar_size_for_type(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeFloat:
    case TokenKind::TypeInt:
    case TokenKind::TypeUint:
    case TokenKind::TypeBool:
      return 4;
    case TokenKind::TypeVec2:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeUvec2:
      return 8;
    case TokenKind::TypeVec3:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeUvec3:
      return 12;
    case TokenKind::TypeVec4:
    case TokenKind::TypeIvec4:
    case TokenKind::TypeUvec4:
      return 16;
    case TokenKind::TypeMat2:
      return 32;
    case TokenKind::TypeMat3:
      return 48;
    case TokenKind::TypeMat4:
      return 64;
    default:
      return 4;
  }
}

bool ends_with_terminator(const CanonicalStmt &stmt) {
  if (std::holds_alternative<CanonicalReturnStmt>(stmt.data) ||
      std::holds_alternative<CanonicalDiscardStmt>(stmt.data) ||
      std::holds_alternative<CanonicalBreakStmt>(stmt.data) ||
      std::holds_alternative<CanonicalContinueStmt>(stmt.data))
    return true;
  if (const auto *block = std::get_if<CanonicalBlockStmt>(&stmt.data)) {
    if (!block->stmts.empty())
      return ends_with_terminator(*block->stmts.back());
  }
  return false;
}

} // namespace

void VulkanSPIRVEmitter::add_error(std::string message) {
  m_errors.push_back(std::move(message));
}

bool VulkanSPIRVEmitter::is_float_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeFloat:
    case TokenKind::TypeVec2:
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeMat2:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return true;
    default:
      return false;
  }
}

bool VulkanSPIRVEmitter::is_int_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeInt:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeIvec4:
      return true;
    default:
      return false;
  }
}

bool VulkanSPIRVEmitter::is_uint_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeUint:
    case TokenKind::TypeUvec2:
    case TokenKind::TypeUvec3:
    case TokenKind::TypeUvec4:
      return true;
    default:
      return false;
  }
}

bool VulkanSPIRVEmitter::is_vector_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeVec2:
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeIvec4:
    case TokenKind::TypeUvec2:
    case TokenKind::TypeUvec3:
    case TokenKind::TypeUvec4:
      return true;
    default:
      return false;
  }
}

bool VulkanSPIRVEmitter::is_matrix_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeMat2:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return true;
    default:
      return false;
  }
}

bool VulkanSPIRVEmitter::is_sampler_type(const TypeRef &type) const {
  switch (type.kind) {
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

bool VulkanSPIRVEmitter::is_scalar_type(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeBool:
    case TokenKind::TypeInt:
    case TokenKind::TypeUint:
    case TokenKind::TypeFloat:
      return true;
    default:
      return false;
  }
}

uint32_t VulkanSPIRVEmitter::vector_component_count(const TypeRef &type) const {
  switch (type.kind) {
    case TokenKind::TypeVec2:
    case TokenKind::TypeIvec2:
    case TokenKind::TypeUvec2:
      return 2;
    case TokenKind::TypeVec3:
    case TokenKind::TypeIvec3:
    case TokenKind::TypeUvec3:
      return 3;
    case TokenKind::TypeVec4:
    case TokenKind::TypeIvec4:
    case TokenKind::TypeUvec4:
      return 4;
    default:
      return 0;
  }
}

uint32_t VulkanSPIRVEmitter::swizzle_index(char component) const {
  switch (component) {
    case 'x':
    case 'r':
    case 's':
      return 0;
    case 'y':
    case 'g':
    case 't':
      return 1;
    case 'z':
    case 'b':
    case 'p':
      return 2;
    case 'w':
    case 'a':
    case 'q':
      return 3;
    default:
      return 0;
  }
}

bool VulkanSPIRVEmitter::is_swizzle(const std::string &field, const TypeRef &object_type) const {
  if (!is_vector_type(object_type) && !is_scalar_type(object_type))
    return false;
  if (field.empty() || field.size() > 4)
    return false;
  for (char c : field) {
    if (c != 'x' && c != 'y' && c != 'z' && c != 'w' && c != 'r' &&
        c != 'g' && c != 'b' && c != 'a' && c != 's' && c != 't' &&
        c != 'p' && c != 'q')
      return false;
  }
  return true;
}

void VulkanSPIRVEmitter::setup_module(StageKind stage) {
  m_builder.add_capability(spv::CapabilityShader);
  m_builder.set_memory_model(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
  m_glsl_ext = m_builder.import_extended_instruction_set("GLSL.std.450");

  m_void_type = m_builder.register_void();
  m_bool_type = m_builder.register_bool();
  m_int_type = m_builder.register_int(32, true);
  m_uint_type = m_builder.register_int(32, false);
  m_float_type = m_builder.register_float(32);
}

uint32_t VulkanSPIRVEmitter::resolve_element_type(const TypeRef &type) {
  TypeRef element = type;
  element.array_size = std::nullopt;
  element.is_runtime_sized = false;
  return resolve_type(element);
}

uint32_t VulkanSPIRVEmitter::resolve_type(const TypeRef &type) {
  uint32_t base_type = 0;

  switch (type.kind) {
    case TokenKind::KeywordVoid:
      base_type = m_void_type;
      break;
    case TokenKind::TypeBool:
      base_type = m_bool_type;
      break;
    case TokenKind::TypeInt:
      base_type = m_int_type;
      break;
    case TokenKind::TypeUint:
      base_type = m_uint_type;
      break;
    case TokenKind::TypeFloat:
      base_type = m_float_type;
      break;
    case TokenKind::TypeVec2:
      base_type = m_builder.register_vector(m_float_type, 2);
      break;
    case TokenKind::TypeVec3:
      base_type = m_builder.register_vector(m_float_type, 3);
      break;
    case TokenKind::TypeVec4:
      base_type = m_builder.register_vector(m_float_type, 4);
      break;
    case TokenKind::TypeIvec2:
      base_type = m_builder.register_vector(m_int_type, 2);
      break;
    case TokenKind::TypeIvec3:
      base_type = m_builder.register_vector(m_int_type, 3);
      break;
    case TokenKind::TypeIvec4:
      base_type = m_builder.register_vector(m_int_type, 4);
      break;
    case TokenKind::TypeUvec2:
      base_type = m_builder.register_vector(m_uint_type, 2);
      break;
    case TokenKind::TypeUvec3:
      base_type = m_builder.register_vector(m_uint_type, 3);
      break;
    case TokenKind::TypeUvec4:
      base_type = m_builder.register_vector(m_uint_type, 4);
      break;
    case TokenKind::TypeMat2: {
      auto column = m_builder.register_vector(m_float_type, 2);
      base_type = m_builder.register_matrix(column, 2);
      break;
    }
    case TokenKind::TypeMat3: {
      auto column = m_builder.register_vector(m_float_type, 3);
      base_type = m_builder.register_matrix(column, 3);
      break;
    }
    case TokenKind::TypeMat4: {
      auto column = m_builder.register_vector(m_float_type, 4);
      base_type = m_builder.register_matrix(column, 4);
      break;
    }
    case TokenKind::TypeSampler2D: {
      auto image = m_builder.register_image(m_float_type, spv::Dim2D, 0, 0, 0, 1, spv::ImageFormatUnknown);
      base_type = m_builder.register_sampled_image(image);
      break;
    }
    case TokenKind::TypeSamplerCube: {
      auto image = m_builder.register_image(m_float_type, spv::DimCube, 0, 0, 0, 1, spv::ImageFormatUnknown);
      base_type = m_builder.register_sampled_image(image);
      break;
    }
    case TokenKind::TypeSampler2DShadow: {
      auto image = m_builder.register_image(m_float_type, spv::Dim2D, 1, 0, 0, 1, spv::ImageFormatUnknown);
      base_type = m_builder.register_sampled_image(image);
      break;
    }
    case TokenKind::TypeIsampler2D: {
      auto image = m_builder.register_image(m_int_type, spv::Dim2D, 0, 0, 0, 1, spv::ImageFormatUnknown);
      base_type = m_builder.register_sampled_image(image);
      break;
    }
    case TokenKind::TypeUsampler2D: {
      auto image = m_builder.register_image(m_uint_type, spv::Dim2D, 0, 0, 0, 1, spv::ImageFormatUnknown);
      base_type = m_builder.register_sampled_image(image);
      break;
    }
    case TokenKind::Identifier: {
      auto it = m_struct_type_ids.find(type.name);
      if (it != m_struct_type_ids.end())
        base_type = it->second;
      break;
    }
    default:
      break;
  }

  if (base_type == 0)
    return 0;

  if (type.is_runtime_sized) {
    return m_builder.register_runtime_array(base_type);
  }

  if (type.array_size.has_value()) {
    uint32_t length_id =
        m_builder.constant_uint(*type.array_size, m_uint_type);
    return m_builder.register_array(base_type, length_id);
  }

  return base_type;
}

void VulkanSPIRVEmitter::emit_struct_types(
    const std::vector<CanonicalStructDecl> &structs
) {
  for (const auto &struct_decl : structs) {
    std::vector<uint32_t> member_types;
    std::vector<std::string> member_names;
    std::vector<uint32_t> member_type_ids;

    for (const auto &field : struct_decl.fields) {
      if (is_sampler_type(field.type))
        continue;
      uint32_t field_type = resolve_type(field.type);
      member_types.push_back(field_type);
      member_names.push_back(field.name);
      member_type_ids.push_back(field_type);
    }

    uint32_t struct_id = m_builder.register_struct(member_types);
    m_builder.set_name(struct_id, struct_decl.name);

    for (uint32_t i = 0; i < member_names.size(); ++i) {
      m_builder.set_member_name(struct_id, i, member_names[i]);
    }

    m_struct_type_ids[struct_decl.name] = struct_id;
    m_struct_field_names[struct_decl.name] = std::move(member_names);
    m_struct_field_type_ids[struct_decl.name] = std::move(member_type_ids);

    for (const auto &field : struct_decl.fields) {
      m_named_type_refs[struct_decl.name + "." + field.name] = field.type;
    }
  }
}

void VulkanSPIRVEmitter::emit_input_variables(
    const StageReflection &reflection, const CanonicalEntryPoint &entry
) {
  std::unordered_map<std::string, const StageFieldReflection *> input_by_name;
  for (const auto &input : reflection.stage_inputs) {
    input_by_name[input.logical_name] = &input;
  }

  uint32_t auto_location = 0;
  for (const auto &binding : entry.varying_inputs) {
    for (const auto &field : binding.fields) {
      uint32_t type_id = resolve_type(field.type);
      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassInput, type_id);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassInput);

      std::string key = binding.param_name + "." + field.name;
      m_builder.set_name(variable_id, key);

      auto it = input_by_name.find(key);
      if (it != input_by_name.end() && it->second->glsl.location.has_value()) {
        m_builder.decorate(variable_id, spv::DecorationLocation, *it->second->glsl.location);
      } else {
        m_builder.decorate(variable_id, spv::DecorationLocation, auto_location);
      }

      uint32_t location_slots = 1;
      if (field.type.kind == TokenKind::TypeMat3)
        location_slots = 3;
      else if (field.type.kind == TokenKind::TypeMat4)
        location_slots = 4;
      auto_location += location_slots;

      m_input_variables[key] = {variable_id, type_id, spv::StorageClassInput};
      m_named_type_refs[key] = field.type;
      m_interface_variable_ids.push_back(variable_id);
    }
  }
}

void VulkanSPIRVEmitter::emit_output_variables(
    const StageReflection &reflection, const CanonicalEntryPoint &entry
) {
  if (!entry.output)
    return;

  std::vector<uint32_t> struct_member_types;
  std::vector<std::string> struct_member_names;
  std::vector<uint32_t> struct_member_type_ids;

  uint32_t output_location = 0;
  for (const auto &field : entry.output->fields) {
    uint32_t type_id = resolve_type(field.type);
    uint32_t pointer_type =
        m_builder.register_pointer(spv::StorageClassOutput, type_id);
    uint32_t variable_id =
        m_builder.variable(pointer_type, spv::StorageClassOutput);

    m_builder.set_name(variable_id, field.name);
    m_builder.decorate(variable_id, spv::DecorationLocation, output_location);
    ++output_location;

    m_output_variables[field.name] = {variable_id, type_id, spv::StorageClassOutput};
    m_named_type_refs[field.name] = field.type;
    m_interface_variable_ids.push_back(variable_id);

    struct_member_types.push_back(type_id);
    struct_member_names.push_back(field.name);
    struct_member_type_ids.push_back(type_id);
    m_named_type_refs[entry.output->interface_name + "." + field.name] = field.type;
  }

  if (!struct_member_types.empty()) {
    uint32_t struct_id = m_builder.register_struct(struct_member_types);
    m_builder.set_name(struct_id, entry.output->interface_name);
    for (uint32_t i = 0; i < struct_member_names.size(); ++i)
      m_builder.set_member_name(struct_id, i, struct_member_names[i]);
    m_struct_type_ids[entry.output->interface_name] = struct_id;
    m_struct_field_names[entry.output->interface_name] = std::move(struct_member_names);
    m_struct_field_type_ids[entry.output->interface_name] = std::move(struct_member_type_ids);
  }
}

void VulkanSPIRVEmitter::emit_resource_variables(
    const StageReflection &reflection, const ShaderPipelineLayout &layout,
    const CanonicalEntryPoint &entry
) {
  std::set<std::pair<uint32_t, uint32_t>> used_bindings;
  for (const auto &resource : layout.resource_layout.resources) {
    used_bindings.insert({resource.descriptor_set, resource.binding});
  }

  auto next_binding = [&](uint32_t set) -> uint32_t {
    uint32_t binding = 0;
    while (used_bindings.count({set, binding}) > 0)
      ++binding;
    used_bindings.insert({set, binding});
    return binding;
  };

  for (const auto &value_block : layout.resource_layout.value_blocks) {
    std::vector<uint32_t> member_types;
    std::vector<std::string> field_logical_names;
    std::vector<uint32_t> field_type_ids;

    for (const auto &field : value_block.fields) {
      uint32_t field_type = field.type.kind == TokenKind::TypeBool
                                ? m_uint_type
                                : resolve_type(field.type);
      member_types.push_back(field_type);
      field_logical_names.push_back(field.logical_name);
      field_type_ids.push_back(field_type);
    }

    uint32_t struct_type = m_builder.register_struct(member_types);
    m_builder.set_name(struct_type, value_block.logical_name + "_Block");
    m_builder.decorate(struct_type, spv::DecorationBlock);

    for (uint32_t i = 0; i < value_block.fields.size(); ++i) {
      const auto &field = value_block.fields[i];
      m_builder.set_member_name(struct_type, i, field.logical_name);
      m_builder.member_decorate(struct_type, i, spv::DecorationOffset, field.offset);

      if (field.matrix_stride > 0) {
        m_builder.member_decorate(struct_type, i, spv::DecorationColMajor);
        m_builder.member_decorate(struct_type, i, spv::DecorationMatrixStride, field.matrix_stride);
      }
    }

    uint32_t pointer_type =
        m_builder.register_pointer(spv::StorageClassUniform, struct_type);
    uint32_t variable_id =
        m_builder.variable(pointer_type, spv::StorageClassUniform);

    uint32_t descriptor_set = value_block.descriptor_set.value_or(0);
    uint32_t binding = value_block.binding.has_value()
                           ? *value_block.binding
                           : next_binding(descriptor_set);

    m_builder.set_name(variable_id, value_block.logical_name);
    m_builder.decorate(variable_id, spv::DecorationDescriptorSet, descriptor_set);
    m_builder.decorate(variable_id, spv::DecorationBinding, binding);

    for (const auto &field : value_block.fields) {
      m_named_type_refs[field.logical_name] = field.type;
    }

    m_ubo_map[value_block.logical_name] = {variable_id, struct_type, pointer_type, std::move(field_logical_names), std::move(field_type_ids), spv::StorageClassUniform};
    m_interface_variable_ids.push_back(variable_id);
  }

  for (const auto &resource : layout.resource_layout.resources) {
    if (resource.source_kind == ShaderResourceBindingKind::Sampler) {
      uint32_t type_id = resolve_type(resource.type);
      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassUniformConstant, type_id);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassUniformConstant);

      m_builder.set_name(variable_id, resource.logical_name);
      m_builder.decorate(variable_id, spv::DecorationDescriptorSet, resource.descriptor_set);
      m_builder.decorate(variable_id, spv::DecorationBinding, resource.binding);

      m_sampler_variables[resource.logical_name] = {
          variable_id, type_id, spv::StorageClassUniformConstant
      };
      m_named_type_refs[resource.logical_name] = resource.type;
      m_interface_variable_ids.push_back(variable_id);
      continue;
    }

    if (resource.source_kind == ShaderResourceBindingKind::UniformBlock) {
      const ResourceReflection *refl_resource = nullptr;
      for (const auto &refl : reflection.resources) {
        if (refl.logical_name == resource.logical_name) {
          refl_resource = &refl;
          break;
        }
      }
      if (!refl_resource)
        continue;

      std::vector<uint32_t> member_types;
      std::vector<std::string> field_logical_names;
      std::vector<uint32_t> field_type_ids;

      for (const auto &member : refl_resource->members) {
        uint32_t field_type = member.type.kind == TokenKind::TypeBool
                                  ? m_uint_type
                                  : resolve_type(member.type);
        member_types.push_back(field_type);
        field_logical_names.push_back(member.logical_name);
        field_type_ids.push_back(field_type);
      }

      uint32_t struct_type = m_builder.register_struct(member_types);
      std::string block_name = refl_resource->declared_name;
      m_builder.set_name(struct_type, block_name);
      m_builder.decorate(struct_type, spv::DecorationBlock);

      uint32_t current_offset = 0;
      for (uint32_t i = 0; i < refl_resource->members.size(); ++i) {
        const auto &member = refl_resource->members[i];
        m_builder.set_member_name(struct_type, i, member.logical_name);

        uint32_t size = scalar_size_for_type(member.type.kind);
        uint32_t alignment = base_alignment_for_type(member.type.kind);
        current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
        m_builder.member_decorate(struct_type, i, spv::DecorationOffset, current_offset);

        if (is_matrix_type(member.type)) {
          m_builder.member_decorate(struct_type, i, spv::DecorationColMajor);
          m_builder.member_decorate(struct_type, i, spv::DecorationMatrixStride, 16);
        }

        current_offset += size;
        m_named_type_refs[member.logical_name] = member.type;
      }

      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassUniform, struct_type);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassUniform);

      m_builder.set_name(variable_id, resource.logical_name);
      m_builder.decorate(variable_id, spv::DecorationDescriptorSet, resource.descriptor_set);
      m_builder.decorate(variable_id, spv::DecorationBinding, resource.binding);

      m_ubo_map[resource.logical_name] = {variable_id, struct_type, pointer_type, std::move(field_logical_names), std::move(field_type_ids), spv::StorageClassUniform};
      m_interface_variable_ids.push_back(variable_id);
      continue;
    }

    if (resource.source_kind == ShaderResourceBindingKind::StorageBuffer) {
      std::string struct_name = resource.type.name;
      auto struct_fields_it = m_struct_field_names.find(struct_name);
      auto struct_types_it = m_struct_field_type_ids.find(struct_name);

      if (struct_fields_it == m_struct_field_names.end() ||
          struct_types_it == m_struct_field_type_ids.end())
        continue;

      const auto &canonical_field_names = struct_fields_it->second;
      const auto &canonical_field_type_ids = struct_types_it->second;

      std::vector<uint32_t> member_types;
      std::vector<std::string> field_logical_names;
      std::vector<uint32_t> field_type_ids;

      for (uint32_t i = 0; i < canonical_field_names.size(); ++i) {
        uint32_t field_type = canonical_field_type_ids[i];
        member_types.push_back(field_type);

        std::string logical_name = resource.logical_name + "." + canonical_field_names[i];
        field_logical_names.push_back(logical_name);
        field_type_ids.push_back(field_type);
      }

      uint32_t struct_type = m_builder.register_struct(member_types);
      m_builder.set_name(struct_type, struct_name + "_Block");
      m_builder.decorate(struct_type, spv::DecorationBlock);

      uint32_t current_offset = 0;
      for (uint32_t i = 0; i < canonical_field_names.size(); ++i) {
        m_builder.set_member_name(struct_type, i, canonical_field_names[i]);

        auto type_it = m_named_type_refs.find(struct_name + "." + canonical_field_names[i]);
        if (type_it != m_named_type_refs.end()) {
          const TypeRef &field_type_ref = type_it->second;
          uint32_t alignment = base_alignment_for_type(field_type_ref.kind);
          current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
          m_builder.member_decorate(struct_type, i, spv::DecorationOffset, current_offset);

          if (is_matrix_type(field_type_ref)) {
            m_builder.member_decorate(struct_type, i, spv::DecorationColMajor);
            m_builder.member_decorate(struct_type, i, spv::DecorationMatrixStride, 16);
          }

          if (field_type_ref.is_runtime_sized) {
            uint32_t stride = scalar_size_for_type(field_type_ref.kind);
            m_builder.decorate(canonical_field_type_ids[i], spv::DecorationArrayStride, stride);
          } else {
            current_offset += scalar_size_for_type(field_type_ref.kind);
          }
        } else {
          m_builder.member_decorate(struct_type, i, spv::DecorationOffset, current_offset);
        }

        m_named_type_refs[resource.logical_name + "." + canonical_field_names[i]] =
            m_named_type_refs[struct_name + "." + canonical_field_names[i]];
      }

      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassStorageBuffer, struct_type);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassStorageBuffer);

      m_builder.set_name(variable_id, resource.logical_name);
      m_builder.decorate(variable_id, spv::DecorationDescriptorSet, resource.descriptor_set);
      m_builder.decorate(variable_id, spv::DecorationBinding, resource.binding);

      if (m_stage_kind == StageKind::Vertex)
        m_builder.decorate(variable_id, spv::DecorationNonWritable);

      m_ubo_map[resource.logical_name] = {variable_id, struct_type, pointer_type, std::move(field_logical_names), std::move(field_type_ids), spv::StorageClassStorageBuffer};
      m_named_type_refs[resource.logical_name] = resource.type;
      m_interface_variable_ids.push_back(variable_id);
      continue;
    }
  }
}

bool VulkanSPIRVEmitter::uses_builtin_output(const CanonicalStmt &stmt, const std::string &field) {
  return std::visit(
      Overloaded{
          [&](const CanonicalOutputAssignStmt &statement) {
            return statement.field == field;
          },
          [&](const CanonicalBlockStmt &statement) {
            for (const auto &child : statement.stmts) {
              if (uses_builtin_output(*child, field))
                return true;
            }
            return false;
          },
          [&](const CanonicalIfStmt &statement) {
            return (statement.then_br &&
                    uses_builtin_output(*statement.then_br, field)) ||
                   (statement.else_br &&
                    uses_builtin_output(*statement.else_br, field));
          },
          [&](const CanonicalForStmt &statement) {
            return (statement.init &&
                    uses_builtin_output(*statement.init, field)) ||
                   (statement.body &&
                    uses_builtin_output(*statement.body, field));
          },
          [&](const CanonicalWhileStmt &statement) {
            return statement.body &&
                   uses_builtin_output(*statement.body, field);
          },
          [&](const auto &) { return false; }
      },
      stmt.data
  );
}

void VulkanSPIRVEmitter::emit_builtin_variables(
    StageKind stage, const CanonicalEntryPoint &entry
) {
  if (stage == StageKind::Vertex) {
    uint32_t int_pointer_type =
        m_builder.register_pointer(spv::StorageClassInput, m_int_type);
    uint32_t vertex_index_id =
        m_builder.variable(int_pointer_type, spv::StorageClassInput);
    m_builder.set_name(vertex_index_id, "gl_VertexID");
    m_builder.decorate(vertex_index_id, spv::DecorationBuiltIn, static_cast<uint32_t>(spv::BuiltInVertexIndex));
    m_gl_vertex_id = {vertex_index_id, m_int_type, spv::StorageClassInput};
    m_has_gl_vertex_id = true;
    m_interface_variable_ids.push_back(vertex_index_id);

    uint32_t instance_index_id =
        m_builder.variable(int_pointer_type, spv::StorageClassInput);
    m_builder.set_name(instance_index_id, "gl_InstanceID");
    m_builder.decorate(instance_index_id, spv::DecorationBuiltIn, static_cast<uint32_t>(spv::BuiltInInstanceIndex));
    m_gl_instance_id = {instance_index_id, m_int_type, spv::StorageClassInput};
    m_has_gl_instance_id = true;
    m_interface_variable_ids.push_back(instance_index_id);

    uint32_t vec4_type = m_builder.register_vector(m_float_type, 4);
    uint32_t pointer_type =
        m_builder.register_pointer(spv::StorageClassOutput, vec4_type);
    uint32_t variable_id =
        m_builder.variable(pointer_type, spv::StorageClassOutput);
    m_builder.set_name(variable_id, "gl_Position");
    m_builder.decorate(variable_id, spv::DecorationBuiltIn, static_cast<uint32_t>(spv::BuiltInPosition));
    m_gl_position = {variable_id, vec4_type, spv::StorageClassOutput};
    m_has_gl_position = true;
    m_interface_variable_ids.push_back(variable_id);
  }

  if (stage == StageKind::Fragment) {
    uint32_t vec4_type = m_builder.register_vector(m_float_type, 4);
    uint32_t input_pointer_type =
        m_builder.register_pointer(spv::StorageClassInput, vec4_type);
    uint32_t frag_coord_id =
        m_builder.variable(input_pointer_type, spv::StorageClassInput);
    m_builder.set_name(frag_coord_id, "gl_FragCoord");
    m_builder.decorate(frag_coord_id, spv::DecorationBuiltIn, static_cast<uint32_t>(spv::BuiltInFragCoord));
    m_gl_frag_coord = {frag_coord_id, vec4_type, spv::StorageClassInput};
    m_has_gl_frag_coord = true;
    m_interface_variable_ids.push_back(frag_coord_id);

    if (entry.body && uses_builtin_output(*entry.body, "gl_FragDepth")) {
      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassOutput, m_float_type);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassOutput);
      m_builder.set_name(variable_id, "gl_FragDepth");
      m_builder.decorate(variable_id, spv::DecorationBuiltIn, static_cast<uint32_t>(spv::BuiltInFragDepth));
      m_gl_frag_depth = {variable_id, m_float_type, spv::StorageClassOutput};
      m_has_gl_frag_depth = true;
      m_interface_variable_ids.push_back(variable_id);
    }
  }
}

void VulkanSPIRVEmitter::emit_global_constants(
    const std::vector<CanonicalDecl> &declarations
) {
  for (const auto &decl : declarations) {
    std::visit(
        Overloaded{
            [&](const CanonicalGlobalConstDecl &constant) {
              if (!constant.init)
                return;
              auto result = try_emit_constant_expr(*constant.init);
              if (result) {
                m_global_constants[constant.name] = *result;
              } else {
                m_deferred_constants.push_back({constant.name, constant.init.get()});
              }
            },
            [&](const auto &) {}
        },
        decl
    );
  }
}

std::optional<uint32_t> VulkanSPIRVEmitter::try_emit_constant_expr(const CanonicalExpr &expr) {
  return std::visit(
      Overloaded{
          [&](const CanonicalLiteralExpr &literal) -> std::optional<uint32_t> {
            return std::visit(
                Overloaded{
                    [&](bool value) -> std::optional<uint32_t> {
                      return m_builder.constant_bool(value);
                    },
                    [&](int64_t value) -> std::optional<uint32_t> {
                      return m_builder.constant_int(
                          static_cast<int32_t>(value), m_int_type
                      );
                    },
                    [&](double value) -> std::optional<uint32_t> {
                      return m_builder.constant_float(
                          static_cast<float>(value), m_float_type
                      );
                    },
                },
                literal.value
            );
          },
          [&](const CanonicalConstructExpr &construct) -> std::optional<uint32_t> {
            std::vector<uint32_t> constituents;
            for (const auto &arg : construct.args) {
              auto component = try_emit_constant_expr(*arg);
              if (!component)
                return std::nullopt;
              constituents.push_back(*component);
            }
            uint32_t result_type = resolve_type(construct.type);
            return m_builder.constant_composite(result_type, constituents);
          },
          [&](const CanonicalIdentifierExpr &identifier) -> std::optional<uint32_t> {
            auto it = m_global_constants.find(identifier.name);
            if (it != m_global_constants.end())
              return it->second;
            return std::nullopt;
          },
          [&](const auto &) -> std::optional<uint32_t> {
            return std::nullopt;
          },
      },
      expr.data
  );
}

void VulkanSPIRVEmitter::collect_local_variables(
    const CanonicalStmt &stmt,
    std::vector<std::pair<std::string, TypeRef>> &out
) {
  std::visit(
      Overloaded{
          [&](const CanonicalVarDeclStmt &var_decl) {
            out.push_back({var_decl.name, var_decl.type});
          },
          [&](const CanonicalBlockStmt &block) {
            for (const auto &child : block.stmts)
              collect_local_variables(*child, out);
          },
          [&](const CanonicalIfStmt &if_stmt) {
            if (if_stmt.then_br)
              collect_local_variables(*if_stmt.then_br, out);
            if (if_stmt.else_br)
              collect_local_variables(*if_stmt.else_br, out);
          },
          [&](const CanonicalForStmt &for_stmt) {
            if (for_stmt.init)
              collect_local_variables(*for_stmt.init, out);
            if (for_stmt.body)
              collect_local_variables(*for_stmt.body, out);
          },
          [&](const CanonicalWhileStmt &while_stmt) {
            if (while_stmt.body)
              collect_local_variables(*while_stmt.body, out);
          },
          [&](const auto &) {}
      },
      stmt.data
  );
}

void VulkanSPIRVEmitter::emit_helper_functions(
    const std::vector<CanonicalDecl> &declarations
) {
  for (const auto &decl : declarations) {
    std::visit(
        Overloaded{
            [&](const CanonicalFunctionDecl &function) {
              uint32_t return_type = resolve_type(function.ret);
              std::vector<uint32_t> param_types;
              for (const auto &param : function.params) {
                param_types.push_back(resolve_type(param.type));
              }

              uint32_t function_type =
                  m_builder.register_function_type(return_type, param_types);
              uint32_t function_id = m_builder.allocate_id();
              m_function_ids[function.name] = function_id;
              m_named_type_refs["__return_" + function.name] = function.ret;

              m_builder.begin_function(function_id, return_type, spv::FunctionControlMaskNone, function_type);

              auto saved_locals = std::move(m_local_variables);
              m_local_variables.clear();

              std::vector<uint32_t> param_value_ids;
              for (const auto &param : function.params) {
                uint32_t param_type = resolve_type(param.type);
                uint32_t param_id = m_builder.function_parameter(param_type);
                param_value_ids.push_back(param_id);
              }

              m_builder.label();

              std::vector<uint32_t> param_local_vars;
              for (uint32_t i = 0; i < function.params.size(); ++i) {
                const auto &param = function.params[i];
                uint32_t param_type = resolve_type(param.type);
                uint32_t pointer_type = m_builder.register_pointer(
                    spv::StorageClassFunction, param_type
                );
                uint32_t local_var =
                    m_builder.variable(pointer_type, spv::StorageClassFunction);
                param_local_vars.push_back(local_var);
                m_local_variables[param.name] = {local_var, param_type, spv::StorageClassFunction};
                m_named_type_refs[param.name] = param.type;
              }

              if (function.body) {
                std::vector<std::pair<std::string, TypeRef>> local_vars;
                collect_local_variables(*function.body, local_vars);
                for (const auto &[name, type] : local_vars) {
                  uint32_t type_id = resolve_type(type);
                  uint32_t pointer_type = m_builder.register_pointer(
                      spv::StorageClassFunction, type_id
                  );
                  uint32_t var_id = m_builder.variable(
                      pointer_type, spv::StorageClassFunction
                  );
                  m_local_variables[name] = {var_id, type_id, spv::StorageClassFunction};
                  m_named_type_refs[name] = type;
                }
              }

              for (uint32_t i = 0; i < function.params.size(); ++i) {
                m_builder.op_store(param_local_vars[i], param_value_ids[i]);
              }

              if (function.body) {
                emit_stmt(*function.body);
              }

              if (return_type == m_void_type) {
                m_builder.op_return();
              }
              m_builder.end_function();

              m_local_variables = std::move(saved_locals);
            },
            [&](const auto &) {}
        },
        decl
    );
  }
}

void VulkanSPIRVEmitter::emit_entry_point(const CanonicalEntryPoint &entry) {
  uint32_t void_func_type =
      m_builder.register_function_type(m_void_type, {});
  m_entry_function_id = m_builder.allocate_id();

  m_builder.begin_function(m_entry_function_id, m_void_type, spv::FunctionControlMaskNone, void_func_type);
  m_builder.label();

  for (const auto &[name, init_expr] : m_deferred_constants) {
    uint32_t value_id = emit_expr(*init_expr);
    m_global_constants[name] = value_id;
  }
  m_deferred_constants.clear();

  if (entry.body) {
    std::vector<std::pair<std::string, TypeRef>> local_vars;
    collect_local_variables(*entry.body, local_vars);
    for (const auto &[name, type] : local_vars) {
      uint32_t type_id = resolve_type(type);
      uint32_t pointer_type =
          m_builder.register_pointer(spv::StorageClassFunction, type_id);
      uint32_t variable_id =
          m_builder.variable(pointer_type, spv::StorageClassFunction);
      m_local_variables[name] = {variable_id, type_id, spv::StorageClassFunction};
      m_named_type_refs[name] = type;
    }

    emit_stmt(*entry.body);

    bool body_ends_with_return = ends_with_terminator(*entry.body);
    if (!body_ends_with_return)
      m_builder.op_return();
  } else {
    m_builder.op_return();
  }

  m_builder.end_function();
}

void VulkanSPIRVEmitter::emit_stmt(const CanonicalStmt &stmt) {
  std::visit(
      Overloaded{
          [&](const CanonicalBlockStmt &block) {
            for (const auto &child : block.stmts)
              emit_stmt(*child);
          },

          [&](const CanonicalExprStmt &expression) {
            if (expression.expr)
              emit_expr(*expression.expr);
          },

          [&](const CanonicalVarDeclStmt &var_decl) {
            auto it = m_local_variables.find(var_decl.name);
            if (it != m_local_variables.end() && var_decl.init) {
              uint32_t init_value = emit_expr(*var_decl.init);
              m_builder.op_store(it->second.pointer_id, init_value);
            }
          },

          [&](const CanonicalOutputAssignStmt &output) {
            uint32_t value_id = emit_expr(*output.value);

            uint32_t *target_ptr = nullptr;
            uint32_t target_type = 0;

            if (output.field == "gl_Position" && m_has_gl_position) {
              target_ptr = &m_gl_position.pointer_id;
              target_type = m_gl_position.type_id;
            } else if (output.field == "gl_FragDepth" && m_has_gl_frag_depth) {
              target_ptr = &m_gl_frag_depth.pointer_id;
              target_type = m_gl_frag_depth.type_id;
            } else {
              auto it = m_output_variables.find(output.field);
              if (it != m_output_variables.end()) {
                target_ptr = &it->second.pointer_id;
                target_type = it->second.type_id;
              }
            }

            if (target_ptr) {
              if (output.op != TokenKind::Eq) {
                uint32_t current = m_builder.op_load(target_type, *target_ptr);
                TypeRef result_type{};
                if (output.field == "gl_Position")
                  result_type = TypeRef{TokenKind::TypeVec4, "vec4"};
                else if (output.field == "gl_FragDepth")
                  result_type = TypeRef{TokenKind::TypeFloat, "float"};
                value_id = emit_binary_op(output.op, result_type, result_type, result_type, current, value_id);
              }
              m_builder.op_store(*target_ptr, value_id);
            }
          },

          [&](const CanonicalIfStmt &if_stmt) {
            uint32_t condition = emit_expr(*if_stmt.cond);
            uint32_t then_label = m_builder.allocate_id();
            uint32_t merge_label = m_builder.allocate_id();
            uint32_t else_label =
                if_stmt.else_br ? m_builder.allocate_id() : merge_label;

            m_builder.op_selection_merge(merge_label, spv::SelectionControlMaskNone);
            m_builder.op_branch_conditional(condition, then_label, else_label);

            m_builder.label(then_label);
            if (if_stmt.then_br) {
              emit_stmt(*if_stmt.then_br);
              if (!ends_with_terminator(*if_stmt.then_br))
                m_builder.op_branch(merge_label);
            } else {
              m_builder.op_branch(merge_label);
            }

            if (if_stmt.else_br) {
              m_builder.label(else_label);
              emit_stmt(*if_stmt.else_br);
              if (!ends_with_terminator(*if_stmt.else_br))
                m_builder.op_branch(merge_label);
            }

            m_builder.label(merge_label);
          },

          [&](const CanonicalForStmt &for_stmt) {
            if (for_stmt.init)
              emit_stmt(*for_stmt.init);

            uint32_t header_label = m_builder.allocate_id();
            uint32_t condition_label = m_builder.allocate_id();
            uint32_t body_label = m_builder.allocate_id();
            uint32_t continue_label = m_builder.allocate_id();
            uint32_t merge_label = m_builder.allocate_id();

            m_loop_merge_stack.push_back(merge_label);
            m_loop_continue_stack.push_back(continue_label);

            m_builder.op_branch(header_label);
            m_builder.label(header_label);
            m_builder.op_loop_merge(merge_label, continue_label, spv::LoopControlMaskNone);
            m_builder.op_branch(condition_label);

            m_builder.label(condition_label);
            if (for_stmt.cond) {
              uint32_t condition = emit_expr(*for_stmt.cond);
              m_builder.op_branch_conditional(condition, body_label, merge_label);
            } else {
              m_builder.op_branch(body_label);
            }

            m_builder.label(body_label);
            if (for_stmt.body)
              emit_stmt(*for_stmt.body);
            m_builder.op_branch(continue_label);

            m_builder.label(continue_label);
            if (for_stmt.step)
              emit_expr(*for_stmt.step);
            m_builder.op_branch(header_label);

            m_builder.label(merge_label);

            m_loop_merge_stack.pop_back();
            m_loop_continue_stack.pop_back();
          },

          [&](const CanonicalWhileStmt &while_stmt) {
            uint32_t header_label = m_builder.allocate_id();
            uint32_t condition_label = m_builder.allocate_id();
            uint32_t body_label = m_builder.allocate_id();
            uint32_t continue_label = m_builder.allocate_id();
            uint32_t merge_label = m_builder.allocate_id();

            m_loop_merge_stack.push_back(merge_label);
            m_loop_continue_stack.push_back(continue_label);

            m_builder.op_branch(header_label);
            m_builder.label(header_label);
            m_builder.op_loop_merge(merge_label, continue_label, spv::LoopControlMaskNone);
            m_builder.op_branch(condition_label);

            m_builder.label(condition_label);
            uint32_t condition = emit_expr(*while_stmt.cond);
            m_builder.op_branch_conditional(condition, body_label, merge_label);

            m_builder.label(body_label);
            if (while_stmt.body)
              emit_stmt(*while_stmt.body);
            m_builder.op_branch(continue_label);

            m_builder.label(continue_label);
            m_builder.op_branch(header_label);

            m_builder.label(merge_label);

            m_loop_merge_stack.pop_back();
            m_loop_continue_stack.pop_back();
          },

          [&](const CanonicalReturnStmt &return_stmt) {
            if (return_stmt.value) {
              uint32_t value = emit_expr(*return_stmt.value);
              m_builder.op_return_value(value);
            } else {
              m_builder.op_return();
            }
          },

          [&](const CanonicalBreakStmt &) {
            if (!m_loop_merge_stack.empty()) {
              m_builder.op_branch(m_loop_merge_stack.back());
              m_builder.label();
              m_builder.op_unreachable();
            }
          },

          [&](const CanonicalContinueStmt &) {
            if (!m_loop_continue_stack.empty()) {
              m_builder.op_branch(m_loop_continue_stack.back());
              m_builder.label();
              m_builder.op_unreachable();
            }
          },

          [&](const CanonicalDiscardStmt &) { m_builder.op_kill(); }
      },
      stmt.data
  );
}

bool VulkanSPIRVEmitter::resolve_resource_field_path(
    const CanonicalExpr &expr, std::string &ubo_name,
    std::string &field_path
) const {
  if (const auto *resource_ref =
          std::get_if<CanonicalStageResourceFieldRef>(&expr.data)) {
    ubo_name = resource_ref->param_name;
    field_path = resource_ref->param_name + "." + resource_ref->field;
    return true;
  }
  if (const auto *identifier =
          std::get_if<CanonicalIdentifierExpr>(&expr.data)) {
    if (m_ubo_map.count(identifier->name) > 0) {
      ubo_name = identifier->name;
      field_path = identifier->name;
      return true;
    }
  }
  if (const auto *index_expr = std::get_if<CanonicalIndexExpr>(&expr.data)) {
    return resolve_resource_field_path(*index_expr->array, ubo_name, field_path);
  }
  if (const auto *field_expr = std::get_if<CanonicalFieldExpr>(&expr.data)) {
    if (resolve_resource_field_path(*field_expr->object, ubo_name, field_path)) {
      field_path += "." + field_expr->field;
      return true;
    }
  }
  return false;
}

spv::StorageClass
VulkanSPIRVEmitter::infer_lvalue_storage_class(
    const CanonicalExpr &expr
) const {
  return std::visit(
      Overloaded{
          [&](const CanonicalIdentifierExpr &identifier) -> spv::StorageClass {
            auto it = m_local_variables.find(identifier.name);
            if (it != m_local_variables.end())
              return it->second.storage;
            auto output_it = m_output_variables.find(identifier.name);
            if (output_it != m_output_variables.end())
              return output_it->second.storage;
            if (identifier.name == "gl_Position" ||
                identifier.name == "gl_FragDepth")
              return spv::StorageClassOutput;
            {
              auto ubo_it = m_ubo_map.find(identifier.name);
              if (ubo_it != m_ubo_map.end())
                return ubo_it->second.storage;
            }
            return spv::StorageClassFunction;
          },
          [&](const CanonicalOutputFieldRef &) -> spv::StorageClass {
            return spv::StorageClassOutput;
          },
          [&](const CanonicalStageResourceFieldRef &resource_ref)
              -> spv::StorageClass {
            std::string sampler_key =
                resource_ref.param_name + "." + resource_ref.field;
            if (m_sampler_variables.count(sampler_key))
              return spv::StorageClassUniformConstant;
            auto ubo_it = m_ubo_map.find(resource_ref.param_name);
            if (ubo_it != m_ubo_map.end())
              return ubo_it->second.storage;
            return spv::StorageClassFunction;
          },
          [&](const CanonicalFieldExpr &field_expr) -> spv::StorageClass {
            return infer_lvalue_storage_class(*field_expr.object);
          },
          [&](const CanonicalIndexExpr &index_expr) -> spv::StorageClass {
            return infer_lvalue_storage_class(*index_expr.array);
          },
          [&](const auto &) -> spv::StorageClass {
            return spv::StorageClassFunction;
          }
      },
      expr.data
  );
}

TypeRef VulkanSPIRVEmitter::infer_expr_type(const CanonicalExpr &expr) const {
  if (expr.type.kind != TokenKind::KeywordVoid)
    return expr.type;

  return std::visit(
      Overloaded{
          [&](const CanonicalLiteralExpr &literal) -> TypeRef {
            return std::visit(
                Overloaded{
                    [](bool) -> TypeRef {
                      return {TokenKind::TypeBool, "bool"};
                    },
                    [](int64_t) -> TypeRef {
                      return {TokenKind::TypeInt, "int"};
                    },
                    [](double) -> TypeRef {
                      return {TokenKind::TypeFloat, "float"};
                    }
                },
                literal.value
            );
          },
          [&](const CanonicalIdentifierExpr &identifier) -> TypeRef {
            auto it = m_named_type_refs.find(identifier.name);
            if (it != m_named_type_refs.end())
              return it->second;
            if (identifier.name == "gl_Position")
              return {TokenKind::TypeVec4, "vec4"};
            if (identifier.name == "gl_FragCoord")
              return {TokenKind::TypeVec4, "vec4"};
            if (identifier.name == "gl_FragDepth" ||
                identifier.name == "gl_PointSize")
              return {TokenKind::TypeFloat, "float"};
            if (identifier.name == "gl_VertexID" ||
                identifier.name == "gl_InstanceID")
              return {TokenKind::TypeInt, "int"};
            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalConstructExpr &construct) -> TypeRef {
            return construct.type;
          },
          [&](const CanonicalStageInputFieldRef &input_ref) -> TypeRef {
            std::string key = input_ref.param_name + "." + input_ref.field;
            auto it = m_named_type_refs.find(key);
            if (it != m_named_type_refs.end())
              return it->second;
            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalStageResourceFieldRef &resource_ref) -> TypeRef {
            std::string key =
                resource_ref.param_name + "." + resource_ref.field;
            auto it = m_named_type_refs.find(key);
            if (it != m_named_type_refs.end())
              return it->second;
            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalOutputFieldRef &output_ref) -> TypeRef {
            auto it = m_named_type_refs.find(output_ref.field);
            if (it != m_named_type_refs.end())
              return it->second;
            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalBinaryExpr &binary) -> TypeRef {
            TypeRef lhs_type = infer_expr_type(*binary.lhs);
            TypeRef rhs_type = infer_expr_type(*binary.rhs);
            if (binary.op == TokenKind::Star ||
                binary.op == TokenKind::StarEq) {
              if (is_matrix_type(lhs_type) && is_vector_type(rhs_type))
                return rhs_type;
              if (is_vector_type(lhs_type) && is_matrix_type(rhs_type))
                return lhs_type;
              if (is_matrix_type(lhs_type) && is_matrix_type(rhs_type))
                return lhs_type;
              if (is_matrix_type(lhs_type) && is_scalar_type(rhs_type))
                return lhs_type;
              if (is_scalar_type(lhs_type) && is_matrix_type(rhs_type))
                return rhs_type;
              if (is_vector_type(lhs_type) && is_scalar_type(rhs_type))
                return lhs_type;
              if (is_scalar_type(lhs_type) && is_vector_type(rhs_type))
                return rhs_type;
            }
            if (binary.op == TokenKind::EqEq || binary.op == TokenKind::BangEq ||
                binary.op == TokenKind::Lt || binary.op == TokenKind::Gt ||
                binary.op == TokenKind::LtEq || binary.op == TokenKind::GtEq ||
                binary.op == TokenKind::AmpAmp ||
                binary.op == TokenKind::PipePipe)
              return {TokenKind::TypeBool, "bool"};
            if (is_vector_type(lhs_type))
              return lhs_type;
            if (is_vector_type(rhs_type))
              return rhs_type;
            if (is_float_type(lhs_type))
              return lhs_type;
            if (is_float_type(rhs_type))
              return rhs_type;
            return lhs_type;
          },
          [&](const CanonicalUnaryExpr &unary) -> TypeRef {
            return infer_expr_type(*unary.operand);
          },
          [&](const CanonicalTernaryExpr &ternary) -> TypeRef {
            return infer_expr_type(*ternary.then_expr);
          },
          [&](const CanonicalCallExpr &call) -> TypeRef {
            if (auto *callee_ident =
                    std::get_if<CanonicalIdentifierExpr>(&call.callee->data)) {
              auto it = m_named_type_refs.find(
                  "__return_" + callee_ident->name
              );
              if (it != m_named_type_refs.end())
                return it->second;

              const std::string &builtin_name = callee_ident->name;

              if (builtin_name == "texture" ||
                  builtin_name == "textureLod" ||
                  builtin_name == "textureProj") {
                if (!call.args.empty()) {
                  TypeRef sampler_type = infer_expr_type(*call.args[0]);
                  if (sampler_type.kind == TokenKind::TypeIsampler2D)
                    return {TokenKind::TypeIvec4, "ivec4"};
                  if (sampler_type.kind == TokenKind::TypeUsampler2D)
                    return {TokenKind::TypeUvec4, "uvec4"};
                }
                return {TokenKind::TypeVec4, "vec4"};
              }

              if (builtin_name == "dot" || builtin_name == "length" ||
                  builtin_name == "distance")
                return {TokenKind::TypeFloat, "float"};

              if (builtin_name == "normalize" || builtin_name == "cross" ||
                  builtin_name == "reflect" || builtin_name == "refract" ||
                  builtin_name == "pow" || builtin_name == "sqrt" ||
                  builtin_name == "inversesqrt" || builtin_name == "abs" ||
                  builtin_name == "sign" || builtin_name == "floor" ||
                  builtin_name == "ceil" || builtin_name == "fract" ||
                  builtin_name == "mod" || builtin_name == "min" ||
                  builtin_name == "max" || builtin_name == "clamp" ||
                  builtin_name == "mix" || builtin_name == "step" ||
                  builtin_name == "smoothstep" || builtin_name == "sin" ||
                  builtin_name == "cos" || builtin_name == "tan" ||
                  builtin_name == "asin" || builtin_name == "acos" ||
                  builtin_name == "atan" || builtin_name == "exp" ||
                  builtin_name == "log" || builtin_name == "exp2" ||
                  builtin_name == "log2" || builtin_name == "round" ||
                  builtin_name == "trunc" || builtin_name == "fwidth" ||
                  builtin_name == "dFdx" || builtin_name == "dFdy") {
                if (!call.args.empty())
                  return infer_expr_type(*call.args[0]);
              }

              if (builtin_name == "transpose" || builtin_name == "inverse") {
                if (!call.args.empty())
                  return infer_expr_type(*call.args[0]);
              }
            }
            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalFieldExpr &field_expr) -> TypeRef {
            TypeRef object_type = infer_expr_type(*field_expr.object);
            if (is_vector_type(object_type) || is_scalar_type(object_type)) {
              if (field_expr.field.size() == 1) {
                if (is_int_type(object_type))
                  return {TokenKind::TypeInt, "int"};
                if (is_uint_type(object_type))
                  return {TokenKind::TypeUint, "uint"};
                return {TokenKind::TypeFloat, "float"};
              }
              uint32_t count =
                  static_cast<uint32_t>(field_expr.field.size());
              if (is_int_type(object_type)) {
                if (count == 2)
                  return {TokenKind::TypeIvec2, "ivec2"};
                if (count == 3)
                  return {TokenKind::TypeIvec3, "ivec3"};
                return {TokenKind::TypeIvec4, "ivec4"};
              }
              if (count == 2)
                return {TokenKind::TypeVec2, "vec2"};
              if (count == 3)
                return {TokenKind::TypeVec3, "vec3"};
              return {TokenKind::TypeVec4, "vec4"};
            }
            if (object_type.kind == TokenKind::Identifier) {
              auto struct_it =
                  m_struct_field_names.find(object_type.name);
              if (struct_it != m_struct_field_names.end()) {
                for (uint32_t i = 0;
                     i < struct_it->second.size();
                     ++i) {
                  if (struct_it->second[i] == field_expr.field) {
                    auto type_it = m_struct_field_type_ids.find(
                        object_type.name
                    );
                    if (type_it != m_struct_field_type_ids.end() &&
                        i < type_it->second.size()) {
                      auto named_it = m_named_type_refs.find(
                          object_type.name + "." + field_expr.field
                      );
                      if (named_it != m_named_type_refs.end())
                        return named_it->second;
                    }
                  }
                }
              }
            }

            {
              std::string ubo_name, field_path;
              if (resolve_resource_field_path(expr, ubo_name, field_path)) {
                auto it = m_named_type_refs.find(field_path);
                if (it != m_named_type_refs.end())
                  return it->second;
              }
            }

            return {TokenKind::KeywordVoid, ""};
          },
          [&](const CanonicalIndexExpr &index_expr) -> TypeRef {
            TypeRef array_type = infer_expr_type(*index_expr.array);
            if (is_vector_type(array_type)) {
              if (is_int_type(array_type))
                return {TokenKind::TypeInt, "int"};
              if (is_uint_type(array_type))
                return {TokenKind::TypeUint, "uint"};
              return {TokenKind::TypeFloat, "float"};
            }
            TypeRef element_type = array_type;
            element_type.array_size = std::nullopt;
            element_type.is_runtime_sized = false;
            return element_type;
          },
          [&](const CanonicalAssignExpr &assign) -> TypeRef {
            return infer_expr_type(*assign.lhs);
          }
      },
      expr.data
  );
}

uint32_t VulkanSPIRVEmitter::emit_expr(const CanonicalExpr &expr) {
  return std::visit(
      Overloaded{
          [&](const CanonicalLiteralExpr &literal) -> uint32_t {
            return std::visit(
                Overloaded{
                    [&](bool value) -> uint32_t {
                      return m_builder.constant_bool(value);
                    },
                    [&](int64_t value) -> uint32_t {
                      return m_builder.constant_int(
                          static_cast<int32_t>(value), m_int_type
                      );
                    },
                    [&](double value) -> uint32_t {
                      return m_builder.constant_float(
                          static_cast<float>(value), m_float_type
                      );
                    }
                },
                literal.value
            );
          },

          [&](const CanonicalIdentifierExpr &identifier) -> uint32_t {
            auto constant_it = m_global_constants.find(identifier.name);
            if (constant_it != m_global_constants.end())
              return constant_it->second;

            auto local_it = m_local_variables.find(identifier.name);
            if (local_it != m_local_variables.end())
              return m_builder.op_load(local_it->second.type_id, local_it->second.pointer_id);

            if (identifier.name == "gl_VertexID" && m_has_gl_vertex_id)
              return m_builder.op_load(m_gl_vertex_id.type_id, m_gl_vertex_id.pointer_id);
            if (identifier.name == "gl_InstanceID" && m_has_gl_instance_id)
              return m_builder.op_load(m_gl_instance_id.type_id, m_gl_instance_id.pointer_id);
            if (identifier.name == "gl_FragCoord" && m_has_gl_frag_coord)
              return m_builder.op_load(m_gl_frag_coord.type_id, m_gl_frag_coord.pointer_id);
            if (identifier.name == "gl_Position" && m_has_gl_position)
              return m_builder.op_load(m_gl_position.type_id, m_gl_position.pointer_id);
            if (identifier.name == "gl_FragDepth" && m_has_gl_frag_depth)
              return m_builder.op_load(m_gl_frag_depth.type_id, m_gl_frag_depth.pointer_id);

            auto sampler_it = m_sampler_variables.find(identifier.name);
            if (sampler_it != m_sampler_variables.end())
              return m_builder.op_load(sampler_it->second.type_id, sampler_it->second.pointer_id);

            for (const auto &[ubo_name, ubo_info] : m_ubo_map) {
              for (uint32_t i = 0; i < ubo_info.field_logical_names.size();
                   ++i) {
                if (ubo_info.field_logical_names[i] == identifier.name) {
                  uint32_t member_type = ubo_info.field_type_ids[i];
                  uint32_t member_pointer_type = m_builder.register_pointer(
                      ubo_info.storage, member_type
                  );
                  uint32_t index = m_builder.constant_int(
                      static_cast<int32_t>(i), m_int_type
                  );
                  uint32_t access = m_builder.op_access_chain(
                      member_pointer_type, ubo_info.variable_id, {index}
                  );
                  uint32_t loaded = m_builder.op_load(member_type, access);
                  auto type_it = m_named_type_refs.find(identifier.name);
                  if (type_it != m_named_type_refs.end() &&
                      type_it->second.kind == TokenKind::TypeBool &&
                      member_type == m_uint_type) {
                    uint32_t zero = m_builder.constant_uint(0, m_uint_type);
                    loaded = m_builder.op_i_not_equal(m_bool_type, loaded, zero);
                  }
                  return loaded;
                }
              }
            }

            add_error("[SPIRV] Unresolved identifier expression: " + identifier.name);
            return 0;
          },

          [&](const CanonicalStageInputFieldRef &input_ref) -> uint32_t {
            std::string key =
                input_ref.param_name + "." + input_ref.field;
            auto it = m_input_variables.find(key);
            if (it != m_input_variables.end())
              return m_builder.op_load(it->second.type_id, it->second.pointer_id);
            add_error("[SPIRV] Unresolved stage input field: " + key);
            return 0;
          },

          [&](const CanonicalStageResourceFieldRef &resource_ref) -> uint32_t {
            std::string sampler_key =
                resource_ref.param_name + "." + resource_ref.field;
            auto sampler_it = m_sampler_variables.find(sampler_key);
            if (sampler_it != m_sampler_variables.end())
              return m_builder.op_load(sampler_it->second.type_id, sampler_it->second.pointer_id);

            auto ubo_it = m_ubo_map.find(resource_ref.param_name);
            if (ubo_it != m_ubo_map.end()) {
              const auto &ubo = ubo_it->second;
              std::string field_key = sampler_key;

              for (uint32_t i = 0; i < ubo.field_logical_names.size(); ++i) {
                if (ubo.field_logical_names[i] == field_key) {
                  uint32_t member_type = ubo.field_type_ids[i];
                  uint32_t member_pointer_type = m_builder.register_pointer(
                      ubo.storage, member_type
                  );
                  uint32_t index =
                      m_builder.constant_int(static_cast<int32_t>(i), m_int_type);
                  uint32_t access = m_builder.op_access_chain(
                      member_pointer_type, ubo.variable_id, {index}
                  );
                  uint32_t loaded = m_builder.op_load(member_type, access);
                  auto type_it = m_named_type_refs.find(field_key);
                  if (type_it != m_named_type_refs.end() &&
                      type_it->second.kind == TokenKind::TypeBool &&
                      member_type == m_uint_type) {
                    uint32_t zero = m_builder.constant_uint(0, m_uint_type);
                    loaded = m_builder.op_i_not_equal(m_bool_type, loaded, zero);
                  }
                  return loaded;
                }
              }
            }
            add_error("[SPIRV] Unresolved stage resource field: " + sampler_key);
            return 0;
          },

          [&](const CanonicalOutputFieldRef &output_ref) -> uint32_t {
            auto it = m_output_variables.find(output_ref.field);
            if (it != m_output_variables.end())
              return m_builder.op_load(it->second.type_id, it->second.pointer_id);
            add_error("[SPIRV] Unresolved output field expression: " + output_ref.field);
            return 0;
          },

          [&](const CanonicalBinaryExpr &binary) -> uint32_t {
            uint32_t lhs = emit_expr(*binary.lhs);
            uint32_t rhs = emit_expr(*binary.rhs);
            TypeRef lhs_type = infer_expr_type(*binary.lhs);
            TypeRef rhs_type = infer_expr_type(*binary.rhs);
            TypeRef result = infer_expr_type(expr);
            return emit_binary_op(binary.op, result, lhs_type, rhs_type, lhs, rhs);
          },

          [&](const CanonicalUnaryExpr &unary) -> uint32_t {
            if (unary.op == TokenKind::PlusPlus ||
                unary.op == TokenKind::MinusMinus) {
              uint32_t pointer = emit_lvalue(*unary.operand);
              TypeRef operand_type = infer_expr_type(*unary.operand);
              uint32_t type_id = resolve_type(operand_type);
              uint32_t current = m_builder.op_load(type_id, pointer);

              uint32_t one;
              if (is_float_type(operand_type))
                one = m_builder.constant_float(1.0f, m_float_type);
              else
                one = m_builder.constant_int(1, m_int_type);

              uint32_t result;
              if (unary.op == TokenKind::PlusPlus) {
                result = is_float_type(operand_type)
                             ? m_builder.op_f_add(type_id, current, one)
                             : m_builder.op_i_add(type_id, current, one);
              } else {
                result = is_float_type(operand_type)
                             ? m_builder.op_f_sub(type_id, current, one)
                             : m_builder.op_i_sub(type_id, current, one);
              }
              m_builder.op_store(pointer, result);
              return unary.prefix ? result : current;
            }
            uint32_t operand = emit_expr(*unary.operand);
            return emit_unary_op(unary.op, infer_expr_type(*unary.operand), operand);
          },

          [&](const CanonicalTernaryExpr &ternary) -> uint32_t {
            uint32_t condition = emit_expr(*ternary.cond);
            uint32_t then_value = emit_expr(*ternary.then_expr);
            uint32_t else_value = emit_expr(*ternary.else_expr);
            uint32_t result_type =
                resolve_type(infer_expr_type(*ternary.then_expr));
            return m_builder.op_select(result_type, condition, then_value, else_value);
          },

          [&](const CanonicalCallExpr &call) -> uint32_t {
            if (auto *callee_ident =
                    std::get_if<CanonicalIdentifierExpr>(&call.callee->data)) {
              return emit_builtin_call(callee_ident->name, infer_expr_type(expr), expr);
            }

            uint32_t result_type = resolve_type(infer_expr_type(expr));
            uint32_t callee_id = emit_expr(*call.callee);
            if (callee_id == 0) {
              add_error("[SPIRV] Call target resolved to id 0");
              return 0;
            }
            std::vector<uint32_t> arg_ids;
            for (const auto &arg : call.args)
              arg_ids.push_back(emit_expr(*arg));
            return m_builder.op_function_call(result_type, callee_id, arg_ids);
          },

          [&](const CanonicalIndexExpr &index_expr) -> uint32_t {
            uint32_t array_ptr = emit_lvalue(*index_expr.array);
            uint32_t index_val = emit_expr(*index_expr.index);
            spv::StorageClass storage =
                infer_lvalue_storage_class(*index_expr.array);
            uint32_t element_type = resolve_type(infer_expr_type(expr));
            uint32_t pointer_type =
                m_builder.register_pointer(storage, element_type);
            uint32_t access = m_builder.op_access_chain(pointer_type, array_ptr, {index_val});
            return m_builder.op_load(element_type, access);
          },

          [&](const CanonicalFieldExpr &field_expr) -> uint32_t {
            std::string ubo_name, field_path;
            bool resolved = resolve_resource_field_path(expr, ubo_name, field_path);
            if (resolved) {
              auto sampler_it = m_sampler_variables.find(field_path);
              if (sampler_it != m_sampler_variables.end())
                return m_builder.op_load(sampler_it->second.type_id, sampler_it->second.pointer_id);

              auto ubo_it = m_ubo_map.find(ubo_name);
              if (ubo_it != m_ubo_map.end()) {
                const auto &ubo = ubo_it->second;
                for (uint32_t i = 0; i < ubo.field_logical_names.size(); ++i) {
                  if (ubo.field_logical_names[i] == field_path) {
                    uint32_t member_type = ubo.field_type_ids[i];
                    uint32_t member_pointer_type = m_builder.register_pointer(
                        ubo.storage, member_type
                    );
                    uint32_t index = m_builder.constant_int(
                        static_cast<int32_t>(i), m_int_type
                    );
                    uint32_t access = m_builder.op_access_chain(
                        member_pointer_type, ubo.variable_id, {index}
                    );
                    uint32_t loaded = m_builder.op_load(member_type, access);
                    auto type_it = m_named_type_refs.find(field_path);
                    if (type_it != m_named_type_refs.end() &&
                        type_it->second.kind == TokenKind::TypeBool &&
                        member_type == m_uint_type) {
                      uint32_t zero = m_builder.constant_uint(0, m_uint_type);
                      loaded = m_builder.op_i_not_equal(m_bool_type, loaded, zero);
                    }
                    return loaded;
                  }
                }
              }
            }

            TypeRef object_type = infer_expr_type(*field_expr.object);
            uint32_t object = emit_expr(*field_expr.object);

            if (is_swizzle(field_expr.field, object_type)) {
              TypeRef inferred_result = infer_expr_type(expr);
              uint32_t result_type = resolve_type(inferred_result);

              if (field_expr.field.size() == 1) {
                uint32_t component_index = swizzle_index(field_expr.field[0]);
                return m_builder.op_composite_extract(result_type, object, component_index);
              }

              std::vector<uint32_t> components;
              for (char c : field_expr.field)
                components.push_back(swizzle_index(c));
              return m_builder.op_vector_shuffle(result_type, object, object, components);
            }

            if (object_type.kind == TokenKind::Identifier) {
              auto struct_it =
                  m_struct_field_names.find(object_type.name);
              if (struct_it != m_struct_field_names.end()) {
                const auto &field_names = struct_it->second;
                for (uint32_t i = 0; i < field_names.size(); ++i) {
                  if (field_names[i] == field_expr.field) {
                    TypeRef inferred_result = infer_expr_type(expr);
                    uint32_t result_type = resolve_type(inferred_result);
                    return m_builder.op_composite_extract(result_type, object, i);
                  }
                }
              }
            }

            return object;
          },

          [&](const CanonicalConstructExpr &construct) -> uint32_t {
            return emit_construct(construct.type, construct.args);
          },

          [&](const CanonicalAssignExpr &assign) -> uint32_t {
            if (const auto *field_lhs =
                    std::get_if<CanonicalFieldExpr>(&assign.lhs->data)) {
              TypeRef object_type = infer_expr_type(*field_lhs->object);
              if (is_swizzle(field_lhs->field, object_type) &&
                  field_lhs->field.size() > 1) {
                uint32_t vector_pointer = emit_lvalue(*field_lhs->object);
                uint32_t rhs_value = emit_expr(*assign.rhs);
                uint32_t vector_type = resolve_type(object_type);
                uint32_t current_value =
                    m_builder.op_load(vector_type, vector_pointer);

                uint32_t vec_size = vector_component_count(object_type);
                uint32_t swizzle_size =
                    static_cast<uint32_t>(field_lhs->field.size());

                std::vector<uint32_t> shuffle_indices(vec_size);
                for (uint32_t i = 0; i < vec_size; ++i)
                  shuffle_indices[i] = i;
                for (uint32_t i = 0; i < swizzle_size; ++i) {
                  uint32_t target = swizzle_index(field_lhs->field[i]);
                  shuffle_indices[target] = vec_size + i;
                }

                uint32_t shuffled = m_builder.op_vector_shuffle(
                    vector_type, current_value, rhs_value, shuffle_indices
                );
                m_builder.op_store(vector_pointer, shuffled);
                return shuffled;
              }
            }

            uint32_t pointer = emit_lvalue(*assign.lhs);
            uint32_t rhs_value = emit_expr(*assign.rhs);

            if (assign.op != TokenKind::Eq) {
              TypeRef inferred_lhs_type = infer_expr_type(*assign.lhs);
              TypeRef inferred_rhs_type = infer_expr_type(*assign.rhs);
              uint32_t lhs_type_id = resolve_type(inferred_lhs_type);
              uint32_t current = m_builder.op_load(lhs_type_id, pointer);
              rhs_value = emit_binary_op(assign.op, inferred_lhs_type, inferred_lhs_type, inferred_rhs_type, current, rhs_value);
            }

            m_builder.op_store(pointer, rhs_value);
            return rhs_value;
          }
      },
      expr.data
  );
}

uint32_t VulkanSPIRVEmitter::emit_lvalue(const CanonicalExpr &expr) {
  return std::visit(
      Overloaded{
          [&](const CanonicalIdentifierExpr &identifier) -> uint32_t {
            auto it = m_local_variables.find(identifier.name);
            if (it != m_local_variables.end())
              return it->second.pointer_id;

            if (identifier.name == "gl_Position" && m_has_gl_position)
              return m_gl_position.pointer_id;
            if (identifier.name == "gl_FragDepth" && m_has_gl_frag_depth)
              return m_gl_frag_depth.pointer_id;

            auto output_it = m_output_variables.find(identifier.name);
            if (output_it != m_output_variables.end())
              return output_it->second.pointer_id;

            if (identifier.name == "gl_VertexID" && m_has_gl_vertex_id)
              return m_gl_vertex_id.pointer_id;
            if (identifier.name == "gl_InstanceID" && m_has_gl_instance_id)
              return m_gl_instance_id.pointer_id;
            if (identifier.name == "gl_FragCoord" && m_has_gl_frag_coord)
              return m_gl_frag_coord.pointer_id;

            auto ubo_it = m_ubo_map.find(identifier.name);
            if (ubo_it != m_ubo_map.end())
              return ubo_it->second.variable_id;

            return 0;
          },

          [&](const CanonicalOutputFieldRef &output_ref) -> uint32_t {
            if (output_ref.field == "gl_Position" && m_has_gl_position)
              return m_gl_position.pointer_id;
            if (output_ref.field == "gl_FragDepth" && m_has_gl_frag_depth)
              return m_gl_frag_depth.pointer_id;
            auto it = m_output_variables.find(output_ref.field);
            if (it != m_output_variables.end())
              return it->second.pointer_id;
            return 0;
          },

          [&](const CanonicalStageResourceFieldRef &resource_ref) -> uint32_t {
            std::string sampler_key =
                resource_ref.param_name + "." + resource_ref.field;
            auto sampler_it = m_sampler_variables.find(sampler_key);
            if (sampler_it != m_sampler_variables.end())
              return sampler_it->second.pointer_id;

            auto ubo_it = m_ubo_map.find(resource_ref.param_name);
            if (ubo_it != m_ubo_map.end()) {
              const auto &ubo = ubo_it->second;
              for (uint32_t i = 0; i < ubo.field_logical_names.size(); ++i) {
                if (ubo.field_logical_names[i] == sampler_key) {
                  uint32_t member_type = ubo.field_type_ids[i];
                  uint32_t member_pointer_type = m_builder.register_pointer(
                      ubo.storage, member_type
                  );
                  uint32_t index = m_builder.constant_int(
                      static_cast<int32_t>(i), m_int_type
                  );
                  return m_builder.op_access_chain(
                      member_pointer_type, ubo.variable_id, {index}
                  );
                }
              }
            }
            return 0;
          },

          [&](const CanonicalFieldExpr &field_expr) -> uint32_t {
            uint32_t base_pointer = emit_lvalue(*field_expr.object);
            if (base_pointer == 0)
              return 0;

            spv::StorageClass storage =
                infer_lvalue_storage_class(*field_expr.object);
            TypeRef object_type = infer_expr_type(*field_expr.object);

            if (is_swizzle(field_expr.field, object_type) &&
                field_expr.field.size() == 1) {
              uint32_t component = swizzle_index(field_expr.field[0]);
              TypeRef inferred_result = infer_expr_type(expr);
              uint32_t component_type = resolve_type(inferred_result);
              uint32_t pointer_type =
                  m_builder.register_pointer(storage, component_type);
              uint32_t index_id = m_builder.constant_int(
                  static_cast<int32_t>(component), m_int_type
              );
              return m_builder.op_access_chain(pointer_type, base_pointer, {index_id});
            }

            if (object_type.kind == TokenKind::Identifier) {
              auto struct_it =
                  m_struct_field_names.find(object_type.name);
              if (struct_it != m_struct_field_names.end()) {
                const auto &names = struct_it->second;
                for (uint32_t i = 0; i < names.size(); ++i) {
                  if (names[i] == field_expr.field) {
                    TypeRef inferred_result = infer_expr_type(expr);
                    uint32_t member_type = resolve_type(inferred_result);
                    uint32_t pointer_type =
                        m_builder.register_pointer(storage, member_type);
                    uint32_t index_id = m_builder.constant_int(
                        static_cast<int32_t>(i), m_int_type
                    );
                    return m_builder.op_access_chain(pointer_type, base_pointer, {index_id});
                  }
                }
              }
            }
            return 0;
          },

          [&](const CanonicalIndexExpr &index_expr) -> uint32_t {
            uint32_t base_pointer = emit_lvalue(*index_expr.array);
            uint32_t index_value = emit_expr(*index_expr.index);
            spv::StorageClass storage =
                infer_lvalue_storage_class(*index_expr.array);
            TypeRef inferred_result = infer_expr_type(expr);
            uint32_t element_type = resolve_type(inferred_result);
            uint32_t pointer_type =
                m_builder.register_pointer(storage, element_type);
            return m_builder.op_access_chain(pointer_type, base_pointer, {index_value});
          },

          [&](const auto &) -> uint32_t { return 0; }
      },
      expr.data
  );
}

uint32_t VulkanSPIRVEmitter::emit_binary_op(TokenKind op, const TypeRef &result_type, const TypeRef &lhs_type, const TypeRef &rhs_type, uint32_t lhs, uint32_t rhs) {
  uint32_t type_id = resolve_type(result_type);
  bool lhs_is_float = is_float_type(lhs_type);
  bool lhs_is_matrix = is_matrix_type(lhs_type);
  bool lhs_is_vector = is_vector_type(lhs_type);
  bool lhs_is_scalar = is_scalar_type(lhs_type);
  bool rhs_is_matrix = is_matrix_type(rhs_type);
  bool rhs_is_vector = is_vector_type(rhs_type);
  bool rhs_is_scalar = is_scalar_type(rhs_type);

  bool any_float = lhs_is_float || is_float_type(rhs_type);
  bool any_int = is_int_type(lhs_type) || is_int_type(rhs_type);

  if (lhs_is_vector && rhs_is_scalar) {
    uint32_t component_count = vector_component_count(lhs_type);
    std::vector<uint32_t> components(component_count, rhs);
    rhs = m_builder.op_composite_construct(type_id, components);
    rhs_is_scalar = false;
    rhs_is_vector = true;
  } else if (rhs_is_vector && lhs_is_scalar) {
    uint32_t component_count = vector_component_count(rhs_type);
    std::vector<uint32_t> components(component_count, lhs);
    lhs = m_builder.op_composite_construct(type_id, components);
    lhs_is_scalar = false;
    lhs_is_vector = true;
  }

  switch (op) {
    case TokenKind::Plus:
    case TokenKind::PlusEq:
      return any_float ? m_builder.op_f_add(type_id, lhs, rhs)
                       : m_builder.op_i_add(type_id, lhs, rhs);
    case TokenKind::Minus:
    case TokenKind::MinusEq:
      return any_float ? m_builder.op_f_sub(type_id, lhs, rhs)
                       : m_builder.op_i_sub(type_id, lhs, rhs);
    case TokenKind::Star:
    case TokenKind::StarEq: {
      if (lhs_is_matrix && rhs_is_matrix)
        return m_builder.op_matrix_times_matrix(type_id, lhs, rhs);
      if (lhs_is_matrix && rhs_is_vector)
        return m_builder.op_matrix_times_vector(type_id, lhs, rhs);
      if (lhs_is_vector && rhs_is_matrix)
        return m_builder.op_vector_times_matrix(type_id, lhs, rhs);
      if (lhs_is_matrix && rhs_is_scalar)
        return m_builder.op_matrix_times_scalar(type_id, lhs, rhs);
      if (lhs_is_scalar && rhs_is_matrix)
        return m_builder.op_matrix_times_scalar(type_id, rhs, lhs);
      if (lhs_is_vector && rhs_is_scalar)
        return m_builder.op_vector_times_scalar(type_id, lhs, rhs);
      if (lhs_is_scalar && rhs_is_vector)
        return m_builder.op_vector_times_scalar(type_id, rhs, lhs);
      if (any_float)
        return m_builder.op_f_mul(type_id, lhs, rhs);
      return m_builder.op_i_mul(type_id, lhs, rhs);
    }
    case TokenKind::Slash:
    case TokenKind::SlashEq:
      return any_float ? m_builder.op_f_div(type_id, lhs, rhs)
                       : m_builder.op_s_div(type_id, lhs, rhs);
    case TokenKind::Percent:
    case TokenKind::PercentEq:
      return any_float ? m_builder.op_f_mod(type_id, lhs, rhs)
                       : m_builder.op_s_mod(type_id, lhs, rhs);
    case TokenKind::EqEq:
      return any_float ? m_builder.op_f_ord_equal(type_id, lhs, rhs)
                       : m_builder.op_i_equal(type_id, lhs, rhs);
    case TokenKind::BangEq:
      return any_float ? m_builder.op_f_ord_not_equal(type_id, lhs, rhs)
                       : m_builder.op_i_not_equal(type_id, lhs, rhs);
    case TokenKind::Lt:
      return any_float ? m_builder.op_f_ord_less_than(type_id, lhs, rhs)
             : any_int ? m_builder.op_s_less_than(type_id, lhs, rhs)
                       : m_builder.op_s_less_than(type_id, lhs, rhs);
    case TokenKind::Gt:
      return any_float ? m_builder.op_f_ord_greater_than(type_id, lhs, rhs)
                       : m_builder.op_s_greater_than(type_id, lhs, rhs);
    case TokenKind::LtEq:
      return any_float ? m_builder.op_f_ord_less_than_equal(type_id, lhs, rhs)
                       : m_builder.op_s_less_than_equal(type_id, lhs, rhs);
    case TokenKind::GtEq:
      return any_float
                 ? m_builder.op_f_ord_greater_than_equal(type_id, lhs, rhs)
                 : m_builder.op_s_greater_than_equal(type_id, lhs, rhs);
    case TokenKind::AmpAmp:
      return m_builder.op_logical_and(type_id, lhs, rhs);
    case TokenKind::PipePipe:
      return m_builder.op_logical_or(type_id, lhs, rhs);
    default:
      return 0;
  }
}

uint32_t VulkanSPIRVEmitter::emit_unary_op(TokenKind op, const TypeRef &type, uint32_t operand) {
  uint32_t type_id = resolve_type(type);
  switch (op) {
    case TokenKind::Minus:
      return is_float_type(type) ? m_builder.op_f_negate(type_id, operand)
                                 : m_builder.op_s_negate(type_id, operand);
    case TokenKind::Bang:
      return m_builder.op_logical_not(type_id, operand);
    default:
      return operand;
  }
}

uint32_t
VulkanSPIRVEmitter::emit_construct(const TypeRef &target_type, const std::vector<CanonicalExprPtr> &args) {
  uint32_t result_type = resolve_type(target_type);

  if (is_scalar_type(target_type) && args.size() == 1) {
    uint32_t arg = emit_expr(*args[0]);
    TypeRef arg_type = infer_expr_type(*args[0]);

    if (target_type.kind == TokenKind::TypeFloat && is_int_type(arg_type))
      return m_builder.op_convert_s_to_f(result_type, arg);
    if (target_type.kind == TokenKind::TypeFloat && is_uint_type(arg_type))
      return m_builder.op_convert_u_to_f(result_type, arg);
    if (target_type.kind == TokenKind::TypeInt && is_float_type(arg_type))
      return m_builder.op_convert_f_to_s(result_type, arg);
    if (target_type.kind == TokenKind::TypeUint && is_float_type(arg_type))
      return m_builder.op_convert_f_to_u(result_type, arg);
    return arg;
  }

  if (is_vector_type(target_type)) {
    uint32_t target_count = vector_component_count(target_type);
    bool target_is_float = is_float_type(target_type);
    bool target_is_int = is_int_type(target_type);
    bool target_is_uint = is_uint_type(target_type);

    if (args.size() == 1) {
      uint32_t arg = emit_expr(*args[0]);
      TypeRef arg_type = infer_expr_type(*args[0]);

      if (is_scalar_type(arg_type)) {
        uint32_t coerced = arg;
        if (target_is_float && is_int_type(arg_type))
          coerced = m_builder.op_convert_s_to_f(m_float_type, arg);
        else if (target_is_float && is_uint_type(arg_type))
          coerced = m_builder.op_convert_u_to_f(m_float_type, arg);
        else if (target_is_int && is_float_type(arg_type))
          coerced = m_builder.op_convert_f_to_s(m_int_type, arg);
        else if (target_is_uint && is_float_type(arg_type))
          coerced = m_builder.op_convert_f_to_u(m_uint_type, arg);
        std::vector<uint32_t> components(target_count, coerced);
        return m_builder.op_composite_construct(result_type, components);
      }

      if (is_vector_type(arg_type)) {
        uint32_t arg_count = vector_component_count(arg_type);
        if (arg_count == target_count)
          return arg;
        if (arg_count > target_count) {
          std::vector<uint32_t> indices;
          for (uint32_t i = 0; i < target_count; ++i)
            indices.push_back(i);
          return m_builder.op_vector_shuffle(result_type, arg, arg, indices);
        }
      }
    }

    std::vector<uint32_t> constituents;
    for (const auto &arg_expr : args) {
      uint32_t arg = emit_expr(*arg_expr);
      TypeRef arg_type = infer_expr_type(*arg_expr);
      if (is_scalar_type(arg_type)) {
        if (target_is_float && is_int_type(arg_type))
          arg = m_builder.op_convert_s_to_f(m_float_type, arg);
        else if (target_is_float && is_uint_type(arg_type))
          arg = m_builder.op_convert_u_to_f(m_float_type, arg);
        else if (target_is_int && is_float_type(arg_type))
          arg = m_builder.op_convert_f_to_s(m_int_type, arg);
        else if (target_is_uint && is_float_type(arg_type))
          arg = m_builder.op_convert_f_to_u(m_uint_type, arg);
      }
      constituents.push_back(arg);
    }
    return m_builder.op_composite_construct(result_type, constituents);
  }

  if (is_matrix_type(target_type)) {
    if (args.size() == 1) {
      TypeRef arg_type = infer_expr_type(*args[0]);
      if (is_matrix_type(arg_type)) {
        uint32_t source = emit_expr(*args[0]);
        uint32_t target_dim = (target_type.kind == TokenKind::TypeMat2)   ? 2
                              : (target_type.kind == TokenKind::TypeMat3) ? 3
                                                                          : 4;
        uint32_t source_dim = (arg_type.kind == TokenKind::TypeMat2)   ? 2
                              : (arg_type.kind == TokenKind::TypeMat3) ? 3
                                                                       : 4;
        uint32_t source_col_type = m_builder.register_vector(m_float_type, source_dim);
        uint32_t target_col_type = m_builder.register_vector(m_float_type, target_dim);

        std::vector<uint32_t> columns;
        for (uint32_t col = 0; col < target_dim; ++col) {
          uint32_t column = m_builder.op_composite_extract(source_col_type, source, col);
          if (target_dim < source_dim) {
            std::vector<uint32_t> indices;
            for (uint32_t row = 0; row < target_dim; ++row)
              indices.push_back(row);
            column = m_builder.op_vector_shuffle(target_col_type, column, column, indices);
          }
          columns.push_back(column);
        }
        return m_builder.op_composite_construct(result_type, columns);
      }
    }

    std::vector<uint32_t> columns;
    for (const auto &arg : args)
      columns.push_back(emit_expr(*arg));
    return m_builder.op_composite_construct(result_type, columns);
  }

  std::vector<uint32_t> members;
  for (const auto &arg : args)
    members.push_back(emit_expr(*arg));
  return m_builder.op_composite_construct(result_type, members);
}

uint32_t
VulkanSPIRVEmitter::emit_builtin_call(const std::string &name, const TypeRef &result_type, const CanonicalExpr &call_expr) {
  const auto &call = std::get<CanonicalCallExpr>(call_expr.data);
  uint32_t type_id = resolve_type(result_type);

  auto function_it = m_function_ids.find(name);
  if (function_it != m_function_ids.end()) {
    std::vector<uint32_t> arg_ids;
    for (const auto &arg : call.args)
      arg_ids.push_back(emit_expr(*arg));
    return m_builder.op_function_call(type_id, function_it->second, arg_ids);
  }

  if (name == "texture") {
    uint32_t sampled_image = emit_expr(*call.args[0]);
    uint32_t coordinate = emit_expr(*call.args[1]);
    return m_builder.op_image_sample_implicit_lod(type_id, sampled_image, coordinate);
  }

  if (name == "dot") {
    uint32_t a = emit_expr(*call.args[0]);
    uint32_t b = emit_expr(*call.args[1]);
    return m_builder.op_dot(type_id, a, b);
  }

  if (name == "transpose") {
    uint32_t matrix = emit_expr(*call.args[0]);
    return m_builder.op_transpose(type_id, matrix);
  }

  if (name == "fwidth") {
    uint32_t operand = emit_expr(*call.args[0]);
    return m_builder.op_fwidth(type_id, operand);
  }

  if (name == "dFdx") {
    uint32_t operand = emit_expr(*call.args[0]);
    return m_builder.op_dpdx(type_id, operand);
  }

  if (name == "dFdy") {
    uint32_t operand = emit_expr(*call.args[0]);
    return m_builder.op_dpdy(type_id, operand);
  }

  std::vector<uint32_t> arg_ids;
  for (const auto &arg : call.args)
    arg_ids.push_back(emit_expr(*arg));

  uint32_t glsl_op = 0;

  if (name == "normalize")
    glsl_op = GLSLstd450Normalize;
  else if (name == "length")
    glsl_op = GLSLstd450Length;
  else if (name == "distance")
    glsl_op = GLSLstd450Distance;
  else if (name == "cross")
    glsl_op = GLSLstd450Cross;
  else if (name == "reflect")
    glsl_op = GLSLstd450Reflect;
  else if (name == "refract")
    glsl_op = GLSLstd450Refract;
  else if (name == "inverse")
    glsl_op = GLSLstd450MatrixInverse;
  else if (name == "abs") {
    TypeRef arg_type = !call.args.empty() ? infer_expr_type(*call.args[0]) : TypeRef{};
    if (is_float_type(arg_type))
      glsl_op = GLSLstd450FAbs;
    else
      glsl_op = GLSLstd450SAbs;
  } else if (name == "sign") {
    TypeRef arg_type = !call.args.empty() ? infer_expr_type(*call.args[0]) : TypeRef{};
    if (is_float_type(arg_type))
      glsl_op = GLSLstd450FSign;
    else
      glsl_op = GLSLstd450SSign;
  } else if (name == "floor")
    glsl_op = GLSLstd450Floor;
  else if (name == "ceil")
    glsl_op = GLSLstd450Ceil;
  else if (name == "fract")
    glsl_op = GLSLstd450Fract;
  else if (name == "mod") {
    return m_builder.op_f_mod(type_id, arg_ids[0], arg_ids[1]);
  } else if (name == "min") {
    TypeRef arg_type = !call.args.empty() ? infer_expr_type(*call.args[0]) : TypeRef{};
    if (is_float_type(arg_type))
      glsl_op = GLSLstd450FMin;
    else if (is_uint_type(arg_type))
      glsl_op = GLSLstd450UMin;
    else
      glsl_op = GLSLstd450SMin;
  } else if (name == "max") {
    TypeRef arg_type = !call.args.empty() ? infer_expr_type(*call.args[0]) : TypeRef{};
    if (is_float_type(arg_type))
      glsl_op = GLSLstd450FMax;
    else if (is_uint_type(arg_type))
      glsl_op = GLSLstd450UMax;
    else
      glsl_op = GLSLstd450SMax;
  } else if (name == "clamp") {
    TypeRef arg_type = !call.args.empty() ? infer_expr_type(*call.args[0]) : TypeRef{};
    if (is_float_type(arg_type))
      glsl_op = GLSLstd450FClamp;
    else if (is_uint_type(arg_type))
      glsl_op = GLSLstd450UClamp;
    else
      glsl_op = GLSLstd450SClamp;
  } else if (name == "mix")
    glsl_op = GLSLstd450FMix;
  else if (name == "step")
    glsl_op = GLSLstd450Step;
  else if (name == "smoothstep")
    glsl_op = GLSLstd450SmoothStep;
  else if (name == "pow")
    glsl_op = GLSLstd450Pow;
  else if (name == "sqrt")
    glsl_op = GLSLstd450Sqrt;
  else if (name == "inversesqrt")
    glsl_op = GLSLstd450InverseSqrt;
  else if (name == "sin")
    glsl_op = GLSLstd450Sin;
  else if (name == "cos")
    glsl_op = GLSLstd450Cos;
  else if (name == "tan")
    glsl_op = GLSLstd450Tan;
  else if (name == "asin")
    glsl_op = GLSLstd450Asin;
  else if (name == "acos")
    glsl_op = GLSLstd450Acos;
  else if (name == "atan")
    glsl_op = GLSLstd450Atan;
  else if (name == "exp")
    glsl_op = GLSLstd450Exp;
  else if (name == "log")
    glsl_op = GLSLstd450Log;
  else if (name == "exp2")
    glsl_op = GLSLstd450Exp2;
  else if (name == "log2")
    glsl_op = GLSLstd450Log2;
  else if (name == "round")
    glsl_op = GLSLstd450Round;
  else if (name == "trunc")
    glsl_op = GLSLstd450Trunc;

  if (glsl_op != 0) {
    return m_builder.op_ext_inst(type_id, m_glsl_ext, glsl_op, arg_ids);
  }

  add_error("[SPIRV] Unsupported builtin or unresolved function call: " + name);
  return 0;
}

VulkanSPIRVEmitResult
VulkanSPIRVEmitter::emit(const CanonicalStage &stage, const StageReflection &reflection, const ShaderPipelineLayout &layout) {
  VulkanSPIRVEmitResult result;

  m_builder = SPIRVBuilder();
  m_input_variables.clear();
  m_output_variables.clear();
  m_sampler_variables.clear();
  m_ubo_map.clear();
  m_local_variables.clear();
  m_global_constants.clear();
  m_deferred_constants.clear();
  m_function_ids.clear();
  m_named_type_refs.clear();
  m_struct_type_ids.clear();
  m_struct_field_names.clear();
  m_struct_field_type_ids.clear();
  m_interface_variable_ids.clear();
  m_loop_merge_stack.clear();
  m_loop_continue_stack.clear();
  m_has_gl_position = false;
  m_has_gl_vertex_id = false;
  m_has_gl_instance_id = false;
  m_has_gl_frag_depth = false;
  m_has_gl_frag_coord = false;
  m_stage_kind = stage.stage;
  m_entry_function_id = 0;
  m_errors.clear();

  setup_module(stage.stage);
  emit_struct_types(stage.structs);

  for (const auto &decl : stage.declarations) {
    const CanonicalInterfaceBlockDecl *interface_block =
        std::get_if<CanonicalInterfaceBlockDecl>(&decl);
    const CanonicalBufferDecl *buffer_block =
        std::get_if<CanonicalBufferDecl>(&decl);

    std::string block_name;
    const std::vector<CanonicalFieldDecl> *fields = nullptr;

    if (interface_block && interface_block->is_storage_block) {
      block_name = interface_block->name;
      fields = &interface_block->fields;
    } else if (buffer_block && !buffer_block->is_uniform) {
      block_name = buffer_block->name;
      fields = &buffer_block->fields;
    }

    if (!fields || block_name.empty() ||
        m_struct_field_names.count(block_name))
      continue;

    std::vector<std::string> field_names;
    std::vector<uint32_t> field_type_ids;

    for (const auto &field : *fields) {
      TypeRef effective_type = field.type;
      if (field.array_size.has_value() && *field.array_size == 0) {
        effective_type.is_runtime_sized = true;
      } else if (field.array_size.has_value()) {
        effective_type.array_size = *field.array_size;
      }

      uint32_t field_type = resolve_type(effective_type);
      field_names.push_back(field.name);
      field_type_ids.push_back(field_type);
      m_named_type_refs[block_name + "." + field.name] = effective_type;
    }

    m_struct_field_names[block_name] = std::move(field_names);
    m_struct_field_type_ids[block_name] = std::move(field_type_ids);
  }

  emit_input_variables(reflection, stage.entry);
  emit_output_variables(reflection, stage.entry);
  emit_resource_variables(reflection, layout, stage.entry);
  emit_builtin_variables(stage.stage, stage.entry);
  emit_global_constants(stage.declarations);
  emit_helper_functions(stage.declarations);
  emit_entry_point(stage.entry);

  if (!m_errors.empty()) {
    result.errors = m_errors;
    return result;
  }

  spv::ExecutionModel execution_model =
      (stage.stage == StageKind::Vertex)     ? spv::ExecutionModelVertex
      : (stage.stage == StageKind::Fragment) ? spv::ExecutionModelFragment
      : (stage.stage == StageKind::Geometry) ? spv::ExecutionModelGeometry
                                             : spv::ExecutionModelVertex;

  m_builder.add_entry_point(execution_model, m_entry_function_id, "main", m_interface_variable_ids);

  if (stage.stage == StageKind::Fragment) {
    m_builder.add_execution_mode(m_entry_function_id, spv::ExecutionModeOriginUpperLeft);
    if (m_has_gl_frag_depth) {
      m_builder.add_execution_mode(m_entry_function_id, spv::ExecutionModeDepthReplacing);
    }
  }

  result.spirv = m_builder.finalize();
  return result;
}

} // namespace astralix
