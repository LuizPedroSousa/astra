#pragma once

#include "components/camera.hpp"
#include "components/light.hpp"
#include "components/transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "material-binding.hpp"
#include "render-frame.hpp"
#include "scene-selection.hpp"
#include "world.hpp"
#include <cmath>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix::rendering {

inline glm::vec3 light_term(const Light &light, const glm::vec3 &base) {
  return base * light.color * light.intensity;
}

inline glm::vec3
directional_light_direction(const scene::Transform &transform) {
  const glm::vec3 forward =
      glm::mat3_cast(transform.rotation) * glm::vec3(0.0f, -1.0f, 0.0f);
  if (glm::dot(forward, forward) <= 1.0e-6f) {
    return glm::vec3(0.0f, -1.0f, 0.0f);
  }
  return glm::normalize(forward);
}

inline glm::mat4
build_directional_light_space_matrix(const scene::Transform &transform, const DirectionalShadowSettings &shadow) {
  const glm::vec3 light_direction = directional_light_direction(transform);
  const glm::mat4 projection = glm::ortho(
      -shadow.ortho_extent, shadow.ortho_extent, -shadow.ortho_extent, shadow.ortho_extent, shadow.near_plane, shadow.far_plane
  );
  glm::vec3 up_hint = glm::vec3(0.0f, 1.0f, 0.0f);
  if (std::abs(glm::dot(light_direction, up_hint)) > 0.99f) {
    up_hint = glm::vec3(0.0f, 0.0f, 1.0f);
  }
  const glm::mat4 view = glm::lookAt(
      transform.position, transform.position + light_direction, up_hint
  );

  return projection * view;
}

inline DirectionalLightPacket make_fallback_directional_light() {
  const scene::Transform transform{.position = glm::vec3(-4.0f, 8.0f, -3.0f)};
  const Light light{};
  const DirectionalShadowSettings shadow{};

  return DirectionalLightPacket{
      .valid = false,
      .position = transform.position,
      .direction = glm::normalize(glm::vec3(0.0f) - transform.position),
      .ambient = light_term(light, glm::vec3(light.ambient_strength)),
      .diffuse = light_term(light, glm::vec3(light.diffuse_strength)),
      .specular = light_term(light, glm::vec3(light.specular_strength)),
      .light_space_matrix =
          build_directional_light_space_matrix(transform, shadow),
      .near_plane = shadow.near_plane,
      .far_plane = shadow.far_plane,
  };
}

inline std::array<float, k_shadow_cascade_count + 1> compute_cascade_splits(
    float camera_near, float camera_far, float lambda = 0.75f
) {
  std::array<float, k_shadow_cascade_count + 1> splits{};
  splits[0] = camera_near;

  for (size_t i = 1; i <= k_shadow_cascade_count; ++i) {
    float fraction = static_cast<float>(i) / static_cast<float>(k_shadow_cascade_count);
    float log_split = camera_near * std::pow(camera_far / camera_near, fraction);
    float linear_split = camera_near + (camera_far - camera_near) * fraction;
    splits[i] = lambda * log_split + (1.0f - lambda) * linear_split;
  }

  return splits;
}

