#pragma once

#include "components/material.hpp"
#include "components/model.hpp"
#include "components/render-resource-request.hpp"
#include "components/tags.hpp"
#include "log.hpp"
#include "world.hpp"
#include <vector>

namespace astralix::rendering {

inline void expand_render_resource_requests(ecs::World &world) {
  std::vector<EntityID> pending;

  world.each<RenderResourceRequest>([&](EntityID entity_id,
                                        RenderResourceRequest &) {
    if (world.active(entity_id)) {
      pending.push_back(entity_id);
    }
  });

  for (EntityID entity_id : pending) {
    auto entity = world.entity(entity_id);
    const auto *request = entity.get<RenderResourceRequest>();
    if (request == nullptr) {
      continue;
    }

    const RenderResourceRequest request_copy = *request;

    if (!request_copy.model_ids.empty()) {
      entity.emplace<ModelRef>(ModelRef{.resource_ids = request_copy.model_ids});
    }

    if (!request_copy.shader_id.empty()) {
      entity.emplace<ShaderBinding>(
          ShaderBinding{.shader = request_copy.shader_id}
      );
    }

    if (!request_copy.material_ids.empty()) {
      entity.emplace<MaterialSlots>(
          MaterialSlots{.materials = request_copy.material_ids}
      );
    }

    if (!request_copy.textures.empty()) {
      entity.emplace<TextureBindings>(
          TextureBindings{.bindings = request_copy.textures}
      );
    }

    if (request_copy.renderable) {
      entity.emplace<Renderable>();
    } else {
      entity.erase<Renderable>();
    }

    entity.erase<RenderResourceRequest>();
  }
}

} // namespace astralix::rendering
