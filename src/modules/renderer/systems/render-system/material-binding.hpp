#pragma once

#include "assert.hpp"
#include "components/material.hpp"
#include "glm/glm.hpp"
#include "managers/resource-manager.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "render-frame.hpp"
#include "resources/descriptors/material-descriptor.hpp"
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
  glm::vec3 emissive = glm::vec3(0.0f);
  float bloom_intensity = 0.0f;
  int next_texture_slot = 0;
};

struct MaterialTextureBindingPoint {
  std::string_view logical_name;
  uint64_t binding_id = 0;
};

struct MaterialBindingLayout {
  std::optional<MaterialTextureBindingPoint> diffuse;
  std::optional<MaterialTextureBindingPoint> specular;
  std::optional<MaterialTextureBindingPoint> normal_map;
  std::optional<MaterialTextureBindingPoint> displacement_map;
};

struct TextureBindingIdentity {
  ResourceDescriptorID descriptor_id;
  uint64_t object_id = 0;
  std::string name;
  bool cubemap = false;

  friend bool operator==(const TextureBindingIdentity &,
                         const TextureBindingIdentity &) = default;
};

struct MaterialGroupKey {
  ResourceDescriptorID material_id;
  TextureBindingIdentity diffuse;
  TextureBindingIdentity specular;
  TextureBindingIdentity normal;
  TextureBindingIdentity displacement;
  std::array<float, 3> emissive = {0.0f, 0.0f, 0.0f};
  float bloom_intensity = 0.0f;
  std::vector<TextureBindingIdentity> extra_textures;

  friend bool operator==(const MaterialGroupKey &,
                         const MaterialGroupKey &) = default;
};

struct MaterialGroupKeyHash {
  size_t operator()(const MaterialGroupKey &key) const noexcept {
    auto hash_texture_identity =
        [](size_t seed, const TextureBindingIdentity &identity) {
          seed = hash_combine(seed, std::hash<std::string>{}(identity.descriptor_id));
          seed = hash_combine(seed, std::hash<uint64_t>{}(identity.object_id));
          seed = hash_combine(seed, std::hash<std::string>{}(identity.name));
          seed = hash_combine(seed, std::hash<bool>{}(identity.cubemap));
          return seed;
        };

    size_t seed = std::hash<std::string>{}(key.material_id);
    seed = hash_texture_identity(seed, key.diffuse);
    seed = hash_texture_identity(seed, key.specular);
    seed = hash_texture_identity(seed, key.normal);
    seed = hash_texture_identity(seed, key.displacement);
    seed = hash_combine(seed, std::hash<float>{}(key.emissive[0]));
    seed = hash_combine(seed, std::hash<float>{}(key.emissive[1]));
    seed = hash_combine(seed, std::hash<float>{}(key.emissive[2]));
    seed = hash_combine(seed, std::hash<float>{}(key.bloom_intensity));
    for (const auto &texture : key.extra_textures) {
      seed = hash_texture_identity(seed, texture);
    }
    return seed;
  }
};

enum class MaterialBindingSemantic : uint8_t {
  Diffuse,
  Specular,
  NormalMap,
  DisplacementMap,
};

inline BloomSettings resolve_bloom_settings(const BloomSettings *settings) {
  return settings != nullptr ? *settings : BloomSettings{};
}

inline void finalize_material_binding_state(MaterialBindingState &state) {
  if (state.diffuse_slot < 0 && state.specular_slot >= 0) {
    state.diffuse_slot = state.specular_slot;
  }

  if (state.specular_slot < 0 && state.diffuse_slot >= 0) {
    state.specular_slot = state.diffuse_slot;
  }
}

inline const MaterialSlots *
resolve_material_slots(const Model *model, const MaterialSlots *material_slots,
                       MaterialSlots &fallback_slots) {
  if (material_slots != nullptr && !material_slots->materials.empty()) {
    return material_slots;
  }

  if (model == nullptr || model->materials.empty()) {
    return material_slots;
  }

  fallback_slots.materials = model->materials;
  return &fallback_slots;
}

inline Ref<Texture> resolve_texture_resource(const ResourceDescriptorID &id,
                                             bool cubemap) {
  if (id.empty()) {
    return nullptr;
  }

  return cubemap ? Ref<Texture>(resource_manager()->get_by_descriptor_id<Texture3D>(id))
                 : Ref<Texture>(resource_manager()->get_by_descriptor_id<Texture2D>(id));
}

inline uint64_t texture_object_identity(const Ref<Texture> &texture) {
  return texture != nullptr ? reinterpret_cast<uint64_t>(texture.get()) : 0ull;
}