inline glm::mat4 build_cascade_matrix(
    const glm::vec3 &light_direction,
    const glm::mat4 &inverse_view_projection,
    float texel_snap_world_size = 0.0f
) {
  glm::vec3 frustum_corners[8] = {
      glm::vec3(-1.0f,  1.0f, -1.0f),
      glm::vec3( 1.0f,  1.0f, -1.0f),
      glm::vec3( 1.0f, -1.0f, -1.0f),
      glm::vec3(-1.0f, -1.0f, -1.0f),
      glm::vec3(-1.0f,  1.0f,  1.0f),
      glm::vec3( 1.0f,  1.0f,  1.0f),
      glm::vec3( 1.0f, -1.0f,  1.0f),
      glm::vec3(-1.0f, -1.0f,  1.0f),
  };

  for (auto &corner : frustum_corners) {
    glm::vec4 world_corner = inverse_view_projection * glm::vec4(corner, 1.0f);
    corner = glm::vec3(world_corner) / world_corner.w;
  }

  glm::vec3 center(0.0f);
  for (const auto &corner : frustum_corners) {
    center += corner;
  }
  center /= 8.0f;

  glm::vec3 up_hint = glm::vec3(0.0f, 1.0f, 0.0f);
  if (std::abs(glm::dot(light_direction, up_hint)) > 0.99f) {
    up_hint = glm::vec3(0.0f, 0.0f, 1.0f);
  }
  glm::mat4 light_view = glm::lookAt(center - light_direction, center, up_hint);

  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  for (const auto &corner : frustum_corners) {
    glm::vec3 light_space_corner = glm::vec3(light_view * glm::vec4(corner, 1.0f));
    min_x = std::min(min_x, light_space_corner.x);
    max_x = std::max(max_x, light_space_corner.x);
    min_y = std::min(min_y, light_space_corner.y);
    max_y = std::max(max_y, light_space_corner.y);
    min_z = std::min(min_z, light_space_corner.z);
    max_z = std::max(max_z, light_space_corner.z);
  }

  float z_padding = (max_z - min_z) * 5.0f;
  min_z -= z_padding;
  max_z += z_padding;

  if (texel_snap_world_size > 0.0f) {
    min_x = std::floor(min_x / texel_snap_world_size) * texel_snap_world_size;
    max_x = std::ceil(max_x / texel_snap_world_size) * texel_snap_world_size;
    min_y = std::floor(min_y / texel_snap_world_size) * texel_snap_world_size;
    max_y = std::ceil(max_y / texel_snap_world_size) * texel_snap_world_size;
  }

  glm::mat4 light_projection = glm::ortho(min_x, max_x, min_y, max_y, -max_z, -min_z);
  return light_projection * light_view;
}

inline void compute_cascades(
    DirectionalLightPacket &packet,
    const CameraFrame &camera
) {
  if (camera.orthographic) {
    packet.cascades_valid = false;
    return;
  }

  auto splits = compute_cascade_splits(camera.near_plane, camera.far_plane);

  glm::vec3 light_direction = packet.direction;
  if (glm::dot(light_direction, light_direction) <= 1.0e-6f) {
    light_direction = glm::vec3(0.0f, 0.0f, -1.0f);
  } else {
    light_direction = glm::normalize(light_direction);
  }

  float aspect = 1.0f;
  if (camera.projection[1][1] != 0.0f) {
    aspect = camera.projection[1][1] / camera.projection[0][0];
  }

  for (size_t i = 0; i < k_shadow_cascade_count; ++i) {
    float cascade_near = splits[i];
    float cascade_far = splits[i + 1];

    glm::mat4 cascade_projection = glm::perspective(
        glm::radians(camera.fov_degrees), aspect, cascade_near, cascade_far
    );
    glm::mat4 cascade_view_projection = cascade_projection * camera.view;
    glm::mat4 inverse_cascade_vp = glm::inverse(cascade_view_projection);

    float cascade_extent = cascade_far - cascade_near;
    float texel_snap = (cascade_extent * 2.0f) / 1024.0f;

    packet.cascade_matrices[i] = build_cascade_matrix(
        light_direction, inverse_cascade_vp, texel_snap
    );
    packet.cascade_split_depths[i] = cascade_far;
  }

  packet.cascades_valid = true;
}

