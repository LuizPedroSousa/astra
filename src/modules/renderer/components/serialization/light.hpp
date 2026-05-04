#pragma once

#include "components/light.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"
#include <string>

namespace astralix::serialization {

inline std::string light_type_to_string(rendering::LightType type) {
  switch (type) {
    case rendering::LightType::Directional:
      return "directional";
    case rendering::LightType::Point:
      return "point";
    case rendering::LightType::Spot:
      return "spot";
  }

  return "unknown";
}

inline rendering::LightType light_type_from_string(const std::string &type) {
  if (type == "point") {
    return rendering::LightType::Point;
  }

  if (type == "spot") {
    return rendering::LightType::Spot;
  }

  return rendering::LightType::Directional;
}

inline ComponentSnapshot snapshot_component(const rendering::Light &light) {
  ComponentSnapshot snapshot{.name = "Light"};
  snapshot.fields.push_back({"type", light_type_to_string(light.type)});
  serialization::fields::append_vec3(snapshot.fields, "color", light.color);
  snapshot.fields.push_back({"intensity", light.intensity});
  snapshot.fields.push_back({"ambient_strength", light.ambient_strength});
  snapshot.fields.push_back({"diffuse_strength", light.diffuse_strength});
  snapshot.fields.push_back({"specular_strength", light.specular_strength});
  snapshot.fields.push_back({"casts_shadows", light.casts_shadows});
  return snapshot;
}

inline ComponentSnapshot
snapshot_component(const rendering::PointLightAttenuation &attenuation) {
  ComponentSnapshot snapshot{.name = "PointLightAttenuation"};
  snapshot.fields.push_back({"constant", attenuation.constant});
  snapshot.fields.push_back({"linear", attenuation.linear});
  snapshot.fields.push_back({"quadratic", attenuation.quadratic});
  return snapshot;
}

inline ComponentSnapshot snapshot_component(const rendering::SpotLightCone &cone) {
  ComponentSnapshot snapshot{.name = "SpotLightCone"};
  snapshot.fields.push_back({"inner_cutoff_cos", cone.inner_cutoff_cos});
  snapshot.fields.push_back({"outer_cutoff_cos", cone.outer_cutoff_cos});
  return snapshot;
}

inline ComponentSnapshot
snapshot_component(const rendering::SpotLightAttenuation &attenuation) {
  ComponentSnapshot snapshot{.name = "SpotLightAttenuation"};
  snapshot.fields.push_back({"constant", attenuation.constant});
  snapshot.fields.push_back({"linear", attenuation.linear});
  snapshot.fields.push_back({"quadratic", attenuation.quadratic});
  return snapshot;
}

inline ComponentSnapshot
snapshot_component(const rendering::DirectionalShadowSettings &settings) {
  ComponentSnapshot snapshot{.name = "DirectionalShadowSettings"};
  snapshot.fields.push_back({"ortho_extent", settings.ortho_extent});
  snapshot.fields.push_back({"near_plane", settings.near_plane});
  snapshot.fields.push_back({"far_plane", settings.far_plane});
  snapshot.fields.push_back({"shadow_intensity", settings.shadow_intensity});
  return snapshot;
}

inline ComponentSnapshot snapshot_component(const rendering::SpotLightTarget &target) {
  ComponentSnapshot snapshot{.name = "SpotLightTarget"};
  if (target.camera.has_value()) {
    snapshot.fields.push_back(
        {"camera", static_cast<std::string>(*target.camera)});
  }
  return snapshot;
}

inline void
apply_light_snapshot(ecs::EntityRef entity,
                     const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::Light>(rendering::Light{
      .type = light_type_from_string(
          serialization::fields::read_string(fields, "type")
              .value_or("directional")),
      .color =
          serialization::fields::read_vec3(fields, "color", glm::vec3(1.0f)),
      .intensity = serialization::fields::read_float(fields, "intensity")
                       .value_or(1.0f),
      .ambient_strength = serialization::fields::read_float(fields, "ambient_strength")
                              .value_or(0.2f),
      .diffuse_strength = serialization::fields::read_float(fields, "diffuse_strength")
                              .value_or(0.5f),
      .specular_strength = serialization::fields::read_float(fields, "specular_strength")
                               .value_or(0.5f),
      .casts_shadows =
          serialization::fields::read_bool(fields, "casts_shadows")
              .value_or(true),
  });
}

inline void apply_point_light_attenuation_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::PointLightAttenuation>(rendering::PointLightAttenuation{
      .constant = serialization::fields::read_float(fields, "constant")
                      .value_or(1.0f),
      .linear =
          serialization::fields::read_float(fields, "linear").value_or(0.045f),
      .quadratic = serialization::fields::read_float(fields, "quadratic")
                       .value_or(0.0075f),
  });
}

inline void apply_spot_light_cone_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::SpotLightCone>(rendering::SpotLightCone{
      .inner_cutoff_cos =
          serialization::fields::read_float(fields, "inner_cutoff_cos")
              .value_or(0.976296f),
      .outer_cutoff_cos =
          serialization::fields::read_float(fields, "outer_cutoff_cos")
              .value_or(0.953717f),
  });
}

inline void apply_spot_light_attenuation_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::SpotLightAttenuation>(rendering::SpotLightAttenuation{
      .constant = serialization::fields::read_float(fields, "constant")
                      .value_or(1.0f),
      .linear =
          serialization::fields::read_float(fields, "linear").value_or(0.045f),
      .quadratic = serialization::fields::read_float(fields, "quadratic")
                       .value_or(0.0075f),
  });
}

inline void apply_directional_shadow_settings_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::DirectionalShadowSettings>(rendering::DirectionalShadowSettings{
      .ortho_extent = serialization::fields::read_float(fields, "ortho_extent")
                          .value_or(10.0f),
      .near_plane = serialization::fields::read_float(fields, "near_plane")
                        .value_or(1.0f),
      .far_plane = serialization::fields::read_float(fields, "far_plane")
                       .value_or(100.0f),
      .shadow_intensity = serialization::fields::read_float(fields, "shadow_intensity")
                              .value_or(1.0f),
  });
}

inline void apply_spot_light_target_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::SpotLightTarget>(
      rendering::SpotLightTarget{.camera = serialization::fields::read_entity_id(fields,
                                                                      "camera")});
}

} // namespace astralix::serialization
