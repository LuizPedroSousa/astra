#pragma once

#include "shader-lang/ast.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astralix {

enum class ShaderResourceBindingKind : uint8_t {
  Sampler,
  UniformBlock,
  StorageBuffer,
};

struct ShaderValueFieldDesc {
  uint64_t binding_id = 0;
  std::string logical_name;
  TypeRef type{};
  uint32_t stage_mask = 0;
  uint32_t offset = 0;
  uint32_t size = 0;
  uint32_t array_stride = 0;
  uint32_t matrix_stride = 0;
};

struct ShaderValueBlockDesc {
  uint64_t block_id = 0;
  std::string logical_name;
  std::optional<uint32_t> descriptor_set;
  std::optional<uint32_t> binding;
  uint32_t size = 0;
  std::vector<ShaderValueFieldDesc> fields;
};

struct ShaderResourceBindingDesc {
  uint64_t binding_id = 0;
  std::string logical_name;
  ShaderResourceBindingKind source_kind = ShaderResourceBindingKind::Sampler;
  TypeRef type{};
  uint32_t stage_mask = 0;
  uint32_t descriptor_set = 0;
  uint32_t binding = 0;
  std::optional<uint32_t> array_size;
};

struct ShaderResourceLayout {
  std::vector<ShaderValueBlockDesc> value_blocks;
  std::vector<ShaderResourceBindingDesc> resources;
  uint64_t compatibility_hash = 0;
};

struct VertexAttributeDesc {
  std::string logical_name;
  uint32_t location = 0;
  TypeRef type{};
  uint32_t offset = 0;
  uint32_t stride = 0;
  bool per_instance = false;
};

struct VertexInputLayoutDesc {
  std::vector<VertexAttributeDesc> attributes;
  uint64_t compatibility_hash = 0;
};

struct DescriptorSetLayoutDesc {
  uint32_t set = 0;
  std::vector<ShaderResourceBindingDesc> bindings;
};

struct PipelineLayoutDesc {
  std::vector<DescriptorSetLayoutDesc> descriptor_sets;
  std::vector<ShaderValueBlockDesc> value_blocks;
  uint64_t compatibility_hash = 0;
};

struct ShaderPipelineLayout {
  ShaderResourceLayout resource_layout;
  VertexInputLayoutDesc vertex_input;
  PipelineLayoutDesc pipeline_layout;
};

} // namespace astralix
