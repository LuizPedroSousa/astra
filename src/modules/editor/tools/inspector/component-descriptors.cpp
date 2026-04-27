#include "fields.hpp"

#include "components/camera.hpp"
#include "components/collider.hpp"
#include "components/light.hpp"
#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/model.hpp"
#include "components/rigidbody.hpp"
#include "components/skybox.hpp"
#include "components/tags.hpp"
#include "components/terrain-clipmap-controller.hpp"
#include "components/terrain-tile.hpp"
#include "components/text.hpp"
#include "components/transform.hpp"
#include "components/audio-emitter.hpp"
#include "components/audio-listener.hpp"

namespace astralix::editor::inspector_panel {
namespace {

const std::vector<std::string> kLightTypeOptions = {
    "directional",
    "point",
    "spot",
};
const std::vector<std::string> kRigidBodyModeOptions = {
    "dynamic",
    "static",
};
const std::vector<std::string> kCameraControllerModeOptions = {
    "free",
    "first_person",
    "third_person",
    "orbital",
};

template <typename T>
bool can_add_if_missing(ecs::EntityRef entity) {
  return entity.exists() && !entity.has<T>();
}

template <typename T>
void erase_component(ecs::EntityRef entity) {
  entity.erase<T>();
}

bool can_add_never(ecs::EntityRef) { return false; }

bool can_add_point_light_attenuation(ecs::EntityRef entity) {
  const auto *light = entity.get<rendering::Light>();
  return light != nullptr && light->type == rendering::LightType::Point &&
         !entity.has<rendering::PointLightAttenuation>();
}

bool can_add_spot_light_cone(ecs::EntityRef entity) {
  const auto *light = entity.get<rendering::Light>();
  return light != nullptr && light->type == rendering::LightType::Spot &&
         !entity.has<rendering::SpotLightCone>();
}

bool can_add_spot_light_attenuation(ecs::EntityRef entity) {
  const auto *light = entity.get<rendering::Light>();
  return light != nullptr && light->type == rendering::LightType::Spot &&
         !entity.has<rendering::SpotLightAttenuation>();
}

bool can_add_directional_shadow_settings(ecs::EntityRef entity) {
  const auto *light = entity.get<rendering::Light>();
  return light != nullptr &&
         light->type == rendering::LightType::Directional &&
         !entity.has<rendering::DirectionalShadowSettings>();
}

bool can_add_spot_light_target(ecs::EntityRef entity) {
  const auto *light = entity.get<rendering::Light>();
  return light != nullptr && light->type == rendering::LightType::Spot &&
         !entity.has<rendering::SpotLightTarget>();
}

bool editable_all_fields(std::string_view) { return true; }
bool editable_no_fields(std::string_view) { return false; }

bool editable_text_sprite_field(std::string_view field_name) {
  return field_name != "font_id";
}

bool editable_camera_controller_field(std::string_view field_name) {
  return field_name != "target";
}

bool editable_material_properties_field(std::string_view field_name) {
  return field_name == "base_color_factor" || field_name == "emissive_factor" ||
         field_name == "metallic_factor" || field_name == "roughness_factor" ||
         field_name == "occlusion_strength" || field_name == "normal_scale" ||
         field_name == "bloom_intensity";
}

const std::vector<std::string> *no_enum_options(std::string_view) {
  return nullptr;
}

const std::vector<std::string> *light_enum_options(std::string_view field_name) {
  return field_name == "type" ? &kLightTypeOptions : nullptr;
}

const std::vector<std::string> *rigid_body_enum_options(
    std::string_view field_name
) {
  return field_name == "mode" ? &kRigidBodyModeOptions : nullptr;
}

const std::vector<std::string> *camera_controller_enum_options(
    std::string_view field_name
) {
  return field_name == "mode" ? &kCameraControllerModeOptions : nullptr;
}

const ComponentDescriptor kComponentDescriptors[] = {
    {
        .name = "SceneEntity",
        .visible = false,
        .removable = false,
        .can_add = &can_add_never,
        .remove_component = nullptr,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "Transform",
        .can_add = &can_add_if_missing<scene::Transform>,
        .remove_component = &erase_component<scene::Transform>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "Camera",
        .can_add = &can_add_if_missing<rendering::Camera>,
        .remove_component = &erase_component<rendering::Camera>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "CameraController",
        .can_add = &can_add_if_missing<scene::CameraController>,
        .remove_component = &erase_component<scene::CameraController>,
        .field_editable = &editable_camera_controller_field,
        .enum_options = &camera_controller_enum_options,
    },
    {
        .name = "Light",
        .can_add = &can_add_if_missing<rendering::Light>,
        .remove_component = &erase_component<rendering::Light>,
        .field_editable = &editable_all_fields,
        .enum_options = &light_enum_options,
    },
    {
        .name = "PointLightAttenuation",
        .can_add = &can_add_point_light_attenuation,
        .remove_component = &erase_component<rendering::PointLightAttenuation>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "SpotLightCone",
        .can_add = &can_add_spot_light_cone,
        .remove_component = &erase_component<rendering::SpotLightCone>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "SpotLightAttenuation",
        .can_add = &can_add_spot_light_attenuation,
        .remove_component = &erase_component<rendering::SpotLightAttenuation>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "DirectionalShadowSettings",
        .can_add = &can_add_directional_shadow_settings,
        .remove_component = &erase_component<rendering::DirectionalShadowSettings>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "SpotLightTarget",
        .can_add = &can_add_spot_light_target,
        .remove_component = &erase_component<rendering::SpotLightTarget>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "ModelRef",
        .can_add = &can_add_if_missing<rendering::ModelRef>,
        .remove_component = &erase_component<rendering::ModelRef>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "MeshSet",
        .can_add = &can_add_if_missing<rendering::MeshSet>,
        .remove_component = &erase_component<rendering::MeshSet>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "MaterialSlots",
        .can_add = &can_add_if_missing<rendering::MaterialSlots>,
        .remove_component = &erase_component<rendering::MaterialSlots>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = k_material_properties_component_name.data(),
        .removable = false,
        .can_add = &can_add_never,
        .remove_component = nullptr,
        .field_editable = &editable_material_properties_field,
        .enum_options = &no_enum_options,
    },
    {
        .name = "ShaderBinding",
        .can_add = &can_add_if_missing<rendering::ShaderBinding>,
        .remove_component = &erase_component<rendering::ShaderBinding>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "TextureBindings",
        .can_add = &can_add_if_missing<rendering::TextureBindings>,
        .remove_component = &erase_component<rendering::TextureBindings>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "BloomSettings",
        .can_add = &can_add_if_missing<rendering::BloomSettings>,
        .remove_component = &erase_component<rendering::BloomSettings>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "SkyboxBinding",
        .can_add = &can_add_if_missing<rendering::SkyboxBinding>,
        .remove_component = &erase_component<rendering::SkyboxBinding>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "TextSprite",
        .can_add = &can_add_if_missing<rendering::TextSprite>,
        .remove_component = &erase_component<rendering::TextSprite>,
        .field_editable = &editable_text_sprite_field,
        .enum_options = &no_enum_options,
    },
    {
        .name = "RigidBody",
        .can_add = &can_add_if_missing<physics::RigidBody>,
        .remove_component = &erase_component<physics::RigidBody>,
        .field_editable = &editable_all_fields,
        .enum_options = &rigid_body_enum_options,
    },
    {
        .name = "BoxCollider",
        .can_add = &can_add_if_missing<physics::BoxCollider>,
        .remove_component = &erase_component<physics::BoxCollider>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "FitBoxColliderFromRenderMesh",
        .can_add = &can_add_if_missing<physics::FitBoxColliderFromRenderMesh>,
        .remove_component = &erase_component<physics::FitBoxColliderFromRenderMesh>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "Renderable",
        .can_add = &can_add_if_missing<rendering::Renderable>,
        .remove_component = &erase_component<rendering::Renderable>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "MainCamera",
        .can_add = &can_add_if_missing<rendering::MainCamera>,
        .remove_component = &erase_component<rendering::MainCamera>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "ShadowCaster",
        .can_add = &can_add_if_missing<rendering::ShadowCaster>,
        .remove_component = &erase_component<rendering::ShadowCaster>,
        .field_editable = &editable_no_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "AudioListener",
        .can_add = &can_add_if_missing<audio::AudioListener>,
        .remove_component = &erase_component<audio::AudioListener>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "AudioEmitter",
        .can_add = &can_add_if_missing<audio::AudioEmitter>,
        .remove_component = &erase_component<audio::AudioEmitter>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "TerrainTile",
        .can_add = &can_add_if_missing<terrain::TerrainTile>,
        .remove_component = &erase_component<terrain::TerrainTile>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
    {
        .name = "TerrainClipmapController",
        .can_add = &can_add_if_missing<terrain::TerrainClipmapController>,
        .remove_component = &erase_component<terrain::TerrainClipmapController>,
        .field_editable = &editable_all_fields,
        .enum_options = &no_enum_options,
    },
};

} // namespace

const ComponentDescriptor *component_descriptors() { return kComponentDescriptors; }

size_t component_descriptor_count() {
  return sizeof(kComponentDescriptors) / sizeof(kComponentDescriptors[0]);
}

const ComponentDescriptor *find_component_descriptor(std::string_view name) {
  for (size_t index = 0u; index < component_descriptor_count(); ++index) {
    const auto &descriptor = component_descriptors()[index];
    if (descriptor.name == name) {
      return &descriptor;
    }
  }

  return nullptr;
}

} // namespace astralix::editor::inspector_panel
