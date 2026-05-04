#include "scene-serializer.hpp"

#include "assert.hpp"
#include "axmesh-serializer.hpp"
#include "components/serialization/mesh.hpp"
#include "context-proxy.hpp"
#include "entities/scene.hpp"
#include "entities/serializers/component-snapshot-context.hpp"
#include "entities/serializers/derived-override-serializer.hpp"
#include "fnv1a.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"
#include "scene-snapshot.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace astralix {
namespace {

constexpr int k_scene_version = 1;

std::string scene_artifact_kind_to_string(SceneArtifactKind kind) {
  switch (kind) {
    case SceneArtifactKind::Source:
      return "source";
    case SceneArtifactKind::Preview:
      return "preview";
    case SceneArtifactKind::Runtime:
      return "runtime";
    default:
      ASTRA_EXCEPTION("Unknown scene artifact kind");
  }
}

SceneArtifactKind scene_artifact_kind_from_string(const std::string &kind) {
  if (kind == "source") {
    return SceneArtifactKind::Source;
  }

  if (kind == "preview") {
    return SceneArtifactKind::Preview;
  }

  if (kind == "runtime") {
    return SceneArtifactKind::Runtime;
  }

  ASTRA_EXCEPTION("Unknown scene artifact kind: ", kind);
}

SerializationFormat scene_serialization_format() {
  auto project = active_project();
  return project != nullptr ? project->get_config().serialization.format
                            : SerializationFormat::Json;
}

void reset_context(Ref<SerializationContext> &ctx) {
  ctx = SerializationContext::create(scene_serialization_format());
}

std::filesystem::path scene_absolute_path(
    const Scene &scene, SceneArtifactKind artifact_kind
) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve scene path without an active project");

  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      return project->resolve_path(scene.get_source_scene_path());
    case SceneArtifactKind::Preview:
      return project->resolve_path(scene.get_preview_scene_path());
    case SceneArtifactKind::Runtime:
      return project->resolve_path(scene.get_runtime_scene_path());
  }

  ASTRA_EXCEPTION("Unknown scene artifact kind");
}

std::filesystem::path generated_mesh_directory(
    const Scene &scene, SceneArtifactKind artifact_kind
) {
  const auto parent = scene_absolute_path(scene, artifact_kind).parent_path();
  return artifact_kind == SceneArtifactKind::Preview ? parent / "preview-meshes"
                                                     : parent / "meshes";
}

std::string project_relative_path(const std::filesystem::path &path) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve project-relative path without an active project");

  return path.lexically_relative(
                 std::filesystem::path(project->get_config().directory)
  )
      .generic_string();
}

std::string hash_mesh_set(const std::vector<Mesh> &meshes) {
  constexpr uint64_t k_mesh_set_hash_offset_basis = 1469598103934665603ull;

  uint64_t hash = k_mesh_set_hash_offset_basis;
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(meshes.size()));

  for (const auto &mesh : meshes) {
    hash = fnv1a64_append_value(hash, static_cast<int>(mesh.draw_type));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(mesh.vertices.size()));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(mesh.indices.size()));

    for (const auto &vertex : mesh.vertices) {
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.x)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.y)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.z)
      );
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.x));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.y));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.z));
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.texture_coordinates.x)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.texture_coordinates.y)
      );
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.x));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.y));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.z));
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.bitangent_sign)
      );
    }

    for (unsigned int index : mesh.indices) {
      hash = fnv1a64_append_value(hash, index);
    }
  }

  return fnv1a64_hex_digest(hash);
}

serialization::ComponentSnapshot externalize_mesh_set_snapshot(
    const Scene &scene,
    const rendering::MeshSet &mesh_set,
    SceneArtifactKind artifact_kind
) {
  const auto &meshes = mesh_set.meshes;
  const std::string asset_hash = hash_mesh_set(meshes);
  const auto asset_path =
      generated_mesh_directory(scene, artifact_kind) / (asset_hash + ".axmesh");

  if (!std::filesystem::exists(asset_path)) {
    AxMeshSerializer::write(asset_path, meshes);
  }

  return serialization::ComponentSnapshot{
      .name = "MeshSet",
      .fields = {
          serialization::fields::Field{
              .name = "asset.path",
              .value = project_relative_path(asset_path),
          },
      },
  };
}