inline LightFrameData collect_light_frame(ecs::World &world) {
  LightFrameData frame;
  frame.directional = make_fallback_directional_light();

  const auto main_camera = select_main_camera(world);
  size_t point_index = 0u;

  world.each<scene::Transform, Light>([&](EntityID entity_id, scene::Transform &transform, Light &light) {
    if (!world.active(entity_id)) {
      return;
    }

    switch (light.type) {
      case LightType::Directional: {
        if (frame.directional.valid) {
          return;
        }

        const auto *shadow = world.get<DirectionalShadowSettings>(entity_id);
        const DirectionalShadowSettings fallback_shadow{};
        const auto &settings = shadow != nullptr ? *shadow : fallback_shadow;

        frame.directional.valid = true;
        frame.directional.position = transform.position;
        frame.directional.direction = directional_light_direction(transform);
        frame.directional.ambient = light_term(light, glm::vec3(light.ambient_strength));
        frame.directional.diffuse = light_term(light, glm::vec3(light.diffuse_strength));
        frame.directional.specular = light_term(light, glm::vec3(light.specular_strength));
        frame.directional.light_space_matrix =
            build_directional_light_space_matrix(transform, settings);
        frame.directional.near_plane = settings.near_plane;
        frame.directional.far_plane = settings.far_plane;
        frame.directional.shadow_intensity = settings.shadow_intensity;
        return;
      }

      case LightType::Point: {
        if (point_index >= frame.point_lights.size()) {
          return;
        }

        const auto *attenuation = world.get<PointLightAttenuation>(entity_id);
        const PointLightAttenuation fallback_attenuation{};
        const auto &settings =
            attenuation != nullptr ? *attenuation : fallback_attenuation;

        auto &packet = frame.point_lights[point_index++];
        packet.valid = true;
        packet.position = transform.position;
        packet.ambient = light_term(light, glm::vec3(light.ambient_strength));
        packet.diffuse = light_term(light, glm::vec3(light.diffuse_strength));
        packet.specular = light_term(light, glm::vec3(light.specular_strength));
        packet.constant = settings.constant;
        packet.linear = settings.linear;
        packet.quadratic = settings.quadratic;
        return;
      }

      case LightType::Spot: {
        if (frame.spot.valid) {
          return;
        }

        scene::Transform *camera_transform = nullptr;
        Camera *camera = nullptr;

        if (const auto *target = world.get<SpotLightTarget>(entity_id);
            target != nullptr && target->camera.has_value() &&
            world.contains(*target->camera) && world.active(*target->camera)) {
          auto camera_entity = world.entity(*target->camera);
          camera_transform = camera_entity.get<scene::Transform>();
          camera = camera_entity.get<Camera>();
        }

        if ((camera_transform == nullptr || camera == nullptr) &&
            main_camera.has_value()) {
          camera_transform = main_camera->transform;
          camera = main_camera->camera;
        }

        if (camera_transform == nullptr || camera == nullptr) {
          return;
        }

        const auto *cone = world.get<SpotLightCone>(entity_id);
        const SpotLightCone fallback_cone{};
        const auto &settings = cone != nullptr ? *cone : fallback_cone;
        const auto *attenuation = world.get<SpotLightAttenuation>(entity_id);
        const SpotLightAttenuation fallback_attenuation{};
        const auto &attenuation_settings =
            attenuation != nullptr ? *attenuation : fallback_attenuation;

        frame.spot.valid = true;
        frame.spot.position = camera_transform->position;
        frame.spot.direction = camera->front;
        frame.spot.ambient = light_term(light, glm::vec3(light.ambient_strength));
        frame.spot.diffuse = light_term(light, glm::vec3(light.diffuse_strength));
        frame.spot.specular = light_term(light, glm::vec3(light.specular_strength));
        frame.spot.inner_cutoff_cos = settings.inner_cutoff_cos;
        frame.spot.outer_cutoff_cos = settings.outer_cutoff_cos;
        frame.spot.constant = attenuation_settings.constant;
        frame.spot.linear = attenuation_settings.linear;
        frame.spot.quadratic = attenuation_settings.quadratic;
        return;
      }
    }
  });

  if (frame.directional.valid && main_camera.has_value()) {
    auto camera_frame = extract_main_camera_frame(world);
    if (camera_frame.has_value()) {
      compute_cascades(frame.directional, *camera_frame);
    }
  }

  return frame;
}

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS

