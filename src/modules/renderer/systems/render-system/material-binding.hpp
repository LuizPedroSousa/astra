#pragma once

#include "assert.hpp"
#include "components/material.hpp"
#include "glm/glm.hpp"
#include "managers/resource-manager.hpp"
#include "pbr-default-textures.hpp"
#include "render-frame.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/model.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <string_view>

namespace astralix::rendering {

struct MaterialBindingState {
  int base_color_slot = -1;
  int normal_slot = -1;
  int metallic_slot = -1;
  int roughness_slot = -1;
  int occlusion_slot = -1;
  int emissive_slot = -1;
  int displacement_slot = -1;
  int metallic_channel = 0;
  int roughness_channel = 0;
  glm::vec4 base_color_factor = glm::vec4(1.0f);
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float height_scale = 0.02f;
  float bloom_intensity = 0.0f;
  int alpha_mask_enabled = 0;
  float alpha_cutoff = 0.5f;
  int next_texture_slot = 0;
};

struct MaterialTextureBindingPoint {
  std::string_view logical_name;
  uint64_t binding_id = 0;
};

struct MaterialBindingLayout {
  std::optional<MaterialTextureBindingPoint> base_color;
  std::optional<MaterialTextureBindingPoint> normal;
  std::optional<MaterialTextureBindingPoint> metallic;
  std::optional<MaterialTextureBindingPoint> roughness;
  std::optional<MaterialTextureBindingPoint> occlusion;
  std::optional<MaterialTextureBindingPoint> emissive;
  std::optional<MaterialTextureBindingPoint> displacement;
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
  TextureBindingIdentity base_color;
  TextureBindingIdentity normal;
  TextureBindingIdentity metallic;
  TextureBindingIdentity roughness;
  TextureBindingIdentity occlusion;
  TextureBindingIdentity emissive;
  TextureBindingIdentity displacement;
  int metallic_channel = 0;
  int roughness_channel = 0;
  std::array<float, 4> base_color_factor = {1.0f, 1.0f, 1.0f, 1.0f};
  std::array<float, 3> emissive_factor = {0.0f, 0.0f, 0.0f};
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float height_scale = 0.02f;
  float bloom_intensity = 0.0f;
  int alpha_mask_enabled = 0;
  float alpha_cutoff = 0.5f;
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
    seed = hash_texture_identity(seed, key.base_color);
    seed = hash_texture_identity(seed, key.normal);
    seed = hash_texture_identity(seed, key.metallic);
    seed = hash_texture_identity(seed, key.roughness);
    seed = hash_texture_identity(seed, key.occlusion);
    seed = hash_texture_identity(seed, key.emissive);
    seed = hash_texture_identity(seed, key.displacement);
    seed = hash_combine(seed, std::hash<int>{}(key.metallic_channel));
    seed = hash_combine(seed, std::hash<int>{}(key.roughness_channel));
    seed = hash_combine(seed, std::hash<float>{}(key.base_color_factor[0]));
    seed = hash_combine(seed, std::hash<float>{}(key.base_color_factor[1]));
    seed = hash_combine(seed, std::hash<float>{}(key.base_color_factor[2]));
    seed = hash_combine(seed, std::hash<float>{}(key.base_color_factor[3]));
    seed = hash_combine(seed, std::hash<float>{}(key.emissive_factor[0]));
    seed = hash_combine(seed, std::hash<float>{}(key.emissive_factor[1]));
    seed = hash_combine(seed, std::hash<float>{}(key.emissive_factor[2]));
    seed = hash_combine(seed, std::hash<float>{}(key.metallic_factor));
    seed = hash_combine(seed, std::hash<float>{}(key.roughness_factor));
    seed = hash_combine(seed, std::hash<float>{}(key.occlusion_strength));
    seed = hash_combine(seed, std::hash<float>{}(key.normal_scale));
    seed = hash_combine(seed, std::hash<float>{}(key.height_scale));
    seed = hash_combine(seed, std::hash<float>{}(key.bloom_intensity));
    seed = hash_combine(seed, std::hash<int>{}(key.alpha_mask_enabled));
    seed = hash_combine(seed, std::hash<float>{}(key.alpha_cutoff));
    for (const auto &texture : key.extra_textures) {
      seed = hash_texture_identity(seed, texture);
    }
    return seed;
  }
};