serialization::ComponentSnapshot externalize_component_snapshot(
    const Scene &scene,
    const serialization::ComponentSnapshot &component,
    SceneArtifactKind artifact_kind
) {
  if (component.name != "MeshSet") {
    return component;
  }

  const auto asset_path =
      serialization::fields::read_string(component.fields, "asset.path");
  if (asset_path.has_value() && !asset_path->empty()) {
    return component;
  }

  const auto meshes = serialization::read_mesh_set(component.fields);
  return externalize_mesh_set_snapshot(
      scene, rendering::MeshSet{.meshes = meshes}, artifact_kind
  );
}

bool is_tag_component(const serialization::ComponentSnapshot &component) {
  return component.fields.empty();
}

struct SerializedEntityData {
  std::vector<std::string> tags;
  std::vector<serialization::ComponentSnapshot> components;
};

bool should_strip_component_for_artifact(
    const serialization::ComponentSnapshot &component,
    SceneArtifactKind artifact_kind
) {
  return artifact_kind == SceneArtifactKind::Runtime &&
         (component.name == "MetaEntityOwner" ||
          component.name == "DerivedEntity");
}

bool should_include_live_entity_in_artifact(
    ecs::EntityRef entity, SceneArtifactKind artifact_kind
) {
  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      return !entity.has<scene::DerivedEntity>();
    case SceneArtifactKind::Preview:
    case SceneArtifactKind::Runtime:
      return !entity.has<scene::EditorOnly>() &&
             !entity.has<scene::GeneratorSpec>();
  }

  return true;
}

template <typename T>
void append_filtered_snapshot_if_present(
    ecs::EntityRef entity,
    std::vector<serialization::ComponentSnapshot> &out,
    SceneArtifactKind artifact_kind
) {
  if (auto *component = entity.get<T>(); component != nullptr) {
    auto snapshot = serialization::snapshot_component(*component);
    if (!should_strip_component_for_artifact(snapshot, artifact_kind)) {
      out.push_back(std::move(snapshot));
    }
  }
}

void append_filtered_mesh_set_snapshot_if_present(
    ecs::EntityRef entity,
    std::vector<serialization::ComponentSnapshot> &out,
    const Scene &scene,
    SceneArtifactKind artifact_kind
) {
  if (auto *mesh_set = entity.get<rendering::MeshSet>(); mesh_set != nullptr) {
    auto snapshot =
        externalize_mesh_set_snapshot(scene, *mesh_set, artifact_kind);
    if (!should_strip_component_for_artifact(snapshot, artifact_kind)) {
      out.push_back(std::move(snapshot));
    }
  }
}

