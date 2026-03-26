#pragma once

#include "components/collider.hpp"
#include "components/mesh.hpp"
#include "components/model.hpp"
#include "managers/resource-manager.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/model.hpp"
#include "world.hpp"
#include <optional>
#include <vector>

namespace astralix {

struct MeshBounds {
  glm::vec3 min = glm::vec3(0.0f);
  glm::vec3 max = glm::vec3(0.0f);
  bool valid = false;
};

inline void expand_bounds(MeshBounds &bounds, const glm::vec3 &position) {
  if (!bounds.valid) {
    bounds.min = position;
    bounds.max = position;
    bounds.valid = true;
    return;
  }

  bounds.min = glm::min(bounds.min, position);
  bounds.max = glm::max(bounds.max, position);
}

inline void expand_bounds(MeshBounds &bounds, const Mesh &mesh) {
  for (const auto &vertex : mesh.vertices) {
    expand_bounds(bounds, vertex.position);
  }
}

inline std::optional<MeshBounds>
compute_mesh_bounds(const rendering::MeshSet &mesh_set) {
  MeshBounds bounds;

  for (const auto &mesh : mesh_set.meshes) {
    expand_bounds(bounds, mesh);
  }

  if (!bounds.valid) {
    return std::nullopt;
  }

  return bounds;
}

inline std::optional<MeshBounds>
compute_mesh_bounds(const rendering::ModelRef &model_ref) {
  MeshBounds bounds;

  for (const auto &resource_id : model_ref.resource_ids) {
    resource_manager()->load_from_descriptors_by_ids<ModelDescriptor>(
        RendererBackend::None, {resource_id});

    auto model = resource_manager()->get_by_descriptor_id<Model>(resource_id);
    if (model == nullptr) {
      continue;
    }

    for (const auto &mesh : model->meshes) {
      expand_bounds(bounds, mesh);
    }
  }

  if (!bounds.valid) {
    return std::nullopt;
  }

  return bounds;
}

inline std::optional<MeshBounds>
compute_mesh_bounds(const rendering::ModelRef *model_ref,
                    const rendering::MeshSet *mesh_set) {
  if (mesh_set != nullptr) {
    return compute_mesh_bounds(*mesh_set);
  }

  if (model_ref != nullptr) {
    return compute_mesh_bounds(*model_ref);
  }

  return std::nullopt;
}

struct ResolvedCollider {
  glm::vec3 half_extents = glm::vec3(0.5f);
  glm::vec3 center = glm::vec3(0.0f);
};

inline void resolve_box_colliders_from_render_mesh(ecs::World &world) {
  std::vector<EntityID> pending;

  world.each<physics::FitBoxColliderFromRenderMesh>(
      [&](EntityID entity_id, physics::FitBoxColliderFromRenderMesh &) {
        if (world.active(entity_id)) {
          pending.push_back(entity_id);
        }
      });

  for (EntityID entity_id : pending) {
    auto entity = world.entity(entity_id);
    auto *model_ref = entity.get<rendering::ModelRef>();
    auto *mesh_set = entity.get<rendering::MeshSet>();
    auto bounds = compute_mesh_bounds(model_ref, mesh_set);
    if (!bounds.has_value()) {
      continue;
    }

    entity.emplace<physics::BoxCollider>(physics::BoxCollider{
        .half_extents = (bounds->max - bounds->min) * 0.5f,
        .center = (bounds->max + bounds->min) * 0.5f,
    });
    entity.erase<physics::FitBoxColliderFromRenderMesh>();
  }
}

inline ResolvedCollider
resolve_collider_shape(const physics::BoxCollider &collider) {
  return ResolvedCollider{
      .half_extents = collider.half_extents,
      .center = collider.center,
  };
}

} // namespace astralix
