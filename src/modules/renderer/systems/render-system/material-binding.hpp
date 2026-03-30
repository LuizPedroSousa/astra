#pragma once

#include "components/material.hpp"
#include "components/model.hpp"
#include "managers/resource-manager.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/model.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include <string_view>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix::rendering {

struct MaterialBindingState {
  int diffuse_slot = -1;
  int specular_slot = -1;
  int normal_map_slot = -1;
  int displacement_map_slot = -1;
  float shininess = 32.0f;
  int next_texture_slot = 0;
};

inline void finalize_material_binding_state(MaterialBindingState &state) {
  if (state.diffuse_slot < 0 && state.specular_slot >= 0) {
    state.diffuse_slot = state.specular_slot;
  }

  if (state.specular_slot < 0 && state.diffuse_slot >= 0) {
    state.specular_slot = state.diffuse_slot;
  }
}

inline int bind_texture_2d(RendererAPI *renderer_api, const ResourceDescriptorID &descriptor_id, int slot) {
  if (descriptor_id.empty()) {
    return -1;
  }

  const auto backend = renderer_api->get_backend();

  resource_manager()->load_from_descriptors_by_ids<Texture2DDescriptor>(
      backend, {descriptor_id}
  );

  auto texture =
      resource_manager()->get_by_descriptor_id<Texture2D>(descriptor_id);
  if (texture == nullptr) {
    return -1;
  }

  renderer_api->bind_texture_2d(texture->renderer_id(), slot);
  return slot;
}

inline int bind_texture_3d(RendererAPI *renderer_api, const ResourceDescriptorID &descriptor_id, int slot) {
  if (descriptor_id.empty()) {
    return -1;
  }

  const auto backend = renderer_api->get_backend();

  resource_manager()->load_from_descriptors_by_ids<Texture3DDescriptor>(
      backend, {descriptor_id}
  );

  auto texture =
      resource_manager()->get_by_descriptor_id<Texture3D>(descriptor_id);
  if (texture == nullptr) {
    return -1;
  }

  renderer_api->bind_texture_cube(texture->renderer_id(), slot);
  return slot;
}

inline const MaterialSlots *
resolve_material_slots(RendererBackend backend, const ModelRef *model_ref, const MaterialSlots *material_slots, MaterialSlots &fallback_slots) {
  if (material_slots != nullptr && !material_slots->materials.empty()) {
    return material_slots;
  }

  if (model_ref == nullptr) {
    return material_slots;
  }

  for (const auto &resource_id : model_ref->resource_ids) {
    resource_manager()->load_from_descriptors_by_ids<ModelDescriptor>(
        backend, {resource_id}
    );

    auto model = resource_manager()->get_by_descriptor_id<Model>(resource_id);
    if (model == nullptr || model->materials.empty()) {
      continue;
    }

    fallback_slots.materials = model->materials;
    return &fallback_slots;
  }

  return material_slots;
}

inline MaterialBindingState bind_material_slots(
    RendererAPI *renderer_api, Ref<Shader> shader, const ModelRef *model_ref,
    const MaterialSlots *material_slots,
    const TextureBindings *texture_bindings, int starting_slot = 0
) {
  const auto backend = renderer_api->get_backend();
  MaterialBindingState state;
  state.next_texture_slot = starting_slot;
  MaterialSlots fallback_slots;

  material_slots = resolve_material_slots(backend, model_ref, material_slots, fallback_slots);

  if (material_slots != nullptr && !material_slots->materials.empty()) {
    auto material =
        resource_manager()->get_by_descriptor_id<MaterialDescriptor>(
            material_slots->materials.front()
        );

    if (material != nullptr) {
      if (!material->diffuse_ids.empty()) {
        state.diffuse_slot = bind_texture_2d(
            renderer_api, material->diffuse_ids[0], state.next_texture_slot++
        );
      }

      if (!material->specular_ids.empty()) {
        state.specular_slot = bind_texture_2d(
            renderer_api, material->specular_ids[0], state.next_texture_slot++
        );
      }

      if (material->normal_map_ids.has_value()) {
        state.normal_map_slot = bind_texture_2d(
            renderer_api, *material->normal_map_ids, state.next_texture_slot++
        );
      }

      if (material->displacement_map_ids.has_value()) {
        state.displacement_map_slot =
            bind_texture_2d(renderer_api, *material->displacement_map_ids, state.next_texture_slot++);
      }
    }
  }

  if (texture_bindings != nullptr) {
    for (const auto &binding : texture_bindings->bindings) {
      int slot = binding.cubemap ? bind_texture_3d(renderer_api, binding.id, state.next_texture_slot)
                                 : bind_texture_2d(renderer_api, binding.id, state.next_texture_slot);

      if (slot >= 0) {
        shader->set_int(binding.name, slot);
        state.next_texture_slot = slot + 1;

        const std::string_view name = binding.name;
        if ((name == "materials[0].diffuse" ||
             name == "light.materials[0].diffuse") &&
            state.diffuse_slot < 0) {
          state.diffuse_slot = slot;
        }

        if ((name == "materials[0].specular" ||
             name == "light.materials[0].specular") &&
            state.specular_slot < 0) {
          state.specular_slot = slot;
        }

        if ((name == "normal_map" || name == "light.normal_map") &&
            state.normal_map_slot < 0) {
          state.normal_map_slot = slot;
        }

        if ((name == "displacement_map" || name == "light.displacement_map") &&
            state.displacement_map_slot < 0) {
          state.displacement_map_slot = slot;
        }
      }
    }
  }

  finalize_material_binding_state(state);
  return state;
}

} // namespace astralix::rendering