serialization::EntitySnapshot collect_live_entity_snapshot(
    const Scene &scene,
    ecs::EntityRef entity,
    SceneArtifactKind artifact_kind
) {
  serialization::EntitySnapshot snapshot{
      .id = entity.id(),
      .name = std::string(entity.name()),
      .active = entity.active(),
  };

  append_filtered_snapshot_if_present<scene::SceneEntity>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::EditorOnly>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::GeneratorSpec>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::DerivedEntity>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::MetaEntityOwner>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::Transform>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::Camera>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<scene::CameraController>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::Light>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::PointLightAttenuation>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::SpotLightCone>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::SpotLightAttenuation>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::DirectionalShadowSettings>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::SpotLightTarget>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::ModelRef>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_mesh_set_snapshot_if_present(
      entity, snapshot.components, scene, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::MaterialSlots>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::ShaderBinding>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::TextureBindings>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::BloomSettings>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::SkyboxBinding>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::TextSprite>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<physics::RigidBody>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<physics::BoxCollider>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<physics::FitBoxColliderFromRenderMesh>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<audio::AudioListener>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<audio::AudioEmitter>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<terrain::TerrainTile>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<terrain::TerrainClipmapController>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::LensFlare>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::Renderable>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::MainCamera>(
      entity, snapshot.components, artifact_kind
  );
  append_filtered_snapshot_if_present<rendering::ShadowCaster>(
      entity, snapshot.components, artifact_kind
  );

  return snapshot;
}

std::vector<serialization::EntitySnapshot> collect_live_scene_snapshots(
    const Scene &scene,
    const ecs::World &world,
    SceneArtifactKind artifact_kind
) {
  std::vector<serialization::EntitySnapshot> snapshots;

  world.each<scene::SceneEntity>([&](
                                    EntityID entity_id,
                                    const scene::SceneEntity &
                                ) {
    auto entity = const_cast<ecs::World &>(world).entity(entity_id);
    if (!should_include_live_entity_in_artifact(entity, artifact_kind)) {
      return;
    }

    snapshots.push_back(
        collect_live_entity_snapshot(scene, entity, artifact_kind)
    );
  });

  return snapshots;
}

SerializedEntityData split_entity_snapshot(
    const Scene &scene,
    const serialization::EntitySnapshot &entity,
    SceneArtifactKind artifact_kind
) {
  SerializedEntityData serialized;
  serialized.tags.reserve(entity.components.size());
  serialized.components.reserve(entity.components.size());

  for (const auto &snapshot : entity.components) {
    const auto component =
        externalize_component_snapshot(scene, snapshot, artifact_kind);
    if (is_tag_component(component)) {
      serialized.tags.push_back(component.name);
    } else {
      serialized.components.push_back(component);
    }
  }

  return serialized;
}

void inline_external_mesh_set(serialization::ComponentSnapshot &component) {
  if (component.name != "MeshSet") {
    return;
  }

  auto asset_path =
      serialization::fields::read_string(component.fields, "asset.path");
  ASTRA_ENSURE(!asset_path.has_value() || asset_path->empty(), "MeshSet asset path is required");

  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve MeshSet asset without an active project");

  const auto meshes = AxMeshSerializer::read(project->resolve_path(*asset_path));
  component.fields =
      serialization::snapshot_component(rendering::MeshSet{.meshes = meshes})
          .fields;
}

std::optional<serialization::EntitySnapshot>
read_entity_snapshot(ContextProxy entity_ctx) {
  if (entity_ctx["id"].kind() != SerializationTypeKind::String ||
      entity_ctx["name"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  serialization::EntitySnapshot entity{
      .id = EntityID(std::stoull(entity_ctx["id"].as<std::string>())),
      .name = entity_ctx["name"].as<std::string>(),
      .active = entity_ctx["active"].kind() == SerializationTypeKind::Bool
                    ? entity_ctx["active"].as<bool>()
                    : true,
  };

  auto tags_ctx = entity_ctx["tags"];
  if (tags_ctx.kind() == SerializationTypeKind::Array) {
    entity.components.reserve(entity_ctx["components"].size() + tags_ctx.size());

    for (size_t tag_index = 0; tag_index < tags_ctx.size(); ++tag_index) {
      auto tag_ctx = tags_ctx[static_cast<int>(tag_index)];
      ASTRA_ENSURE(tag_ctx.kind() != SerializationTypeKind::String, "Entity tag must be a string");
      entity.components.push_back(serialization::ComponentSnapshot{
          .name = tag_ctx.as<std::string>(),
      });
    }
  }

  const size_t component_count = entity_ctx["components"].size();
  entity.components.reserve(entity.components.size() + component_count);

  for (size_t component_index = 0; component_index < component_count;
       ++component_index) {
    auto component = read_component_snapshot(
        entity_ctx["components"][static_cast<int>(component_index)]
    );
    if (component.has_value()) {
      inline_external_mesh_set(*component);
      entity.components.push_back(std::move(*component));
    }
  }

  return entity;
}

void serialize_render_overrides(
    SerializationContext &ctx,
    const SceneRenderOverrides &overrides
) {
  if (overrides.empty()) {
    return;
  }

  if (overrides.ssgi.has_value()) {
    const auto &value = *overrides.ssgi;
    auto section = ctx["render_overrides"]["ssgi"];
    section["enabled"] = value.enabled;
    section["full_resolution"] = value.full_resolution;
    section["temporal"] = value.temporal;
    section["intensity"] = value.intensity;
    section["radius"] = value.radius;
    section["thickness"] = value.thickness;
    section["directions"] = value.directions;
    section["steps_per_direction"] = value.steps_per_direction;
    section["max_distance"] = value.max_distance;
    section["history_weight"] = value.history_weight;
    section["normal_reject_dot"] = value.normal_reject_dot;
    section["position_reject_distance"] = value.position_reject_distance;
  }

  if (overrides.ssr.has_value()) {
    const auto &value = *overrides.ssr;
    auto section = ctx["render_overrides"]["ssr"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
    section["max_distance"] = value.max_distance;
    section["thickness"] = value.thickness;
    section["max_steps"] = value.max_steps;
    section["stride"] = value.stride;
    section["roughness_cutoff"] = value.roughness_cutoff;
  }

  if (overrides.volumetric.has_value()) {
    const auto &value = *overrides.volumetric;
    auto section = ctx["render_overrides"]["volumetric"];
    section["enabled"] = value.enabled;
    section["max_steps"] = value.max_steps;
    section["density"] = value.density;
    section["scattering"] = value.scattering;
    section["max_distance"] = value.max_distance;
    section["intensity"] = value.intensity;
    section["fog_base_height"] = value.fog_base_height;
    section["height_falloff_rate"] = value.height_falloff_rate;
    section["noise_scale"] = value.noise_scale;
    section["noise_weight"] = value.noise_weight;
    section["wind_direction"]["x"] = value.wind_direction.x;
    section["wind_direction"]["y"] = value.wind_direction.y;
    section["wind_direction"]["z"] = value.wind_direction.z;
    section["wind_speed"] = value.wind_speed;
    section["temporal"] = value.temporal;
    section["temporal_blend_weight"] = value.temporal_blend_weight;
  }

  if (overrides.lens_flare.has_value()) {
    const auto &value = *overrides.lens_flare;
    auto section = ctx["render_overrides"]["lens_flare"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
    section["threshold"] = value.threshold;
    section["ghost_count"] = value.ghost_count;
    section["ghost_dispersal"] = value.ghost_dispersal;
    section["ghost_weight"] = value.ghost_weight;
    section["halo_radius"] = value.halo_radius;
    section["halo_weight"] = value.halo_weight;
    section["halo_thickness"] = value.halo_thickness;
    section["chromatic_aberration"] = value.chromatic_aberration;
  }

  if (overrides.eye_adaptation.has_value()) {
    const auto &value = *overrides.eye_adaptation;
    auto section = ctx["render_overrides"]["eye_adaptation"];
    section["enabled"] = value.enabled;
    section["min_log_luminance"] = value.min_log_luminance;
    section["max_log_luminance"] = value.max_log_luminance;
    section["adaptation_speed_up"] = value.adaptation_speed_up;
    section["adaptation_speed_down"] = value.adaptation_speed_down;
    section["key_value"] = value.key_value;
    section["low_percentile"] = value.low_percentile;
    section["high_percentile"] = value.high_percentile;
  }

  if (overrides.motion_blur.has_value()) {
    const auto &value = *overrides.motion_blur;
    auto section = ctx["render_overrides"]["motion_blur"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
    section["max_samples"] = value.max_samples;
    section["depth_threshold"] = value.depth_threshold;
  }

  if (overrides.chromatic_aberration.has_value()) {
    const auto &value = *overrides.chromatic_aberration;
    auto section = ctx["render_overrides"]["chromatic_aberration"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
  }

  if (overrides.vignette.has_value()) {
    const auto &value = *overrides.vignette;
    auto section = ctx["render_overrides"]["vignette"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
    section["smoothness"] = value.smoothness;
    section["roundness"] = value.roundness;
  }

  if (overrides.film_grain.has_value()) {
    const auto &value = *overrides.film_grain;
    auto section = ctx["render_overrides"]["film_grain"];
    section["enabled"] = value.enabled;
    section["intensity"] = value.intensity;
  }

  if (overrides.cas.has_value()) {
    const auto &value = *overrides.cas;
    auto section = ctx["render_overrides"]["cas"];
    section["enabled"] = value.enabled;
    section["sharpness"] = value.sharpness;
    section["contrast"] = value.contrast;
    section["sharpening_limit"] = value.sharpening_limit;
  }

  if (overrides.taa.has_value()) {
    const auto &value = *overrides.taa;
    auto section = ctx["render_overrides"]["taa"];
    section["enabled"] = value.enabled;
    section["blend_factor"] = value.blend_factor;
  }

  if (overrides.tonemapping.has_value()) {
    const auto &value = *overrides.tonemapping;
    auto section = ctx["render_overrides"]["tonemapping"];
    section["tonemap_operator"] = static_cast<int>(value.tonemap_operator);
    section["gamma"] = value.gamma;
    section["bloom_strength"] = value.bloom_strength;
  }
}

float read_override_number(ContextProxy ctx, float fallback = 0.0f) {
  const auto kind = ctx.kind();
  if (kind == SerializationTypeKind::Float) {
    return ctx.as<float>();
  }
  if (kind == SerializationTypeKind::Int) {
    return static_cast<float>(ctx.as<int>());
  }
  return fallback;
}

bool read_override_bool(ContextProxy ctx, bool fallback = false) {
  if (ctx.kind() == SerializationTypeKind::Bool) {
    return ctx.as<bool>();
  }
  return fallback;
}

SceneRenderOverrides deserialize_render_overrides(
    Ref<SerializationContext> ctx
) {
  SceneRenderOverrides overrides;
  if (ctx == nullptr) {
    return overrides;
  }

  auto root = (*ctx)["render_overrides"];
  if (root.kind() != SerializationTypeKind::Object) {
    return overrides;
  }

  auto ssgi = root["ssgi"];
  if (ssgi.kind() == SerializationTypeKind::Object) {
    SSGIConfig value;
    value.enabled = read_override_bool(ssgi["enabled"], value.enabled);
    value.full_resolution = read_override_bool(ssgi["full_resolution"], value.full_resolution);
    value.temporal = read_override_bool(ssgi["temporal"], value.temporal);
    value.intensity = read_override_number(ssgi["intensity"], value.intensity);
    value.radius = read_override_number(ssgi["radius"], value.radius);
    value.thickness = read_override_number(ssgi["thickness"], value.thickness);
    value.directions = static_cast<int>(read_override_number(ssgi["directions"], static_cast<float>(value.directions)));
    value.steps_per_direction = static_cast<int>(read_override_number(ssgi["steps_per_direction"], static_cast<float>(value.steps_per_direction)));
    value.max_distance = read_override_number(ssgi["max_distance"], value.max_distance);
    value.history_weight = read_override_number(ssgi["history_weight"], value.history_weight);
    value.normal_reject_dot = read_override_number(ssgi["normal_reject_dot"], value.normal_reject_dot);
    value.position_reject_distance = read_override_number(ssgi["position_reject_distance"], value.position_reject_distance);
    overrides.ssgi = value;
  }

  auto ssr = root["ssr"];
  if (ssr.kind() == SerializationTypeKind::Object) {
    SSRConfig value;
    value.enabled = read_override_bool(ssr["enabled"], value.enabled);
    value.intensity = read_override_number(ssr["intensity"], value.intensity);
    value.max_distance = read_override_number(ssr["max_distance"], value.max_distance);
    value.thickness = read_override_number(ssr["thickness"], value.thickness);
    value.max_steps = static_cast<int>(read_override_number(ssr["max_steps"], static_cast<float>(value.max_steps)));
    value.stride = read_override_number(ssr["stride"], value.stride);
    value.roughness_cutoff = read_override_number(ssr["roughness_cutoff"], value.roughness_cutoff);
    overrides.ssr = value;
  }

  auto volumetric = root["volumetric"];
  if (volumetric.kind() == SerializationTypeKind::Object) {
    VolumetricFogConfig value;
    value.enabled = read_override_bool(volumetric["enabled"], value.enabled);
    value.max_steps = static_cast<int>(read_override_number(volumetric["max_steps"], static_cast<float>(value.max_steps)));
    value.density = read_override_number(volumetric["density"], value.density);
    value.scattering = read_override_number(volumetric["scattering"], value.scattering);
    value.max_distance = read_override_number(volumetric["max_distance"], value.max_distance);
    value.intensity = read_override_number(volumetric["intensity"], value.intensity);
    value.fog_base_height = read_override_number(volumetric["fog_base_height"], value.fog_base_height);
    value.height_falloff_rate = read_override_number(volumetric["height_falloff_rate"], value.height_falloff_rate);
    value.noise_scale = read_override_number(volumetric["noise_scale"], value.noise_scale);
    value.noise_weight = read_override_number(volumetric["noise_weight"], value.noise_weight);
    auto wind = volumetric["wind_direction"];
    if (wind.kind() == SerializationTypeKind::Object) {
      value.wind_direction.x = read_override_number(wind["x"], value.wind_direction.x);
      value.wind_direction.y = read_override_number(wind["y"], value.wind_direction.y);
      value.wind_direction.z = read_override_number(wind["z"], value.wind_direction.z);
    }
    value.wind_speed = read_override_number(volumetric["wind_speed"], value.wind_speed);
    value.temporal = read_override_bool(volumetric["temporal"], value.temporal);
    value.temporal_blend_weight = read_override_number(volumetric["temporal_blend_weight"], value.temporal_blend_weight);
    overrides.volumetric = value;
  }

  auto lens_flare = root["lens_flare"];
  if (lens_flare.kind() == SerializationTypeKind::Object) {
    LensFlareConfig value;
    value.enabled = read_override_bool(lens_flare["enabled"], value.enabled);
    value.intensity = read_override_number(lens_flare["intensity"], value.intensity);
    value.threshold = read_override_number(lens_flare["threshold"], value.threshold);
    value.ghost_count = static_cast<int>(read_override_number(lens_flare["ghost_count"], static_cast<float>(value.ghost_count)));
    value.ghost_dispersal = read_override_number(lens_flare["ghost_dispersal"], value.ghost_dispersal);
    value.ghost_weight = read_override_number(lens_flare["ghost_weight"], value.ghost_weight);
    value.halo_radius = read_override_number(lens_flare["halo_radius"], value.halo_radius);
    value.halo_weight = read_override_number(lens_flare["halo_weight"], value.halo_weight);
    value.halo_thickness = read_override_number(lens_flare["halo_thickness"], value.halo_thickness);
    value.chromatic_aberration = read_override_number(lens_flare["chromatic_aberration"], value.chromatic_aberration);
    overrides.lens_flare = value;
  }

  auto eye_adaptation = root["eye_adaptation"];
  if (eye_adaptation.kind() == SerializationTypeKind::Object) {
    EyeAdaptationConfig value;
    value.enabled = read_override_bool(eye_adaptation["enabled"], value.enabled);
    value.min_log_luminance = read_override_number(eye_adaptation["min_log_luminance"], value.min_log_luminance);
    value.max_log_luminance = read_override_number(eye_adaptation["max_log_luminance"], value.max_log_luminance);
    value.adaptation_speed_up = read_override_number(eye_adaptation["adaptation_speed_up"], value.adaptation_speed_up);
    value.adaptation_speed_down = read_override_number(eye_adaptation["adaptation_speed_down"], value.adaptation_speed_down);
    value.key_value = read_override_number(eye_adaptation["key_value"], value.key_value);
    value.low_percentile = read_override_number(eye_adaptation["low_percentile"], value.low_percentile);
    value.high_percentile = read_override_number(eye_adaptation["high_percentile"], value.high_percentile);
    overrides.eye_adaptation = value;
  }

  auto motion_blur = root["motion_blur"];
  if (motion_blur.kind() == SerializationTypeKind::Object) {
    MotionBlurConfig value;
    value.enabled = read_override_bool(motion_blur["enabled"], value.enabled);
    value.intensity = read_override_number(motion_blur["intensity"], value.intensity);
    value.max_samples = static_cast<int>(read_override_number(motion_blur["max_samples"], static_cast<float>(value.max_samples)));
    value.depth_threshold = read_override_number(motion_blur["depth_threshold"], value.depth_threshold);
    overrides.motion_blur = value;
  }

  auto chromatic_aberration = root["chromatic_aberration"];
  if (chromatic_aberration.kind() == SerializationTypeKind::Object) {
    ChromaticAberrationConfig value;
    value.enabled = read_override_bool(chromatic_aberration["enabled"], value.enabled);
    value.intensity = read_override_number(chromatic_aberration["intensity"], value.intensity);
    overrides.chromatic_aberration = value;
  }

  auto vignette = root["vignette"];
  if (vignette.kind() == SerializationTypeKind::Object) {
    VignetteConfig value;
    value.enabled = read_override_bool(vignette["enabled"], value.enabled);
    value.intensity = read_override_number(vignette["intensity"], value.intensity);
    value.smoothness = read_override_number(vignette["smoothness"], value.smoothness);
    value.roundness = read_override_number(vignette["roundness"], value.roundness);
    overrides.vignette = value;
  }

  auto film_grain = root["film_grain"];
  if (film_grain.kind() == SerializationTypeKind::Object) {
    FilmGrainConfig value;
    value.enabled = read_override_bool(film_grain["enabled"], value.enabled);
    value.intensity = read_override_number(film_grain["intensity"], value.intensity);
    overrides.film_grain = value;
  }

  auto depth_of_field = root["depth_of_field"];
  if (depth_of_field.kind() == SerializationTypeKind::Object) {
    DepthOfFieldConfig value;
    value.enabled = read_override_bool(depth_of_field["enabled"], value.enabled);
    value.focus_distance = read_override_number(depth_of_field["focus_distance"], value.focus_distance);
    value.focus_range = read_override_number(depth_of_field["focus_range"], value.focus_range);
    value.max_blur_radius = read_override_number(depth_of_field["max_blur_radius"], value.max_blur_radius);
    value.sample_count = static_cast<int>(read_override_number(depth_of_field["sample_count"], static_cast<float>(value.sample_count)));
    overrides.depth_of_field = value;
  }

  auto cas = root["cas"];
  if (cas.kind() == SerializationTypeKind::Object) {
    CASConfig value;
    value.enabled = read_override_bool(cas["enabled"], value.enabled);
    value.sharpness = read_override_number(cas["sharpness"], value.sharpness);
    value.contrast = read_override_number(cas["contrast"], value.contrast);
    value.sharpening_limit = read_override_number(cas["sharpening_limit"], value.sharpening_limit);
    overrides.cas = value;
  }

  auto taa = root["taa"];
  if (taa.kind() == SerializationTypeKind::Object) {
    TAAConfig value;
    value.enabled = read_override_bool(taa["enabled"], value.enabled);
    value.blend_factor = read_override_number(taa["blend_factor"], value.blend_factor);
    overrides.taa = value;
  }

  auto tonemapping = root["tonemapping"];
  if (tonemapping.kind() == SerializationTypeKind::Object) {
    TonemappingConfig value;
    value.tonemap_operator = static_cast<TonemapOperator>(
        static_cast<int>(read_override_number(
            tonemapping["tonemap_operator"],
            static_cast<float>(static_cast<int>(value.tonemap_operator))
        ))
    );
    value.gamma = read_override_number(tonemapping["gamma"], value.gamma);
    value.bloom_strength = read_override_number(tonemapping["bloom_strength"], value.bloom_strength);
    overrides.tonemapping = value;
  }

  return overrides;
}

void serialize_entities_to_context(
    SceneSerializer &serializer,
    const Scene &scene,
    const std::vector<serialization::EntitySnapshot> &entities
) {
  SerializationContext &ctx = *serializer.get_ctx().get();

  ctx["scene"]["version"] = k_scene_version;
  ctx["scene"]["kind"] =
      scene_artifact_kind_to_string(serializer.get_artifact_kind());
  ctx["scene"]["id"] = scene.get_scene_id();
  ctx["scene"]["type"] = scene.get_type();

  switch (serializer.get_artifact_kind()) {
    case SceneArtifactKind::Source:
      serialize_derived_state(ctx, scene.get_derived_state());
      break;
    case SceneArtifactKind::Preview:
      if (const auto &preview_info = scene.get_preview_build_info();
          preview_info.has_value()) {
        ctx["build"]["source_revision"] =
            static_cast<int>(preview_info->source_revision);
        ctx["build"]["built_at_utc"] = preview_info->built_at_utc;
      }
      break;
    case SceneArtifactKind::Runtime:
      if (const auto &runtime_info = scene.get_runtime_promotion_info();
          runtime_info.has_value()) {
        ctx["build"]["promoted_from_preview_revision"] =
            static_cast<int>(runtime_info->promoted_from_preview_revision);
        ctx["build"]["promoted_at_utc"] = runtime_info->promoted_at_utc;
      }
      break;
  }

  serialize_render_overrides(ctx, scene.render_overrides());

  std::vector<SerializedEntityData> serialized_entities;
  serialized_entities.reserve(entities.size());

  for (const auto &entity : entities) {
    serialized_entities.push_back(
        split_entity_snapshot(scene, entity, serializer.get_artifact_kind())
    );
  }

  for (size_t i = 0; i < entities.size(); ++i) {
    const auto &entity = entities[i];
    const auto &serialized = serialized_entities[i];

    ctx["entities"][static_cast<int>(i)]["active"] = entity.active;
    ctx["entities"][static_cast<int>(i)]["id"] =
        static_cast<std::string>(entity.id);
    ctx["entities"][static_cast<int>(i)]["name"] = entity.name;

    for (size_t j = 0; j < serialized.tags.size(); ++j) {
      ctx["entities"][static_cast<int>(i)]["tags"][static_cast<int>(j)] =
          serialized.tags[j];
    }

    for (size_t j = 0; j < serialized.components.size(); ++j) {
      const auto &component = serialized.components[j];
      auto component_ctx =
          ctx["entities"][static_cast<int>(i)]["components"][static_cast<int>(j)];
      component_ctx["type"] = component.name;

      for (const auto &field : component.fields) {
        write_nested_field(component_ctx["fields"], field.name, field.value);
      }
    }
  }
}

} // namespace

SceneSerializer::SceneSerializer(Ref<Scene> scene)
    : Serializer(), m_scene(scene), m_artifact_kind(SceneArtifactKind::Source) {}

SceneSerializer::SceneSerializer()
    : Serializer(), m_artifact_kind(SceneArtifactKind::Source) {}

void SceneSerializer::serialize() {
  auto scene = m_scene.lock();
  if (scene == nullptr) {
    return;
  }

  serialize_world(scene->world());
}

std::vector<serialization::EntitySnapshot>
SceneSerializer::collect_artifact_snapshots(const ecs::World &world) const {
  auto scene = m_scene.lock();
  ASTRA_ENSURE(scene == nullptr,
               "Cannot collect scene snapshots without a bound scene");
  return collect_live_scene_snapshots(*scene, world, m_artifact_kind);
}

void SceneSerializer::serialize_world(const ecs::World &world) {
  serialize_snapshots(collect_artifact_snapshots(world));
}

void SceneSerializer::serialize_snapshots(
    const std::vector<serialization::EntitySnapshot> &entities
) {
  auto scene = m_scene.lock();
  if (scene == nullptr) {
    return;
  }

  ASTRA_ENSURE(scene->get_scene_id().empty(), "Scene id is not configured for scene type ", scene->get_type());
  reset_context(m_ctx);
  serialize_entities_to_context(*this, *scene, entities);
}

void SceneSerializer::deserialize() {
  auto scene = m_scene.lock();
  if (scene == nullptr || m_ctx == nullptr) {
    return;
  }

  std::vector<serialization::EntitySnapshot> entities;

  ASTRA_ENSURE(
      (*m_ctx)["scene"]["version"].kind() != SerializationTypeKind::Int,
      "Scene version is missing or invalid"
  );
  const int scene_version = (*m_ctx)["scene"]["version"].as<int>();
  ASTRA_ENSURE(
      scene_version != k_scene_version,
      "Unsupported scene version: ",
      scene_version
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["kind"].kind() != SerializationTypeKind::String,
      "Scene kind is missing"
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["id"].kind() != SerializationTypeKind::String,
      "Scene id is missing"
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["type"].kind() != SerializationTypeKind::String,
      "Scene type is missing"
  );

  const auto scene_kind = scene_artifact_kind_from_string(
      (*m_ctx)["scene"]["kind"].as<std::string>()
  );
  const std::string scene_id = (*m_ctx)["scene"]["id"].as<std::string>();
  const std::string scene_type = (*m_ctx)["scene"]["type"].as<std::string>();

  ASTRA_ENSURE(scene_kind != m_artifact_kind, "Scene kind mismatch. Expected ", scene_artifact_kind_to_string(m_artifact_kind), ", got ", scene_artifact_kind_to_string(scene_kind));
  ASTRA_ENSURE(scene_id != scene->get_scene_id(), "Scene id mismatch. Expected ", scene->get_scene_id(), ", got ", scene_id);
  ASTRA_ENSURE(scene_type != scene->get_type(), "Scene type mismatch. Expected ", scene->get_type(), ", got ", scene_type);

  const size_t entity_count = (*m_ctx)["entities"].size();
  entities.reserve(entity_count);

  for (size_t index = 0; index < entity_count; ++index) {
    auto entity =
        read_entity_snapshot((*m_ctx)["entities"][static_cast<int>(index)]);
    if (entity.has_value()) {
      entities.push_back(std::move(*entity));
    }
  }

  serialization::apply_scene_snapshots(scene->world(), entities);
  scene->set_render_overrides(deserialize_render_overrides(m_ctx));

  switch (m_artifact_kind) {
    case SceneArtifactKind::Source:
      scene->set_derived_state(deserialize_derived_state(m_ctx));
      scene->set_preview_build_info(std::nullopt);
      scene->set_runtime_promotion_info(std::nullopt);
      break;
    case SceneArtifactKind::Preview: {
      ScenePreviewBuildInfo preview_info;
      bool has_preview_info = false;
      if ((*m_ctx)["build"]["source_revision"].kind() ==
          SerializationTypeKind::Int) {
        preview_info.source_revision = static_cast<uint64_t>(
            (*m_ctx)["build"]["source_revision"].as<int>()
        );
        has_preview_info = true;
      }
      if ((*m_ctx)["build"]["built_at_utc"].kind() ==
          SerializationTypeKind::String) {
        preview_info.built_at_utc =
            (*m_ctx)["build"]["built_at_utc"].as<std::string>();
        has_preview_info = true;
      }
      scene->set_preview_build_info(
          has_preview_info ? std::optional<ScenePreviewBuildInfo>(preview_info)
                           : std::nullopt
      );
      scene->set_runtime_promotion_info(std::nullopt);
      scene->set_derived_state({});
      break;
    }
    case SceneArtifactKind::Runtime: {
      SceneRuntimePromotionInfo runtime_info;
      bool has_runtime_info = false;
      if ((*m_ctx)["build"]["promoted_from_preview_revision"].kind() ==
          SerializationTypeKind::Int) {
        runtime_info.promoted_from_preview_revision = static_cast<uint64_t>(
            (*m_ctx)["build"]["promoted_from_preview_revision"].as<int>()
        );
        has_runtime_info = true;
      }
      if ((*m_ctx)["build"]["promoted_at_utc"].kind() ==
          SerializationTypeKind::String) {
        runtime_info.promoted_at_utc =
            (*m_ctx)["build"]["promoted_at_utc"].as<std::string>();
        has_runtime_info = true;
      }
      scene->set_runtime_promotion_info(
          has_runtime_info
              ? std::optional<SceneRuntimePromotionInfo>(runtime_info)
              : std::nullopt
      );
      scene->set_preview_build_info(std::nullopt);
      scene->set_derived_state({});
      break;
    }
  }
}

} // namespace astralix
