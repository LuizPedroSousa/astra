#pragma once

#include "components/material.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"
#include <string>
#include <vector>

namespace astralix::serialization {

inline ComponentSnapshot
snapshot_component(const rendering::MaterialSlots &material_slots) {
  ComponentSnapshot snapshot{.name = "MaterialSlots"};
  for (size_t i = 0; i < material_slots.materials.size(); ++i) {
    snapshot.fields.push_back(
        {"material_" + std::to_string(i), material_slots.materials[i]});
  }
  return snapshot;
}

inline ComponentSnapshot snapshot_component(const rendering::ShaderBinding &binding) {
  ComponentSnapshot snapshot{.name = "ShaderBinding"};
  snapshot.fields.push_back({"shader", binding.shader});
  return snapshot;
}

inline ComponentSnapshot snapshot_component(const rendering::TextureBindings &bindings) {
  ComponentSnapshot snapshot{.name = "TextureBindings"};
  for (size_t i = 0; i < bindings.bindings.size(); ++i) {
    const auto &binding = bindings.bindings[i];
    const auto prefix = "binding_" + std::to_string(i);
    snapshot.fields.push_back({prefix + ".id", binding.id});
    snapshot.fields.push_back({prefix + ".name", binding.name});
    snapshot.fields.push_back({prefix + ".cubemap", binding.cubemap});
  }
  return snapshot;
}

inline std::vector<rendering::TextureBinding>
read_texture_bindings(const serialization::fields::FieldList &fields) {
  std::vector<rendering::TextureBinding> bindings;

  for (size_t index = 0;; ++index) {
    const auto prefix = "binding_" + std::to_string(index);
    auto id = serialization::fields::read_string(fields, prefix + ".id");
    if (!id.has_value()) {
      break;
    }

    bindings.push_back(rendering::TextureBinding{
        .id = *id,
        .name =
            serialization::fields::read_string(fields, prefix + ".name")
                .value_or(""),
        .cubemap =
            serialization::fields::read_bool(fields, prefix + ".cubemap")
                .value_or(false),
    });
  }

  return bindings;
}

inline void apply_material_slots_snapshot(ecs::EntityRef entity,
                                          const serialization::fields::
                                              FieldList &fields) {
  entity.emplace<rendering::MaterialSlots>(
      rendering::MaterialSlots{.materials =
                        serialization::fields::read_string_series(fields,
                                                                  "material_")});
}

inline void apply_shader_binding_snapshot(ecs::EntityRef entity,
                                          const serialization::fields::
                                              FieldList &fields) {
  entity.emplace<rendering::ShaderBinding>(
      rendering::ShaderBinding{.shader =
                        serialization::fields::read_string(fields, "shader")
                            .value_or("")});
}

inline void apply_texture_bindings_snapshot(ecs::EntityRef entity,
                                            const serialization::fields::
                                                FieldList &fields) {
  entity.emplace<rendering::TextureBindings>(
      rendering::TextureBindings{.bindings = read_texture_bindings(fields)});
}

} // namespace astralix::serialization