enum class MaterialBindingSemantic : uint8_t {
  BaseColor,
  Normal,
  Metallic,
  Roughness,
  Occlusion,
  Emissive,
  Displacement,
};

struct ResolvedSurfaceTextureBinding {
  ResourceDescriptorID descriptor_id;
  Ref<Texture> texture = nullptr;
  int channel = 0;
};

inline BloomSettings resolve_bloom_settings(const BloomSettings *settings) {
  return settings != nullptr ? *settings : BloomSettings{};
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

inline const ResourceDescriptorID *
resolve_material_id_for_submesh(const Model *model,
                                const MaterialSlots *material_slots,
                                uint32_t submesh_index) {
  if (material_slots != nullptr && !material_slots->materials.empty()) {
    if (material_slots->materials.size() == 1u) {
      return &material_slots->materials.front();
    }

    if (submesh_index < material_slots->materials.size()) {
      return &material_slots->materials[submesh_index];
    }

    return &material_slots->materials.front();
  }

  if (model == nullptr || model->materials.empty()) {
    return nullptr;
  }

  uint32_t material_slot = submesh_index;
  if (submesh_index < model->material_slots.size()) {
    material_slot = model->material_slots[submesh_index];
  }

  if (material_slot < model->materials.size()) {
    return &model->materials[material_slot];
  }

  return &model->materials.front();
}

inline Ref<Texture> resolve_texture_resource(const ResourceDescriptorID &id,
                                             bool cubemap) {
  auto manager = resource_manager();
  if (id.empty() || manager == nullptr) {
    return nullptr;
  }

  return cubemap ? Ref<Texture>(manager->get_by_descriptor_id<Texture3D>(id))
                 : Ref<Texture>(manager->get_by_descriptor_id<Texture2D>(id));
}

inline uint64_t texture_object_identity(const Ref<Texture> &texture) {
  return texture != nullptr ? reinterpret_cast<uint64_t>(texture.get()) : 0ull;
}

inline TextureBindingIdentity make_texture_binding_identity(
    const ResourceDescriptorID &descriptor_id, const Ref<Texture> &texture,
    bool cubemap = false, std::string name = {}) {
  return TextureBindingIdentity{
      .descriptor_id = descriptor_id,
      .object_id = texture_object_identity(texture),
      .name = std::move(name),
      .cubemap = cubemap,
  };
}

inline const ResourceDescriptorID &
default_texture_id_for_semantic(MaterialBindingSemantic semantic) {
  switch (semantic) {
  case MaterialBindingSemantic::BaseColor:
    return default_pbr_base_color_texture_id();
  case MaterialBindingSemantic::Normal:
    return default_pbr_normal_texture_id();
  case MaterialBindingSemantic::Metallic:
    return default_pbr_metallic_texture_id();
  case MaterialBindingSemantic::Roughness:
    return default_pbr_roughness_texture_id();
  case MaterialBindingSemantic::Occlusion:
    return default_pbr_occlusion_texture_id();
  case MaterialBindingSemantic::Emissive:
    return default_pbr_emissive_texture_id();
  case MaterialBindingSemantic::Displacement:
    return default_pbr_displacement_texture_id();
  }

  return default_pbr_base_color_texture_id();
}

inline Ref<Texture> default_texture_for_semantic(MaterialBindingSemantic semantic) {
  auto manager = resource_manager();
  if (manager == nullptr) {
    return nullptr;
  }

  return manager->get_by_descriptor_id<Texture2D>(
      default_texture_id_for_semantic(semantic));
}

inline std::pair<ResourceDescriptorID, Ref<Texture>> normalize_pbr_texture_binding(
    MaterialBindingSemantic semantic, const ResourceDescriptorID &descriptor_id,
    const Ref<Texture> &texture) {
  if (!descriptor_id.empty() && texture != nullptr) {
    return {descriptor_id, texture};
  }

  return {default_texture_id_for_semantic(semantic),
          default_texture_for_semantic(semantic)};
}

inline ResolvedSurfaceTextureBinding resolve_surface_texture_binding(
    const ResolvedMaterialData &material_data, MaterialBindingSemantic semantic
) {
  switch (semantic) {
  case MaterialBindingSemantic::BaseColor: {
    const auto [descriptor_id, texture] = normalize_pbr_texture_binding(
        semantic, material_data.base_color_descriptor_id, material_data.base_color
    );
    return {.descriptor_id = descriptor_id, .texture = texture, .channel = 0};
  }
  case MaterialBindingSemantic::Normal: {
    const auto [descriptor_id, texture] = normalize_pbr_texture_binding(
        semantic, material_data.normal_descriptor_id, material_data.normal
    );
    return {.descriptor_id = descriptor_id, .texture = texture, .channel = 0};
  }
  case MaterialBindingSemantic::Metallic:
    if (!material_data.metallic_descriptor_id.empty() &&
        material_data.metallic != nullptr) {
      return {
          .descriptor_id = material_data.metallic_descriptor_id,
          .texture = material_data.metallic,
          .channel = 0,
      };
    }
    if (!material_data.metallic_roughness_descriptor_id.empty() &&
        material_data.metallic_roughness != nullptr) {
      return {
          .descriptor_id = material_data.metallic_roughness_descriptor_id,
          .texture = material_data.metallic_roughness,
          .channel = 2,
      };
    }
    return {
        .descriptor_id = default_pbr_metallic_texture_id(),
        .texture = default_texture_for_semantic(semantic),
        .channel = 0,
    };
  case MaterialBindingSemantic::Roughness:
    if (!material_data.roughness_descriptor_id.empty() &&
        material_data.roughness != nullptr) {
      return {
          .descriptor_id = material_data.roughness_descriptor_id,
          .texture = material_data.roughness,
          .channel = 0,
      };
    }
    if (!material_data.metallic_roughness_descriptor_id.empty() &&
        material_data.metallic_roughness != nullptr) {
      return {
          .descriptor_id = material_data.metallic_roughness_descriptor_id,
          .texture = material_data.metallic_roughness,
          .channel = 1,
      };
    }
    return {
        .descriptor_id = default_pbr_roughness_texture_id(),
        .texture = default_texture_for_semantic(semantic),
        .channel = 0,
    };
  case MaterialBindingSemantic::Occlusion: {
    const auto [descriptor_id, texture] = normalize_pbr_texture_binding(
        semantic, material_data.occlusion_descriptor_id, material_data.occlusion
    );
    return {.descriptor_id = descriptor_id, .texture = texture, .channel = 0};
  }
  case MaterialBindingSemantic::Emissive: {
    const auto [descriptor_id, texture] = normalize_pbr_texture_binding(
        semantic, material_data.emissive_descriptor_id, material_data.emissive
    );
    return {.descriptor_id = descriptor_id, .texture = texture, .channel = 0};
  }
  case MaterialBindingSemantic::Displacement: {
    const auto [descriptor_id, texture] = normalize_pbr_texture_binding(
        semantic, material_data.displacement_descriptor_id,
        material_data.displacement
    );
    return {.descriptor_id = descriptor_id, .texture = texture, .channel = 0};
  }
  }

  return {};
}

inline ResolvedMaterialData
resolve_material_data(const Model *model, const MaterialSlots *material_slots,
                      const TextureBindings *texture_bindings,
                      uint32_t submesh_index = 0u) {
  ResolvedMaterialData material_data;

  if (const auto *material_id =
          resolve_material_id_for_submesh(model, material_slots, submesh_index);
      material_id != nullptr) {
    material_data.material_id = *material_id;

    auto manager = resource_manager();
    auto material =
        manager != nullptr
            ? manager->get_by_descriptor_id<MaterialDescriptor>(
                  material_data.material_id)
            : nullptr;

    if (material != nullptr) {
      material_data.base_color_factor = material->base_color_factor;
      material_data.emissive_factor = material->emissive_factor;
      material_data.metallic_factor = material->metallic_factor;
      material_data.roughness_factor = material->roughness_factor;
      material_data.occlusion_strength = material->occlusion_strength;
      material_data.normal_scale = material->normal_scale;
      material_data.height_scale = material->height_scale;
      material_data.bloom_intensity = material->bloom_intensity;
      material_data.alpha_mask = material->alpha_mask;
      material_data.alpha_blend = material->alpha_blend;
      material_data.alpha_cutoff = material->alpha_cutoff;
      material_data.double_sided = material->double_sided;

      if (material->base_color_id.has_value()) {
        material_data.base_color_descriptor_id = *material->base_color_id;
        material_data.base_color =
            manager->get_by_descriptor_id<Texture2D>(*material->base_color_id);
      }

      if (material->normal_id.has_value()) {
        material_data.normal_descriptor_id = *material->normal_id;
        material_data.normal =
            manager->get_by_descriptor_id<Texture2D>(*material->normal_id);
      }

      if (material->metallic_id.has_value()) {
        material_data.metallic_descriptor_id = *material->metallic_id;
        material_data.metallic =
            manager->get_by_descriptor_id<Texture2D>(*material->metallic_id);
      }

      if (material->roughness_id.has_value()) {
        material_data.roughness_descriptor_id = *material->roughness_id;
        material_data.roughness =
            manager->get_by_descriptor_id<Texture2D>(*material->roughness_id);
      }

      if (material->metallic_roughness_id.has_value()) {
        material_data.metallic_roughness_descriptor_id =
            *material->metallic_roughness_id;
        material_data.metallic_roughness =
            manager->get_by_descriptor_id<Texture2D>(
                *material->metallic_roughness_id);
      }

      if (material->occlusion_id.has_value()) {
        material_data.occlusion_descriptor_id = *material->occlusion_id;
        material_data.occlusion =
            manager->get_by_descriptor_id<Texture2D>(*material->occlusion_id);
      }

      if (material->emissive_id.has_value()) {
        material_data.emissive_descriptor_id = *material->emissive_id;
        material_data.emissive =
            manager->get_by_descriptor_id<Texture2D>(*material->emissive_id);
      }

      if (material->displacement_id.has_value()) {
        material_data.displacement_descriptor_id = *material->displacement_id;
        material_data.displacement =
            manager->get_by_descriptor_id<Texture2D>(*material->displacement_id);
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
    const ResolvedMaterialData &material_data) {
  MaterialGroupKey key;
  key.material_id = material_data.material_id;

  const auto [base_color_id, base_color_texture] = normalize_pbr_texture_binding(
      MaterialBindingSemantic::BaseColor, material_data.base_color_descriptor_id,
      material_data.base_color);
  const auto [normal_id, normal_texture] = normalize_pbr_texture_binding(
      MaterialBindingSemantic::Normal, material_data.normal_descriptor_id,
      material_data.normal);
  const auto metallic = resolve_surface_texture_binding(
      material_data, MaterialBindingSemantic::Metallic
  );
  const auto roughness = resolve_surface_texture_binding(
      material_data, MaterialBindingSemantic::Roughness
  );
  const auto [occlusion_id, occlusion_texture] = normalize_pbr_texture_binding(
      MaterialBindingSemantic::Occlusion, material_data.occlusion_descriptor_id,
      material_data.occlusion);
  const auto [emissive_id, emissive_texture] = normalize_pbr_texture_binding(
      MaterialBindingSemantic::Emissive, material_data.emissive_descriptor_id,
      material_data.emissive);
  const auto [displacement_id, displacement_texture] =
      normalize_pbr_texture_binding(MaterialBindingSemantic::Displacement,
                                    material_data.displacement_descriptor_id,
                                    material_data.displacement);

  key.base_color = make_texture_binding_identity(base_color_id, base_color_texture);
  key.normal = make_texture_binding_identity(normal_id, normal_texture);
  key.metallic =
      make_texture_binding_identity(metallic.descriptor_id, metallic.texture);
  key.roughness =
      make_texture_binding_identity(roughness.descriptor_id, roughness.texture);
  key.occlusion = make_texture_binding_identity(occlusion_id, occlusion_texture);
  key.emissive = make_texture_binding_identity(emissive_id, emissive_texture);
  key.displacement =
      make_texture_binding_identity(displacement_id, displacement_texture);
  key.metallic_channel = metallic.channel;
  key.roughness_channel = roughness.channel;
  key.base_color_factor = {
      material_data.base_color_factor.x,
      material_data.base_color_factor.y,
      material_data.base_color_factor.z,
      material_data.base_color_factor.w,
  };
  key.emissive_factor = {
      material_data.emissive_factor.x,
      material_data.emissive_factor.y,
      material_data.emissive_factor.z,
  };
  key.metallic_factor = material_data.metallic_factor;
  key.roughness_factor = material_data.roughness_factor;
  key.occlusion_strength = material_data.occlusion_strength;
  key.normal_scale = material_data.normal_scale;
  key.height_scale = material_data.height_scale;
  key.bloom_intensity = material_data.bloom_intensity;
  key.alpha_mask_enabled = material_data.alpha_mask ? 1 : 0;
  key.alpha_cutoff = material_data.alpha_cutoff;

  key.extra_textures.reserve(material_data.extra_textures.size());
  for (const auto &binding : material_data.extra_textures) {
    key.extra_textures.push_back(make_texture_binding_identity(
        binding.descriptor_id, binding.texture, binding.cubemap, binding.name));
  }

  return key;
}

inline int record_resident_texture_binding(
    CompiledFrame &frame, RenderBindingGroupHandle binding_group,
    const Ref<Texture> &texture, bool cubemap, int slot,
    const std::string &logical_name, const std::string &debug_name,
    uint64_t binding_id) {
  if (texture == nullptr || slot < 0) {
    return -1;
  }

  ASTRA_ENSURE(binding_id == 0, "Cannot record material binding '",
               logical_name, "' with binding id 0");

  const auto image =
      cubemap ? frame.register_texture_cube(debug_name, texture)
              : frame.register_texture_2d(debug_name, texture);

  frame.add_sampled_image_binding(
      binding_group, binding_id, ImageViewRef{.image = image},
      cubemap ? CompiledSampledImageTarget::TextureCube
              : CompiledSampledImageTarget::Texture2D);
  return slot;
}

inline void update_material_binding_slot(MaterialBindingState &state,
                                         MaterialBindingSemantic semantic,
                                         int slot) {
  switch (semantic) {
  case MaterialBindingSemantic::BaseColor:
    if (state.base_color_slot < 0) {
      state.base_color_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Normal:
    if (state.normal_slot < 0) {
      state.normal_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Metallic:
    if (state.metallic_slot < 0) {
      state.metallic_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Roughness:
    if (state.roughness_slot < 0) {
      state.roughness_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Occlusion:
    if (state.occlusion_slot < 0) {
      state.occlusion_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Emissive:
    if (state.emissive_slot < 0) {
      state.emissive_slot = slot;
    }
    return;
  case MaterialBindingSemantic::Displacement:
    if (state.displacement_slot < 0) {
      state.displacement_slot = slot;
    }
    return;
  }
}

inline const MaterialTextureBindingPoint *resolve_binding_point(
    const MaterialBindingLayout &layout, MaterialBindingSemantic semantic
) {
  switch (semantic) {
  case MaterialBindingSemantic::BaseColor:
    return layout.base_color.has_value() ? &*layout.base_color : nullptr;
  case MaterialBindingSemantic::Normal:
    return layout.normal.has_value() ? &*layout.normal : nullptr;
  case MaterialBindingSemantic::Metallic:
    return layout.metallic.has_value() ? &*layout.metallic : nullptr;
  case MaterialBindingSemantic::Roughness:
    return layout.roughness.has_value() ? &*layout.roughness : nullptr;
  case MaterialBindingSemantic::Occlusion:
    return layout.occlusion.has_value() ? &*layout.occlusion : nullptr;
  case MaterialBindingSemantic::Emissive:
    return layout.emissive.has_value() ? &*layout.emissive : nullptr;
  case MaterialBindingSemantic::Displacement:
    return layout.displacement.has_value() ? &*layout.displacement : nullptr;
  }

  return nullptr;
}

inline std::optional<MaterialBindingSemantic> resolve_material_binding_semantic(
    const MaterialBindingLayout &layout, std::string_view name) {
  if (layout.base_color.has_value() && name == layout.base_color->logical_name) {
    return MaterialBindingSemantic::BaseColor;
  }
  if (layout.normal.has_value() && name == layout.normal->logical_name) {
    return MaterialBindingSemantic::Normal;
  }
  if (layout.metallic.has_value() && name == layout.metallic->logical_name) {
    return MaterialBindingSemantic::Metallic;
  }
  if (layout.roughness.has_value() && name == layout.roughness->logical_name) {
    return MaterialBindingSemantic::Roughness;
  }
  if (layout.occlusion.has_value() && name == layout.occlusion->logical_name) {
    return MaterialBindingSemantic::Occlusion;
  }
  if (layout.emissive.has_value() && name == layout.emissive->logical_name) {
    return MaterialBindingSemantic::Emissive;
  }
  if (layout.displacement.has_value() && name == layout.displacement->logical_name) {
    return MaterialBindingSemantic::Displacement;
  }

  return std::nullopt;
}

inline MaterialBindingState record_resolved_material_bindings(
    CompiledFrame &frame, RenderBindingGroupHandle binding_group,
    const ResolvedMaterialData &material_data, const MaterialBindingLayout &layout,
    int starting_slot = 0) {
  MaterialBindingState state;
  state.base_color_factor = material_data.base_color_factor;
  state.emissive_factor = material_data.emissive_factor;
  state.metallic_factor = material_data.metallic_factor;
  state.roughness_factor = material_data.roughness_factor;
  state.occlusion_strength = material_data.occlusion_strength;
  state.normal_scale = material_data.normal_scale;
  state.height_scale = material_data.height_scale;
  state.bloom_intensity = material_data.bloom_intensity;
  state.alpha_mask_enabled = material_data.alpha_mask ? 1 : 0;
  state.alpha_cutoff = material_data.alpha_cutoff;
  state.next_texture_slot = starting_slot;

  const auto record_pbr_binding =
      [&](MaterialBindingSemantic semantic,
          const std::optional<MaterialTextureBindingPoint> &binding_point,
          const ResourceDescriptorID &descriptor_id, const Ref<Texture> &texture,
          const std::string &debug_name) -> int {
    if (!binding_point.has_value()) {
      return -1;
    }

    const auto [resolved_id, resolved_texture] =
        normalize_pbr_texture_binding(semantic, descriptor_id, texture);
    (void)resolved_id;
    return record_resident_texture_binding(
        frame, binding_group, resolved_texture, false, state.next_texture_slot++,
        std::string(binding_point->logical_name), debug_name,
        binding_point->binding_id);
  };

  const auto metallic = resolve_surface_texture_binding(
      material_data, MaterialBindingSemantic::Metallic
  );
  const auto roughness = resolve_surface_texture_binding(
      material_data, MaterialBindingSemantic::Roughness
  );

  state.base_color_slot = record_pbr_binding(
      MaterialBindingSemantic::BaseColor, layout.base_color,
      material_data.base_color_descriptor_id, material_data.base_color,
      "material.base_color");
  state.normal_slot = record_pbr_binding(
      MaterialBindingSemantic::Normal, layout.normal,
      material_data.normal_descriptor_id, material_data.normal, "material.normal");
  state.metallic_slot = record_pbr_binding(
      MaterialBindingSemantic::Metallic, layout.metallic,
      metallic.descriptor_id, metallic.texture, "material.metallic");
  state.roughness_slot = record_pbr_binding(
      MaterialBindingSemantic::Roughness, layout.roughness,
      roughness.descriptor_id, roughness.texture, "material.roughness");
  state.occlusion_slot = record_pbr_binding(
      MaterialBindingSemantic::Occlusion, layout.occlusion,
      material_data.occlusion_descriptor_id, material_data.occlusion,
      "material.occlusion");
  state.emissive_slot = record_pbr_binding(
      MaterialBindingSemantic::Emissive, layout.emissive,
      material_data.emissive_descriptor_id, material_data.emissive,
      "material.emissive");
  state.displacement_slot = record_pbr_binding(
      MaterialBindingSemantic::Displacement, layout.displacement,
      material_data.displacement_descriptor_id, material_data.displacement,
      "material.displacement");
  state.metallic_channel = metallic.channel;
  state.roughness_channel = roughness.channel;

  for (const auto &binding : material_data.extra_textures) {
    const auto semantic =
        resolve_material_binding_semantic(layout, binding.name);
    if (!semantic.has_value()) {
      continue;
    }

    const auto *binding_point = resolve_binding_point(layout, *semantic);
    if (binding_point == nullptr) {
      continue;
    }

    const int slot = record_resident_texture_binding(
        frame, binding_group, binding.texture, binding.cubemap, state.next_texture_slot,
        std::string(binding_point->logical_name),
        "material.extra." + binding.name, binding_point->binding_id);
    if (slot < 0) {
      continue;
    }

    state.next_texture_slot = slot + 1;
    if (*semantic == MaterialBindingSemantic::Metallic) {
      state.metallic_channel = 0;
    } else if (*semantic == MaterialBindingSemantic::Roughness) {
      state.roughness_channel = 0;
    }
    update_material_binding_slot(state, *semantic, slot);
  }

  return state;
}

} // namespace astralix::rendering