inline TextureBindingIdentity make_texture_binding_identity(
    const ResourceDescriptorID &descriptor_id,
    const Ref<Texture> &texture,
    bool cubemap = false,
    std::string name = {}
) {
  return TextureBindingIdentity{
      .descriptor_id = descriptor_id,
      .object_id = texture_object_identity(texture),
      .name = std::move(name),
      .cubemap = cubemap,
  };
}

inline ResolvedMaterialData
resolve_material_data(const Model *model, const MaterialSlots *material_slots,
                      const TextureBindings *texture_bindings) {
  ResolvedMaterialData material_data;
  MaterialSlots fallback_slots;

  material_slots = resolve_material_slots(model, material_slots, fallback_slots);

  if (material_slots != nullptr && !material_slots->materials.empty()) {
    material_data.material_id = material_slots->materials.front();

    auto material =
        resource_manager()->get_by_descriptor_id<MaterialDescriptor>(
            material_data.material_id
        );

    if (material != nullptr) {
      material_data.emissive = material->emissive;
      material_data.bloom_intensity = material->bloom_intensity;

      if (!material->diffuse_ids.empty()) {
        material_data.diffuse_descriptor_id = material->diffuse_ids[0];
        material_data.diffuse =
            resource_manager()->get_by_descriptor_id<Texture2D>(
                material->diffuse_ids[0]);
      }

      if (!material->specular_ids.empty()) {
        material_data.specular_descriptor_id = material->specular_ids[0];
        material_data.specular =
            resource_manager()->get_by_descriptor_id<Texture2D>(
                material->specular_ids[0]);
      }

      if (material->normal_map_ids.has_value()) {
        material_data.normal_descriptor_id = *material->normal_map_ids;
        material_data.normal =
            resource_manager()->get_by_descriptor_id<Texture2D>(
                *material->normal_map_ids);
      }

      if (material->displacement_map_ids.has_value()) {
        material_data.displacement_descriptor_id =
            *material->displacement_map_ids;
        material_data.displacement =
            resource_manager()->get_by_descriptor_id<Texture2D>(
                *material->displacement_map_ids);
      }
    }
  }

  if (texture_bindings != nullptr) {
    for (const auto &binding : texture_bindings->bindings) {
      material_data.extra_textures.push_back(ResolvedTextureBinding{
          .descriptor_id = binding.id,
          .name = binding.name,
          .texture = resolve_texture_resource(binding.id, binding.cubemap),
          .cubemap = binding.cubemap,
      });
    }
  }

  return material_data;
}

inline MaterialGroupKey make_material_group_key(
    const ResolvedMaterialData &material_data
) {
  MaterialGroupKey key;
  key.material_id = material_data.material_id;
  key.diffuse = make_texture_binding_identity(
      material_data.diffuse_descriptor_id, material_data.diffuse
  );
  key.specular = make_texture_binding_identity(
      material_data.specular_descriptor_id, material_data.specular
  );
  key.normal = make_texture_binding_identity(
      material_data.normal_descriptor_id, material_data.normal
  );
  key.displacement = make_texture_binding_identity(
      material_data.displacement_descriptor_id, material_data.displacement
  );
  key.emissive = {
      material_data.emissive.x,
      material_data.emissive.y,
      material_data.emissive.z,
  };
  key.bloom_intensity = material_data.bloom_intensity;

  key.extra_textures.reserve(material_data.extra_textures.size());
  for (const auto &binding : material_data.extra_textures) {
    key.extra_textures.push_back(make_texture_binding_identity(
        binding.descriptor_id,
        binding.texture,
        binding.cubemap,
        binding.name
    ));
  }

  return key;
}

inline int record_resident_texture_binding(
    CompiledFrame &frame, RenderBindingGroupHandle binding_group,
    const Ref<Texture> &texture, bool cubemap, int slot,
    const std::string &logical_name, const std::string &debug_name,
    uint64_t binding_id
) {
  if (texture == nullptr || slot < 0) {
    return -1;
  }

  ASTRA_ENSURE(
      binding_id == 0, "Cannot record material binding '", logical_name,
      "' with binding id 0"
  );

  const auto image =
      cubemap ? frame.register_texture_cube(debug_name, texture)
              : frame.register_texture_2d(debug_name, texture);

  frame.add_sampled_image_binding(
      binding_group, binding_id, ImageViewRef{.image = image},
      cubemap ? CompiledSampledImageTarget::TextureCube
              : CompiledSampledImageTarget::Texture2D
  );
  return slot;
}

