#pragma once

#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/model.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "frustum-culling.hpp"
#include "light-frame.hpp"
#include "material-binding.hpp"
#include "mesh-resolution.hpp"
#include "render-frame.hpp"
#include "render-residency.hpp"
#include "scene-selection.hpp"
#include "targets/render-target.hpp"
#include <algorithm>
#include <optional>
#include <unordered_map>

namespace astralix::rendering {

inline const Model *resolve_primary_model(const ModelRef *model_ref) {
  if (model_ref == nullptr) {
    return nullptr;
  }

  const Model *first_model = nullptr;
  for (const auto &resource_id : model_ref->resource_ids) {
    auto model = resource_manager()->get_by_descriptor_id<Model>(resource_id);
    if (model == nullptr) {
      continue;
    }

    if (first_model == nullptr) {
      first_model = model.get();
    }

    if (!model->materials.empty()) {
      return model.get();
    }
  }

  return first_model;
}

inline void gather_initial_scene_residency_requests(
    ecs::World &world, const std::optional<SkyboxFrame> &skybox,
    const std::vector<TextDrawItem> &text_items,
    const std::vector<UIRootDrawList> &ui_roots,
    SceneResidencyRequests &requests
) {
  if (skybox.has_value()) {
    request_shader(requests, skybox->shader_id);
    request_texture(requests, skybox->cubemap_id, true);
  }

  for (const auto &text_item : text_items) {
    request_font_size(requests, text_item.sprite.font_id, text_item.glyph_pixel_size);
  }

  for (const auto &ui_root : ui_roots) {
    for (const auto &command : ui_root.commands) {
      request_ui_command_resources(requests, command);
    }
  }

  world.each<Renderable, scene::Transform, ShaderBinding>(
      [&](EntityID entity_id, Renderable &, scene::Transform &, ShaderBinding &shader) {
        if (!world.active(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *model_ref = entity.get<ModelRef>();
        auto *mesh_set = entity.get<MeshSet>();
        if (model_ref == nullptr && mesh_set == nullptr) {
          return;
        }

        request_shader(requests, shader.shader);

        if (model_ref != nullptr) {
          for (const auto &resource_id : model_ref->resource_ids) {
            request_model(requests, resource_id);
          }
        }

        if (auto *materials = entity.get<MaterialSlots>();
            materials != nullptr) {
          for (const auto &material_id : materials->materials) {
            request_material(requests, material_id);
          }
        }

        request_texture_bindings(requests, entity.get<TextureBindings>());
      }
  );
}

inline void gather_material_residency_requests(ecs::World &world, SceneResidencyRequests &requests) {
  world.each<Renderable, scene::Transform, ShaderBinding>(
      [&](EntityID entity_id, Renderable &, scene::Transform &, ShaderBinding &) {
        if (!world.active(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *model_ref = entity.get<ModelRef>();
        auto *mesh_set = entity.get<MeshSet>();
        if (model_ref == nullptr && mesh_set == nullptr) {
          return;
        }

        if (model_ref != nullptr) {
          for (const auto &resource_id : model_ref->resource_ids) {
            auto descriptor =
                resource_manager()->get_descriptor_by_id<ModelDescriptor>(
                    resource_id
                );
            if (descriptor == nullptr) {
              continue;
            }

            for (const auto &material_id : descriptor->material_ids) {
              request_material_descriptor_textures(requests, material_id);
            }
          }
        }

        const Model *model = resolve_primary_model(model_ref);
        MaterialSlots fallback_slots;
        const auto *material_slots = resolve_material_slots(
            model, entity.get<MaterialSlots>(), fallback_slots
        );
        if (material_slots == nullptr) {
          return;
        }

        for (const auto &material_id : material_slots->materials) {
          request_material_descriptor_textures(requests, material_id);
        }
      }
  );
}

inline uint32_t ensure_pick_id(
    EntityID entity_id, std::vector<EntityID> &pick_id_lut,
    std::unordered_map<EntityID, uint32_t> &pick_ids_by_entity
) {
  auto [it, inserted] =
      pick_ids_by_entity.emplace(entity_id, static_cast<uint32_t>(0));
  if (inserted) {
    pick_id_lut.push_back(entity_id);
    it->second = static_cast<uint32_t>(pick_id_lut.size());
  }

  return it->second;
}

inline void resolve_ui_resources(SceneFrame &frame) {
  for (const auto &text_item : frame.text_items) {
    if (text_item.font == nullptr) {
      continue;
    }

    const auto &glyphs = text_item.font->glyphs(text_item.glyph_pixel_size);
    for (const auto &glyph : glyphs) {
      if (glyph.texture_id.empty()) {
        continue;
      }

      frame.ui_resources.textures.try_emplace(
          glyph.texture_id,
          resource_manager()->get_by_descriptor_id<Texture2D>(
              glyph.texture_id
          )
      );
    }
  }

  for (const auto &ui_root : frame.ui_roots) {
    for (const auto &command : ui_root.commands) {
      switch (command.type) {
        case ui::DrawCommandType::Image:
          if (!command.texture_id.empty()) {
            frame.ui_resources.textures.try_emplace(
                command.texture_id,
                resource_manager()->get_by_descriptor_id<Texture2D>(
                    command.texture_id
                )
            );
          }
          break;

        case ui::DrawCommandType::SvgImage:
          if (!command.texture_id.empty()) {
            frame.ui_resources.svgs.try_emplace(
                command.texture_id,
                resource_manager()->get_by_descriptor_id<Svg>(
                    command.texture_id
                )
            );
          }
          break;

        case ui::DrawCommandType::Text: {
          if (command.font_id.empty()) {
            break;
          }

          auto [font_iterator, inserted] =
              frame.ui_resources.fonts.try_emplace(
                  command.font_id,
                  resource_manager()->get_by_descriptor_id<Font>(
                      command.font_id
                  )
              );

          if (font_iterator->second != nullptr) {
            const uint32_t font_size = static_cast<uint32_t>(
                std::max(1.0f, std::round(command.font_size))
            );
            const auto &glyphs = font_iterator->second->glyphs(font_size);
            for (const auto &glyph : glyphs) {
              if (glyph.texture_id.empty()) {
                continue;
              }

              frame.ui_resources.textures.try_emplace(
                  glyph.texture_id,
                  resource_manager()->get_by_descriptor_id<Texture2D>(
                      glyph.texture_id
                  )
              );
            }
          }
          break;
        }

        case ui::DrawCommandType::Rect:
        case ui::DrawCommandType::RenderImageView:
        case ui::DrawCommandType::Polyline:
        case ui::DrawCommandType::Path:
          break;

        default:
          break;
      }
    }
  }
}

inline SceneFrame build_scene_frame(
    ecs::World &world,
    Ref<RenderTarget> render_target,
    RenderRuntimeStore &render_runtime_store,
    const CameraHistoryState &camera_history = {}
) {
  render_runtime_store.prune(world);

  SceneFrame frame;
  frame.main_camera = extract_main_camera_frame(world);
  if (frame.main_camera.has_value()) {
    if (camera_history.valid &&
        camera_history.entity_id == frame.main_camera->entity_id) {
      frame.main_camera->previous_view = camera_history.previous_view;
      frame.main_camera->previous_projection =
          camera_history.previous_projection;
      frame.main_camera->has_history = true;
    } else {
      frame.main_camera->previous_view = frame.main_camera->view;
      frame.main_camera->previous_projection =
          frame.main_camera->projection;
      frame.main_camera->has_history = false;
    }
  }
  frame.light_frame = collect_light_frame(world);
  if (frame.main_camera.has_value() && frame.light_frame.directional.valid) {
    compute_cascades(frame.light_frame.directional, *frame.main_camera);
  }
  frame.skybox = extract_skybox_frame(world);
  frame.text_items = extract_text_items(world);
  frame.ui_roots = extract_ui_roots(world);

  if (render_target == nullptr) {
    return frame;
  }

  SceneResidencyRequests initial_requests;
  gather_initial_scene_residency_requests(world, frame.skybox, frame.text_items, frame.ui_roots, initial_requests);
  resolve_scene_residency_async(initial_requests, render_target);

  SceneResidencyRequests material_requests;
  gather_material_residency_requests(world, material_requests);
  resolve_scene_residency_async(material_requests, render_target);

  prepare_requested_font_glyphs(initial_requests);

  if (frame.skybox.has_value()) {
    frame.skybox->shader =
        resource_manager()->get_by_descriptor_id<Shader>(frame.skybox->shader_id);
    frame.skybox->cubemap =
        resource_manager()->get_by_descriptor_id<Texture3D>(
            frame.skybox->cubemap_id
        );
  }

  for (auto &text_item : frame.text_items) {
    text_item.font =
        resource_manager()->get_by_descriptor_id<Font>(text_item.sprite.font_id);
  }

  resolve_ui_resources(frame);

  std::optional<Frustum> camera_frustum;
  if (frame.main_camera.has_value()) {
    camera_frustum = extract_frustum(
        frame.main_camera->projection * frame.main_camera->view
    );
  }

  std::unordered_map<EntityID, uint32_t> pick_ids_by_entity;
  const bool use_shadow_caster_tags = world.count<ShadowCaster>() > 0u;

  world.each<Renderable, scene::Transform, ShaderBinding>(
      [&](EntityID entity_id, Renderable &, scene::Transform &transform, ShaderBinding &shader_binding) {
        if (!world.active(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *model_ref = entity.get<ModelRef>();
        auto *mesh_set = entity.get<MeshSet>();
        if (model_ref == nullptr && mesh_set == nullptr) {
          return;
        }

        auto resolved_meshes =
            prepare_render_meshes(model_ref, mesh_set, render_target);
        if (resolved_meshes.empty()) {
          return;
        }

        const uint32_t pick_id = ensure_pick_id(
            entity_id, frame.pick_id_lut, pick_ids_by_entity
        );
        const bool casts_shadow =
            !use_shadow_caster_tags || entity.get<ShadowCaster>() != nullptr;

        const Model *model = resolve_primary_model(model_ref);
        const auto bloom_settings =
            resolve_bloom_settings(entity.get<BloomSettings>());
        const auto shader =
            resource_manager()->get_by_descriptor_id<Shader>(
                shader_binding.shader
            );

        for (const auto &mesh : resolved_meshes) {
          const bool visible_to_camera =
              !camera_frustum.has_value() ||
              is_aabb_visible(*camera_frustum, mesh.local_bounds, transform.matrix);

          if (casts_shadow) {
            frame.shadow_draws.push_back(ShadowDrawItem{
                .entity_id = entity_id,
                .model = transform.matrix,
                .mesh = mesh,
                .sort_key = 0,
            });
          }

          if (!visible_to_camera) {
            continue;
          }

          const auto material = resolve_material_data(
              model,
              entity.get<MaterialSlots>(),
              entity.get<TextureBindings>(),
              mesh.submesh_index
          );
          const uint64_t sort_key = compute_surface_sort_key(
              shader_binding.shader, material.material_id, mesh.mesh_id
          );
          auto &runtime_state = render_runtime_store.entity_states[entity_id];
          const bool has_previous_model = runtime_state.has_previous_model;
          const glm::mat4 previous_model =
              has_previous_model ? runtime_state.previous_model
                                 : transform.matrix;
          runtime_state.surface_sort_key = sort_key;
          runtime_state.initialized = true;

          auto &target_list = material.alpha_blend
                                   ? frame.blend_surfaces
                                   : frame.opaque_surfaces;
          target_list.push_back(SurfaceDrawItem{
              .entity_id = entity_id,
              .pick_id = pick_id,
              .shader_id = shader_binding.shader,
              .shader = shader,
              .material = material,
              .mesh = mesh,
              .model = transform.matrix,
              .previous_model = previous_model,
              .has_previous_model = has_previous_model,
              .bloom_enabled = bloom_settings.enabled,
              .bloom_layer = bloom_settings.render_layer,
              .casts_shadow = casts_shadow,
              .sort_key = sort_key,
          });
        }
      }
  );

  std::stable_sort(
      frame.opaque_surfaces.begin(), frame.opaque_surfaces.end(), [](const SurfaceDrawItem &lhs, const SurfaceDrawItem &rhs) {
        return lhs.sort_key < rhs.sort_key;
      }
  );
  std::stable_sort(
      frame.shadow_draws.begin(), frame.shadow_draws.end(), [](const ShadowDrawItem &lhs, const ShadowDrawItem &rhs) {
        return lhs.sort_key < rhs.sort_key;
      }
  );

  return frame;
}

} // namespace astralix::rendering
