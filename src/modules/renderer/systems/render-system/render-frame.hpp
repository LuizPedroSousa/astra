#pragma once

#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/model.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "world.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::rendering {

struct RenderPacket {
  EntityID entity_id;
  size_t batch_key = 0u;
  bool dirty = true;
  const scene::Transform *transform = nullptr;
  ModelRef *model_ref = nullptr;
  MeshSet *mesh_set = nullptr;
  const MaterialSlots *materials = nullptr;
  const ShaderBinding *shader = nullptr;
  const TextureBindings *textures = nullptr;
};

struct RenderBatch {
  size_t key = 0u;
  std::vector<EntityID> entities;
};

struct RenderFrameData {
  std::vector<RenderPacket> packets;
  std::vector<RenderBatch> batches;
};

struct RenderRuntimeState {
  size_t batch_key = 0u;
  glm::mat4 transform = glm::mat4(1.0f);
  bool initialized = false;
};

struct RenderRuntimeStore {
  std::unordered_map<EntityID, RenderRuntimeState> entity_states;

  void prune(const ecs::World &world) {
    std::erase_if(entity_states, [&](const auto &entry) {
      return !world.contains(entry.first);
    });
  }
};

inline size_t hash_combine(size_t seed, size_t value) {
  return seed ^ (value + 0x9e3779b9 + (seed << 6u) + (seed >> 2u));
}

inline bool matrix_equal(const glm::mat4 &lhs, const glm::mat4 &rhs) {
  const float *lhs_ptr = glm::value_ptr(lhs);
  const float *rhs_ptr = glm::value_ptr(rhs);
  return std::equal(lhs_ptr, lhs_ptr + 16, rhs_ptr);
}

inline size_t compute_batch_key(const ModelRef *model_ref, const MeshSet *mesh_set, const ShaderBinding &shader, const MaterialSlots *materials, const TextureBindings *textures) {
  size_t seed = std::hash<std::string>{}(shader.shader);

  if (model_ref != nullptr) {
    for (const auto &resource_id : model_ref->resource_ids) {
      seed = hash_combine(seed, std::hash<std::string>{}(resource_id));
    }
  }

  if (mesh_set != nullptr) {
    for (const auto &mesh : mesh_set->meshes) {
      seed = hash_combine(seed, std::hash<size_t>{}(mesh.id));
    }
  }

  if (materials != nullptr) {
    for (const auto &material : materials->materials) {
      seed = hash_combine(seed, std::hash<std::string>{}(material));
    }
  }

  if (textures != nullptr) {
    for (const auto &binding : textures->bindings) {
      seed = hash_combine(seed, std::hash<std::string>{}(binding.id));
      seed = hash_combine(seed, std::hash<std::string>{}(binding.name));
      seed = hash_combine(seed, std::hash<bool>{}(binding.cubemap));
    }
  }

  return seed;
}

inline RenderFrameData collect_render_frame(ecs::World &world, RenderRuntimeStore &runtime_store) {
  runtime_store.prune(world);

  RenderFrameData frame;
  std::unordered_map<size_t, size_t> batch_lookup;

  world.each<Renderable, scene::Transform, ShaderBinding>([&](EntityID entity_id,
                                                              Renderable &,
                                                              scene::Transform &transform,
                                                              ShaderBinding &shader) {
    if (!world.active(entity_id)) {
      return;
    }

    auto entity = world.entity(entity_id);
    auto *model_ref = entity.get<ModelRef>();
    auto *mesh_set = entity.get<MeshSet>();
    if (model_ref == nullptr && mesh_set == nullptr) {
      return;
    }

    auto *materials = entity.get<MaterialSlots>();
    auto *textures = entity.get<TextureBindings>();

    const size_t batch_key =
        compute_batch_key(model_ref, mesh_set, shader, materials, textures);

    auto &runtime_state = runtime_store.entity_states[entity_id];
    const bool dirty = !runtime_state.initialized ||
                       runtime_state.batch_key != batch_key ||
                       !matrix_equal(runtime_state.transform, transform.matrix);

    runtime_state.batch_key = batch_key;
    runtime_state.transform = transform.matrix;
    runtime_state.initialized = true;

    frame.packets.push_back(RenderPacket{
        .entity_id = entity_id,
        .batch_key = batch_key,
        .dirty = dirty,
        .transform = &transform,
        .model_ref = model_ref,
        .mesh_set = mesh_set,
        .materials = materials,
        .shader = &shader,
        .textures = textures,
    });

    auto [it, inserted] = batch_lookup.emplace(batch_key, frame.batches.size());
    if (inserted) {
      frame.batches.push_back(RenderBatch{.key = batch_key});
    }

    frame.batches[it->second].entities.push_back(entity_id);
  });

  return frame;
}

} // namespace astralix::rendering
