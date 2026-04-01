#pragma once

#include "components/camera.hpp"
#include "components/light.hpp"
#include "components/transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "material-binding.hpp"
#include "scene-selection.hpp"
#include "world.hpp"
#include <array>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix::rendering {

inline constexpr float k_default_directional_shadow_extent = 10.0f;
inline constexpr float k_default_directional_shadow_near_plane = 1.0f;
inline constexpr float k_default_directional_shadow_far_plane = 100.0f;
inline constexpr size_t k_max_point_lights = 4u;

struct DirectionalLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(-4.0f, 8.0f, -3.0f);
  glm::vec3 ambient = glm::vec3(0.2f);
  glm::vec3 diffuse = glm::vec3(0.5f);
  glm::vec3 specular = glm::vec3(0.5f);
  glm::mat4 light_space_matrix = glm::mat4(1.0f);
  float near_plane = k_default_directional_shadow_near_plane;
  float far_plane = k_default_directional_shadow_far_plane;
};

struct PointLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 ambient = glm::vec3(0.0f);
  glm::vec3 diffuse = glm::vec3(0.0f);
  glm::vec3 specular = glm::vec3(0.0f);
  float constant = 1.0f;
  float linear = 0.045f;
  float quadratic = 0.0075f;
};

struct SpotLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 ambient = glm::vec3(0.0f);
  glm::vec3 diffuse = glm::vec3(0.0f);
  glm::vec3 specular = glm::vec3(0.0f);
  float inner_cutoff_cos = 0.0f;
  float outer_cutoff_cos = 0.0f;
};

struct LightFrameData {
  DirectionalLightPacket directional;
  std::array<PointLightPacket, k_max_point_lights> point_lights{};
  SpotLightPacket spot;
};

inline glm::vec3 light_term(const Light &light, const glm::vec3 &base) {
  return base * light.color * light.intensity;
}

inline glm::mat4
build_directional_light_space_matrix(const scene::Transform &transform, const DirectionalShadowSettings &shadow) {
  const glm::mat4 projection = glm::ortho(
      -shadow.ortho_extent, shadow.ortho_extent, -shadow.ortho_extent, shadow.ortho_extent, shadow.near_plane, shadow.far_plane
  );
  const glm::mat4 view = glm::lookAt(transform.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  return projection * view;
}

inline DirectionalLightPacket make_fallback_directional_light() {
  const scene::Transform transform{.position = glm::vec3(-4.0f, 8.0f, -3.0f)};
  const Light light{};
  const DirectionalShadowSettings shadow{};

  return DirectionalLightPacket{
      .valid = false,
      .position = transform.position,
      .ambient = light_term(light, glm::vec3(0.2f)),
      .diffuse = light_term(light, glm::vec3(0.5f)),
      .specular = light_term(light, glm::vec3(0.5f)),
      .light_space_matrix =
          build_directional_light_space_matrix(transform, shadow),
      .near_plane = shadow.near_plane,
      .far_plane = shadow.far_plane,
  };
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
        frame.directional.ambient = light_term(light, glm::vec3(0.2f));
        frame.directional.diffuse = light_term(light, glm::vec3(0.5f));
        frame.directional.specular = light_term(light, glm::vec3(0.5f));
        frame.directional.light_space_matrix =
            build_directional_light_space_matrix(transform, settings);
        frame.directional.near_plane = settings.near_plane;
        frame.directional.far_plane = settings.far_plane;
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
        packet.ambient = light_term(light, glm::vec3(5.0f));
        packet.diffuse = light_term(light, glm::vec3(2.0f));
        packet.specular = light_term(light, glm::vec3(1.0f));
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

        frame.spot.valid = true;
        frame.spot.position = camera_transform->position;
        frame.spot.direction = camera->front;
        frame.spot.ambient = light_term(light, glm::vec3(0.2f));
        frame.spot.diffuse = light_term(light, glm::vec3(0.5f));
        frame.spot.specular = light_term(light, glm::vec3(1.0f));
        frame.spot.inner_cutoff_cos = settings.inner_cutoff_cos;
        frame.spot.outer_cutoff_cos = settings.outer_cutoff_cos;
        return;
      }
    }
  });

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
  params.directional.direction =
      glm::normalize(glm::vec3(0.0f) - directional.position);
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
}

inline shader_bindings::engine_shaders_g_buffer_axsl::LightParams
build_gbuffer_light_params(const LightFrameData &frame, const MaterialBindingState &material_binding) {
  using namespace shader_bindings::engine_shaders_g_buffer_axsl;

  LightParams params{};
  params.materials[0].diffuse = material_binding.diffuse_slot;
  params.materials[0].specular = material_binding.specular_slot;
  params.materials[0].shininess = material_binding.shininess;
  params.materials[0].emissive = material_binding.emissive;
  params.materials[0].bloom_intensity = material_binding.bloom_intensity;
  params.normal_map = material_binding.normal_map_slot >= 0
                          ? material_binding.normal_map_slot
                          : material_binding.diffuse_slot;
  params.displacement_map = material_binding.displacement_map_slot >= 0
                                ? material_binding.displacement_map_slot
                                : material_binding.specular_slot;
  params.bloom_layer = k_default_bloom_render_layer;
  populate_directional_light_params(frame, params);

  return params;
}

inline shader_bindings::engine_shaders_light_axsl::LightParams
build_deferred_light_params(
    const LightFrameData &frame,
    int shadow_map_slot,
    int g_position_slot,
    int g_normal_slot,
    int g_albedo_slot,
    int g_emissive_slot,
    int g_entity_id_slot,
    int g_ssao_slot
) {
  using namespace shader_bindings::engine_shaders_light_axsl;

  LightParams params{};
  params.shadow_map = shadow_map_slot;
  params.g_position = g_position_slot;
  params.g_normal = g_normal_slot;
  params.g_albedo = g_albedo_slot;
  params.g_emissive = g_emissive_slot;
  params.g_entity_id = g_entity_id_slot;
  params.g_ssao = g_ssao_slot;
  params.bloom_layer = k_default_bloom_render_layer;
  populate_directional_light_params(frame, params);
  populate_point_light_params(frame, params);

  return params;
}

inline shader_bindings::engine_shaders_lighting_forward_axsl::LightParams
build_forward_light_params(const LightFrameData &frame, const MaterialBindingState &material_binding, int shadow_map_slot) {
  using namespace shader_bindings::engine_shaders_lighting_forward_axsl;

  LightParams params{};
  params.materials[0].diffuse = material_binding.diffuse_slot;
  params.materials[0].specular = material_binding.specular_slot;
  params.materials[0].shininess = material_binding.shininess;
  params.materials[0].emissive = material_binding.emissive;
  params.materials[0].bloom_intensity = material_binding.bloom_intensity;
  params.normal_map = material_binding.normal_map_slot >= 0
                          ? material_binding.normal_map_slot
                          : material_binding.diffuse_slot;
  params.displacement_map = material_binding.displacement_map_slot >= 0
                                ? material_binding.displacement_map_slot
                                : material_binding.specular_slot;
  params.shadow_map = shadow_map_slot;
  params.near_plane = frame.directional.near_plane;
  params.far_plane = frame.directional.far_plane;
  params.bloom_layer = k_default_bloom_render_layer;

  populate_directional_light_params(frame, params);
  populate_spot_light_params(frame, params);
  populate_point_light_params(frame, params);

  return params;
}

#endif

} // namespace astralix::rendering