template <typename Params>
inline void populate_directional_light_params(const LightFrameData &frame, Params &params) {
  const auto &directional = frame.directional;
  params.directional.exposure.ambient = directional.ambient;
  params.directional.exposure.diffuse = directional.diffuse;
  params.directional.exposure.specular = directional.specular;
  params.directional.position = directional.position;
  params.directional.direction = directional.direction;
  params.light_space_matrix = directional.light_space_matrix;
}

template <typename Params>
inline void populate_point_light_params(const LightFrameData &frame, Params &params) {
  for (size_t i = 0; i < frame.point_lights.size(); ++i) {
    const auto &source = frame.point_lights[i];
    params.point_lights[i].position = source.position;
    params.point_lights[i].exposure.ambient = source.ambient;
    params.point_lights[i].exposure.diffuse = source.diffuse;
    params.point_lights[i].exposure.specular = source.specular;
    params.point_lights[i].attenuation.constant = source.constant;
    params.point_lights[i].attenuation.linear = source.linear;
    params.point_lights[i].attenuation.quadratic = source.quadratic;
  }
}

template <typename Params>
inline void populate_spot_light_params(const LightFrameData &frame, Params &params) {
  params.spot_light.exposure.ambient = frame.spot.ambient;
  params.spot_light.exposure.diffuse = frame.spot.diffuse;
  params.spot_light.exposure.specular = frame.spot.specular;
  params.spot_light.position = frame.spot.position;
  params.spot_light.direction = frame.spot.direction;
  params.spot_light.inner_cut_off = frame.spot.inner_cutoff_cos;
  params.spot_light.outer_cut_off = frame.spot.outer_cutoff_cos;
  params.spot_light.attenuation.constant = frame.spot.constant;
  params.spot_light.attenuation.linear = frame.spot.linear;
  params.spot_light.attenuation.quadratic = frame.spot.quadratic;
}

inline shader_bindings::engine_shaders_g_buffer_axsl::SceneLightParams
build_gbuffer_scene_params(const LightFrameData &frame) {
  using namespace shader_bindings::engine_shaders_g_buffer_axsl;

  SceneLightParams params{};
  params.bloom_layer = k_default_bloom_render_layer;
  populate_directional_light_params(frame, params);

  return params;
}

inline shader_bindings::engine_shaders_g_buffer_axsl::MaterialParams
build_gbuffer_material_params(const MaterialBindingState &material_binding) {
  using namespace shader_bindings::engine_shaders_g_buffer_axsl;

  MaterialParams params{};
  params.materials[0].base_color_factor = material_binding.base_color_factor;
  params.materials[0].emissive_factor = material_binding.emissive_factor;
  params.materials[0].metallic_channel = material_binding.metallic_channel;
  params.materials[0].roughness_channel = material_binding.roughness_channel;
  params.materials[0].metallic_factor = material_binding.metallic_factor;
  params.materials[0].roughness_factor = material_binding.roughness_factor;
  params.materials[0].occlusion_strength = material_binding.occlusion_strength;
  params.materials[0].normal_scale = material_binding.normal_scale;
  params.materials[0].height_scale = material_binding.height_scale;
  params.materials[0].bloom_intensity = material_binding.bloom_intensity;
  params.materials[0].alpha_mask_enabled = material_binding.alpha_mask_enabled;
  params.materials[0].alpha_cutoff = material_binding.alpha_cutoff;

  return params;
}

