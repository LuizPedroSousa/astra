#pragma once

#include "fnv1a.hpp"
#include "shader-lang/ast.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace astralix {

enum class ShaderResourceKind : uint8_t {
  UniformValue,
  Sampler,
  UniformInterface,
  UniformBlock,
  StorageBuffer,
};

using ShaderDefaultValue = std::variant<bool, int, float>;

constexpr uint32_t shader_stage_mask(StageKind stage) {
  return 1u << static_cast<uint32_t>(stage);
}

constexpr uint64_t shader_binding_id(std::string_view logical_name) {
  return fnv1a64_append_string(logical_name);
}

struct BackendLayoutReflection {
  std::optional<uint32_t> descriptor_set;
  std::optional<uint32_t> binding;
  std::optional<uint32_t> location;
  std::optional<std::string> emitted_name;
  std::optional<std::string> block_name;
  std::optional<std::string> instance_name;
  std::optional<std::string> storage;
};

struct DeclaredFieldReflection {
  std::string name;
  std::string logical_name;
  TypeRef type{};
  std::optional<uint32_t> array_size;
  std::optional<ShaderDefaultValue> default_value;
  uint32_t active_stage_mask = 0;
  uint64_t binding_id = 0;
  std::vector<DeclaredFieldReflection> fields;

  bool is_leaf() const { return fields.empty(); }
};

struct MemberReflection {
  std::string logical_name;
  std::optional<std::string> compatibility_alias;
  TypeRef type{};
  std::optional<uint32_t> array_size;
  uint64_t binding_id = 0;
  BackendLayoutReflection glsl;
};

struct ResourceReflection {
  std::string logical_name;
  ShaderResourceKind kind = ShaderResourceKind::UniformValue;
  StageKind stage = StageKind::Vertex;
  std::string declared_name;
  TypeRef type{};
  std::optional<uint32_t> array_size;
  uint64_t binding_id = 0;
  BackendLayoutReflection glsl;
  std::vector<MemberReflection> members;
  std::vector<DeclaredFieldReflection> declared_fields;
};

struct StageFieldReflection {
  std::string logical_name;
  std::optional<std::string> compatibility_alias;
  std::string interface_name;
  TypeRef type{};
  std::optional<uint32_t> array_size;
  BackendLayoutReflection glsl;
};

struct StageReflection {
  StageKind stage = StageKind::Vertex;
  std::vector<StageFieldReflection> stage_inputs;
  std::vector<StageFieldReflection> stage_outputs;
  std::vector<ResourceReflection> resources;
};

struct ShaderReflection {
  int version = 3;
  std::map<StageKind, StageReflection> stages;

  bool empty() const { return stages.empty(); }
};

} // namespace astralix
