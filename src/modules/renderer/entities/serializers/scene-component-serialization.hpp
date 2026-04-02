#pragma once

#include "components/serialization/camera.hpp"
#include "components/serialization/collider.hpp"
#include "components/serialization/light.hpp"
#include "components/serialization/material.hpp"
#include "components/serialization/mesh.hpp"
#include "components/serialization/model.hpp"
#include "components/serialization/rigidbody.hpp"
#include "components/serialization/skybox.hpp"
#include "components/serialization/tags.hpp"
#include "components/serialization/text.hpp"
#include "components/serialization/transform.hpp"
#include "scene-snapshot-types.hpp"
#include <string_view>
#include <utility>
#include <vector>

namespace astralix::serialization {

enum class ComponentType {
  SceneEntity,
  Renderable,
  MainCamera,
  ShadowCaster,
  Transform,
  Camera,
  CameraController,
  Light,
  PointLightAttenuation,
  SpotLightCone,
  DirectionalShadowSettings,
  SpotLightTarget,
  ModelRef,
  MeshSet,
  MaterialSlots,
  ShaderBinding,
  TextureBindings,
  BloomSettings,
  SkyboxBinding,
  TextSprite,
  RigidBody,
  BoxCollider,
  FitBoxColliderFromRenderMesh,
  Unknown,
};

inline ComponentType component_type_from_string(std::string_view name) {
  static const std::pair<std::string_view, ComponentType> mapping[] = {
      {"SceneEntity", ComponentType::SceneEntity},
      {"Renderable", ComponentType::Renderable},
      {"MainCamera", ComponentType::MainCamera},
      {"ShadowCaster", ComponentType::ShadowCaster},
      {"Transform", ComponentType::Transform},
      {"Camera", ComponentType::Camera},
      {"CameraController", ComponentType::CameraController},
      {"Light", ComponentType::Light},
      {"PointLightAttenuation", ComponentType::PointLightAttenuation},
      {"SpotLightCone", ComponentType::SpotLightCone},
      {"DirectionalShadowSettings", ComponentType::DirectionalShadowSettings},
      {"SpotLightTarget", ComponentType::SpotLightTarget},
      {"ModelRef", ComponentType::ModelRef},
      {"MeshSet", ComponentType::MeshSet},
      {"MaterialSlots", ComponentType::MaterialSlots},
      {"ShaderBinding", ComponentType::ShaderBinding},
      {"TextureBindings", ComponentType::TextureBindings},
      {"BloomSettings", ComponentType::BloomSettings},
      {"SkyboxBinding", ComponentType::SkyboxBinding},
      {"TextSprite", ComponentType::TextSprite},
      {"RigidBody", ComponentType::RigidBody},
      {"BoxCollider", ComponentType::BoxCollider},
      {"FitBoxColliderFromRenderMesh",
       ComponentType::FitBoxColliderFromRenderMesh},
  };

  for (const auto &[key, value] : mapping) {
    if (key == name) {
      return value;
    }
  }

  return ComponentType::Unknown;
}

template <typename T>
inline void append_snapshot_if_present(ecs::EntityRef entity, std::vector<ComponentSnapshot> &out) {
  if (auto *component = entity.get<T>(); component != nullptr) {
    out.push_back(snapshot_component(*component));
  }
}

inline void apply_component_snapshot(ecs::EntityRef entity, const ComponentSnapshot &snapshot) {
  const auto &fields = snapshot.fields;

  switch (component_type_from_string(snapshot.name)) {
    case ComponentType::SceneEntity:
      apply_scene_entity_snapshot(entity);
      break;

    case ComponentType::Renderable:
      apply_renderable_snapshot(entity);
      break;

    case ComponentType::MainCamera:
      apply_main_camera_snapshot(entity);
      break;

    case ComponentType::ShadowCaster:
      apply_shadow_caster_snapshot(entity);
      break;

    case ComponentType::Transform:
      apply_transform_snapshot(entity, fields);
      break;

    case ComponentType::Camera:
      apply_camera_snapshot(entity, fields);
      break;

    case ComponentType::CameraController:
      apply_camera_controller_snapshot(entity, fields);
      break;

    case ComponentType::Light:
      apply_light_snapshot(entity, fields);
      break;

    case ComponentType::PointLightAttenuation:
      apply_point_light_attenuation_snapshot(entity, fields);
      break;

    case ComponentType::SpotLightCone:
      apply_spot_light_cone_snapshot(entity, fields);
      break;

    case ComponentType::DirectionalShadowSettings:
      apply_directional_shadow_settings_snapshot(entity, fields);
      break;

    case ComponentType::SpotLightTarget:
      apply_spot_light_target_snapshot(entity, fields);
      break;

    case ComponentType::ModelRef:
      apply_model_ref_snapshot(entity, fields);
      break;

    case ComponentType::MeshSet:
      apply_mesh_set_snapshot(entity, fields);
      break;

    case ComponentType::MaterialSlots:
      apply_material_slots_snapshot(entity, fields);
      break;

    case ComponentType::ShaderBinding:
      apply_shader_binding_snapshot(entity, fields);
      break;

    case ComponentType::TextureBindings:
      apply_texture_bindings_snapshot(entity, fields);
      break;

    case ComponentType::BloomSettings:
      apply_bloom_settings_snapshot(entity, fields);
      break;

    case ComponentType::SkyboxBinding:
      apply_skybox_snapshot(entity, fields);
      break;

    case ComponentType::TextSprite:
      apply_text_sprite_snapshot(entity, fields);
      break;

    case ComponentType::RigidBody:
      apply_rigid_body_snapshot(entity, fields);
      break;

    case ComponentType::BoxCollider:
      apply_box_collider_snapshot(entity, fields);
      break;

    case ComponentType::FitBoxColliderFromRenderMesh:
      apply_fit_box_collider_from_render_mesh_snapshot(entity);
      break;

    case ComponentType::Unknown:
      break;
  }
}

} // namespace astralix::serialization