inline shader_bindings::engine_shaders_g_buffer_blend_axsl::MaterialParams
build_gbuffer_blend_material_params(const MaterialBindingState &material_binding) {
  using namespace shader_bindings::engine_shaders_g_buffer_blend_axsl;

  MaterialParams params{};
  params.materials[0].base_color_factor = material_binding.base_color_factor;
  params.materials[0].emissive_factor = material_binding.emissive_factor;
  params.materials[0].metallic_channel = material_binding.metallic_channel;
  params.materials[0].roughness_channel = material_binding.roughness_channel;
  params.materials[0].metallic_factor = material_binding.metallic_factor;
  params.materials[0].roughness_factor = material_binding.roughness_factor;
  params.materials[0].occlusion_strength = material_binding.occlusion_strength;
  params.materials[0].normal_scale = material_binding.normal_scale;
  params.materials[0].height_scale = material_binding.height_scale;
  params.materials[0].bloom_intensity = material_binding.bloom_intensity;
  params.materials[0].alpha_mask_enabled = material_binding.alpha_mask_enabled;
  params.materials[0].alpha_cutoff = material_binding.alpha_cutoff;

  return params;
}

inline shader_bindings::engine_shaders_light_axsl::LightParams
build_deferred_light_params(const LightFrameData &frame, bool ibl_available = false, float prefilter_max_lod = 4.0f) {
  using namespace shader_bindings::engine_shaders_light_axsl;

  LightParams params{};
  params.bloom_layer = k_default_bloom_render_layer;
  populate_directional_light_params(frame, params);
  populate_point_light_params(frame, params);

  if (frame.directional.cascades_valid) {
    for (size_t i = 0; i < k_shadow_cascade_count; ++i) {
      params.cascade_matrices[i] = frame.directional.cascade_matrices[i];
      params.cascade_split_depths[i] = frame.directional.cascade_split_depths[i];
    }
    params.cascades_enabled = 1;
  } else {
    for (size_t i = 0; i < k_shadow_cascade_count; ++i) {
      params.cascade_matrices[i] = frame.directional.light_space_matrix;
      params.cascade_split_depths[i] = 0.0f;
    }
    params.cascades_enabled = 0;
  }

  params.shadow_intensity = frame.directional.shadow_intensity;
  params.ibl_enabled = ibl_available ? 1 : 0;
  params.prefilter_max_lod = prefilter_max_lod;

  return params;
}

inline shader_bindings::engine_shaders_lighting_forward_axsl::SceneLightParams
build_forward_scene_params(const LightFrameData &frame) {
  using namespace shader_bindings::engine_shaders_lighting_forward_axsl;

  SceneLightParams params{};
  params.near_plane = frame.directional.near_plane;
  params.far_plane = frame.directional.far_plane;
  params.bloom_layer = k_default_bloom_render_layer;

  populate_directional_light_params(frame, params);
  populate_spot_light_params(frame, params);
  populate_point_light_params(frame, params);
  params.shadow_intensity = frame.directional.shadow_intensity;

  return params;
}

inline shader_bindings::engine_shaders_lighting_forward_axsl::MaterialParams
build_forward_material_params(const MaterialBindingState &material_binding) {
  using namespace shader_bindings::engine_shaders_lighting_forward_axsl;

  MaterialParams params{};
  params.materials[0].base_color_factor = material_binding.base_color_factor;
  params.materials[0].emissive_factor = material_binding.emissive_factor;
  params.materials[0].metallic_channel = material_binding.metallic_channel;
  params.materials[0].roughness_channel = material_binding.roughness_channel;
  params.materials[0].metallic_factor = material_binding.metallic_factor;
  params.materials[0].roughness_factor = material_binding.roughness_factor;
  params.materials[0].occlusion_strength = material_binding.occlusion_strength;
  params.materials[0].normal_scale = material_binding.normal_scale;
  params.materials[0].height_scale = material_binding.height_scale;
  params.materials[0].bloom_intensity = material_binding.bloom_intensity;
  params.materials[0].alpha_mask_enabled = material_binding.alpha_mask_enabled;
  params.materials[0].alpha_cutoff = material_binding.alpha_cutoff;

  return params;
}

#endif

} // namespace astralix::rendering