inline void update_material_binding_slot(
    MaterialBindingState &state, MaterialBindingSemantic semantic, int slot
) {
  switch (semantic) {
  case MaterialBindingSemantic::Diffuse:
    if (state.diffuse_slot < 0) {
      state.diffuse_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Specular:
    if (state.specular_slot < 0) {
      state.specular_slot = slot;
    }
    return;
  case MaterialBindingSemantic::NormalMap:
    if (state.normal_map_slot < 0) {
      state.normal_map_slot = slot;
    }
    return;
  case MaterialBindingSemantic::DisplacementMap:
    if (state.displacement_map_slot < 0) {
      state.displacement_map_slot = slot;
    }
    return;
  }
}

inline std::optional<MaterialBindingSemantic> resolve_material_binding_semantic(
    const MaterialBindingLayout &layout, std::string_view name
) {
  if (layout.diffuse.has_value() && name == layout.diffuse->logical_name) {
    return MaterialBindingSemantic::Diffuse;
  }
  if (layout.specular.has_value() && name == layout.specular->logical_name) {
    return MaterialBindingSemantic::Specular;
  }
  if (layout.normal_map.has_value() && name == layout.normal_map->logical_name) {
    return MaterialBindingSemantic::NormalMap;
  }
  if (layout.displacement_map.has_value() &&
      name == layout.displacement_map->logical_name) {
    return MaterialBindingSemantic::DisplacementMap;
  }

  return std::nullopt;
}

inline MaterialBindingState record_resolved_material_bindings(
    CompiledFrame &frame, RenderBindingGroupHandle binding_group,
    const ResolvedMaterialData &material_data,
    const MaterialBindingLayout &layout, int starting_slot = 0
) {
  MaterialBindingState state;
  state.emissive = material_data.emissive;
  state.bloom_intensity = material_data.bloom_intensity;
  state.next_texture_slot = starting_slot;

  if (material_data.diffuse != nullptr && layout.diffuse.has_value()) {
    state.diffuse_slot = record_resident_texture_binding(
        frame, binding_group, material_data.diffuse, false, state.next_texture_slot++,
        std::string(layout.diffuse->logical_name), "material.diffuse",
        layout.diffuse->binding_id
    );
  }

  if (material_data.specular != nullptr && layout.specular.has_value()) {
    state.specular_slot = record_resident_texture_binding(
        frame, binding_group, material_data.specular, false, state.next_texture_slot++,
        std::string(layout.specular->logical_name), "material.specular",
        layout.specular->binding_id
    );
  }

  if (material_data.normal != nullptr && layout.normal_map.has_value()) {
    state.normal_map_slot = record_resident_texture_binding(
        frame, binding_group, material_data.normal, false, state.next_texture_slot++,
        std::string(layout.normal_map->logical_name), "material.normal_map",
        layout.normal_map->binding_id
    );
  }

  if (material_data.displacement != nullptr &&
      layout.displacement_map.has_value()) {
    state.displacement_map_slot = record_resident_texture_binding(
        frame, binding_group, material_data.displacement, false,
        state.next_texture_slot++,
        std::string(layout.displacement_map->logical_name),
        "material.displacement_map", layout.displacement_map->binding_id
    );
  }

  for (const auto &binding : material_data.extra_textures) {
    const auto semantic =
        resolve_material_binding_semantic(layout, binding.name);
    if (!semantic.has_value()) {
      continue;
    }

    const auto *binding_point = [&]() -> const MaterialTextureBindingPoint * {
      switch (*semantic) {
      case MaterialBindingSemantic::Diffuse:
        return layout.diffuse.has_value() ? &*layout.diffuse : nullptr;
      case MaterialBindingSemantic::Specular:
        return layout.specular.has_value() ? &*layout.specular : nullptr;
      case MaterialBindingSemantic::NormalMap:
        return layout.normal_map.has_value() ? &*layout.normal_map : nullptr;
      case MaterialBindingSemantic::DisplacementMap:
        return layout.displacement_map.has_value()
                   ? &*layout.displacement_map
                   : nullptr;
      }

      return nullptr;
    }();
    if (binding_point == nullptr) {
      continue;
    }

    const int slot = record_resident_texture_binding(
        frame, binding_group, binding.texture, binding.cubemap, state.next_texture_slot,
        std::string(binding_point->logical_name),
        "material.extra." + binding.name, binding_point->binding_id
    );
    if (slot < 0) {
      continue;
    }

    state.next_texture_slot = slot + 1;
    update_material_binding_slot(state, *semantic, slot);
  }

  finalize_material_binding_state(state);
  return state;
}

} // namespace astralix::rendering
