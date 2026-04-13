#pragma once

#include "components/material.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/svg-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/font.hpp"
#include "targets/render-target.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace astralix::rendering {

struct SceneResidencyRequests {
  std::unordered_set<ResourceDescriptorID> shaders;
  std::unordered_set<ResourceDescriptorID> models;
  std::unordered_set<ResourceDescriptorID> materials;
  std::unordered_set<ResourceDescriptorID> textures_2d;
  std::unordered_set<ResourceDescriptorID> textures_3d;
  std::unordered_set<ResourceDescriptorID> fonts;
  std::unordered_set<ResourceDescriptorID> svgs;
  std::unordered_map<ResourceDescriptorID, std::unordered_set<uint32_t>>
      font_sizes;
};

inline void request_shader(SceneResidencyRequests &requests,
                           const ResourceDescriptorID &shader_id) {
  if (!shader_id.empty()) {
    requests.shaders.insert(shader_id);
  }
}

inline void request_model(SceneResidencyRequests &requests,
                          const ResourceDescriptorID &model_id) {
  if (!model_id.empty()) {
    requests.models.insert(model_id);
  }
}

inline void request_material(SceneResidencyRequests &requests,
                             const ResourceDescriptorID &material_id) {
  if (!material_id.empty()) {
    requests.materials.insert(material_id);
  }
}

inline void request_texture(SceneResidencyRequests &requests,
                            const ResourceDescriptorID &texture_id,
                            bool cubemap = false) {
  if (texture_id.empty()) {
    return;
  }

  if (cubemap) {
    requests.textures_3d.insert(texture_id);
  } else {
    requests.textures_2d.insert(texture_id);
  }
}

inline void request_font_size(SceneResidencyRequests &requests,
                              const ResourceDescriptorID &font_id,
                              uint32_t pixel_size) {
  if (font_id.empty()) {
    return;
  }

  requests.fonts.insert(font_id);
  requests.font_sizes[font_id].insert(std::max(1u, pixel_size));
}

inline void request_svg(SceneResidencyRequests &requests,
                        const ResourceDescriptorID &svg_id) {
  if (!svg_id.empty()) {
    requests.svgs.insert(svg_id);
  }
}

inline void request_texture_bindings(SceneResidencyRequests &requests,
                                     const TextureBindings *texture_bindings) {
  if (texture_bindings == nullptr) {
    return;
  }

  for (const auto &binding : texture_bindings->bindings) {
    request_texture(requests, binding.id, binding.cubemap);
  }
}

inline void request_material_descriptor_textures(
    SceneResidencyRequests &requests,
    const ResourceDescriptorID &material_id) {
  request_material(requests, material_id);

  auto material =
      resource_manager()->get_by_descriptor_id<MaterialDescriptor>(material_id);
  if (material == nullptr) {
    return;
  }

  for (const auto &diffuse_id : material->diffuse_ids) {
    request_texture(requests, diffuse_id, false);
  }

  for (const auto &specular_id : material->specular_ids) {
    request_texture(requests, specular_id, false);
  }

  if (material->normal_map_ids.has_value()) {
    request_texture(requests, *material->normal_map_ids, false);
  }

  if (material->displacement_map_ids.has_value()) {
    request_texture(requests, *material->displacement_map_ids, false);
  }
}

inline void request_ui_command_resources(SceneResidencyRequests &requests,
                                         const ui::UIDrawCommand &command) {
  switch (command.type) {
    case ui::DrawCommandType::Image:
      request_texture(requests, command.texture_id, false);
      return;

    case ui::DrawCommandType::SvgImage:
      request_svg(requests, command.texture_id);
      return;

    case ui::DrawCommandType::Text:
      request_font_size(
          requests, command.font_id,
          static_cast<uint32_t>(
              std::max(1.0f, std::round(command.font_size))));
      return;

    case ui::DrawCommandType::Rect:
    case ui::DrawCommandType::RenderImageView:
    case ui::DrawCommandType::Polyline:
      return;
  }
}

template <typename Descriptor>
inline void load_descriptor_ids(
    RendererBackend backend,
    const std::unordered_set<ResourceDescriptorID> &descriptor_ids) {
  for (const auto &descriptor_id : descriptor_ids) {
    resource_manager()->load_from_descriptors_by_ids<Descriptor>(
        backend, {descriptor_id});
  }
}

inline void resolve_scene_residency(const SceneResidencyRequests &requests,
                                    Ref<RenderTarget> render_target) {
  if (render_target == nullptr) {
    return;
  }

  const auto backend = render_target->backend();
  load_descriptor_ids<ShaderDescriptor>(backend, requests.shaders);
  load_descriptor_ids<ModelDescriptor>(backend, requests.models);
  load_descriptor_ids<MaterialDescriptor>(backend, requests.materials);
  load_descriptor_ids<Texture2DDescriptor>(backend, requests.textures_2d);
  load_descriptor_ids<Texture3DDescriptor>(backend, requests.textures_3d);
  load_descriptor_ids<FontDescriptor>(backend, requests.fonts);
  load_descriptor_ids<SvgDescriptor>(backend, requests.svgs);
}

inline void prepare_requested_font_glyphs(
    const SceneResidencyRequests &requests) {
  for (const auto &[font_id, sizes] : requests.font_sizes) {
    auto font = resource_manager()->get_by_descriptor_id<Font>(font_id);
    if (font == nullptr) {
      continue;
    }

    for (uint32_t size : sizes) {
      (void)font->glyphs(size);
    }
  }
}

} // namespace astralix::rendering
