#include "scene.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "adapters/file/file-stream-writer.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "entities/scene-build-context.hpp"
#include "managers/project-manager.hpp"
#include "project.hpp"
#include "serializers/scene-snapshot.hpp"
#include "serializers/scene-serializer.hpp"
#include "stream-buffer.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace astralix {
namespace {

std::filesystem::path scene_path(const Scene &scene, SceneArtifactKind artifact_kind) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve scene path without an active project");

  const std::string *relative_path = nullptr;
  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      relative_path = &scene.get_source_scene_path();
      break;
    case SceneArtifactKind::Preview:
      relative_path = &scene.get_preview_scene_path();
      break;
    case SceneArtifactKind::Runtime:
      relative_path = &scene.get_runtime_scene_path();
      break;
  }

  ASTRA_ENSURE(relative_path == nullptr || relative_path->empty(),
               "Scene path is not configured for scene ",
               scene.get_type());

  return project->resolve_path(*relative_path);
}

bool has_component_named(const serialization::EntitySnapshot &entity,
                         std::string_view name) {
  return std::any_of(
      entity.components.begin(),
      entity.components.end(),
      [name](const auto &component) { return component.name == name; }
  );
}

bool is_provenance_component(std::string_view name) {
  return name == "SceneEntity" || name == "DerivedEntity" ||
         name == "MetaEntityOwner";
}

bool field_lists_equal(
    const serialization::fields::FieldList &lhs,
    const serialization::fields::FieldList &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0; index < lhs.size(); ++index) {
    if (lhs[index].name != rhs[index].name ||
        lhs[index].value != rhs[index].value) {
      return false;
    }
  }

  return true;
}

bool component_snapshots_equal(
    const serialization::ComponentSnapshot &lhs,
    const serialization::ComponentSnapshot &rhs
) {
  return lhs.name == rhs.name && field_lists_equal(lhs.fields, rhs.fields);
}

std::optional<DerivedEntityKey>
derived_entity_key(const serialization::EntitySnapshot &entity) {
  const auto owner_it = std::find_if(
      entity.components.begin(),
      entity.components.end(),
      [](const auto &component) { return component.name == "MetaEntityOwner"; }
  );
  if (owner_it == entity.components.end()) {
    return std::nullopt;
  }

  const auto generator_id =
      serialization::fields::read_string(owner_it->fields, "generator_id");
  const auto stable_key =
      serialization::fields::read_string(owner_it->fields, "stable_key");
  if (!generator_id.has_value() || !stable_key.has_value() ||
      generator_id->empty() || stable_key->empty()) {
    return std::nullopt;
  }

  return DerivedEntityKey{
      .generator_id = *generator_id,
      .stable_key = *stable_key,
  };
}

std::string derived_entity_lookup_key(const DerivedEntityKey &key) {
  return key.generator_id + '\x1f' + key.stable_key;
}

std::vector<serialization::ComponentSnapshot>
filtered_derived_components(const serialization::EntitySnapshot &entity) {
  std::vector<serialization::ComponentSnapshot> components;
  components.reserve(entity.components.size());

  for (const auto &component : entity.components) {
    if (!is_provenance_component(component.name)) {
      components.push_back(component);
    }
  }

  return components;
}

template <typename T>
bool should_capture_component_in_derived_state(ecs::EntityRef entity) {
  if (!entity.has<T>()) {
    return false;
  }

  if (!entity.has<terrain::TerrainTile>()) {
    return true;
  }

  using Component = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<Component, rendering::Renderable> ||
                std::is_same_v<Component, rendering::ShadowCaster> ||
                std::is_same_v<Component, rendering::MeshSet> ||
                std::is_same_v<Component, rendering::ShaderBinding> ||
                std::is_same_v<Component, rendering::MaterialSlots>) {
    return false;
  }

  return true;
}

template <typename T>
void append_derived_state_component_if_present(
    ecs::EntityRef entity,
    std::vector<serialization::ComponentSnapshot> &components
) {
  if (!should_capture_component_in_derived_state<T>(entity)) {
    return;
  }

  serialization::append_snapshot_if_present<T>(entity, components);
}

serialization::EntitySnapshot collect_derived_entity_snapshot(ecs::EntityRef entity) {
  serialization::EntitySnapshot snapshot{
      .id = entity.id(),
      .name = std::string(entity.name()),
      .active = entity.active(),
  };

  append_derived_state_component_if_present<scene::SceneEntity>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::EditorOnly>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::GeneratorSpec>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::DerivedEntity>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::MetaEntityOwner>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::Transform>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::Camera>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<scene::CameraController>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::Light>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::PointLightAttenuation>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::SpotLightCone>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::SpotLightAttenuation>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::DirectionalShadowSettings>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::SpotLightTarget>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::ModelRef>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::MeshSet>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::MaterialSlots>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::ShaderBinding>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::TextureBindings>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::BloomSettings>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::SkyboxBinding>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::TextSprite>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<physics::RigidBody>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<physics::BoxCollider>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<physics::FitBoxColliderFromRenderMesh>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<audio::AudioListener>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<audio::AudioEmitter>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<terrain::TerrainTile>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<terrain::TerrainClipmapController>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::Renderable>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::MainCamera>(
      entity, snapshot.components
  );
  append_derived_state_component_if_present<rendering::ShadowCaster>(
      entity, snapshot.components
  );

  return snapshot;
}

std::optional<DerivedOverrideRecord> build_derived_override_record(
    const DerivedEntityKey &key,
    const serialization::EntitySnapshot &baseline_entity,
    const serialization::EntitySnapshot &actual_entity
) {
  DerivedOverrideRecord override_record{
      .key = key,
      .active = actual_entity.active,
  };
  if (actual_entity.name != baseline_entity.name) {
    override_record.name = actual_entity.name;
  }

  const auto baseline_components = filtered_derived_components(baseline_entity);
  const auto actual_components = filtered_derived_components(actual_entity);

  for (const auto &baseline_component : baseline_components) {
    const auto actual_it = std::find_if(
        actual_components.begin(),
        actual_components.end(),
        [&](const auto &component) {
          return component.name == baseline_component.name;
        }
    );
    if (actual_it == actual_components.end()) {
      override_record.removed_components.push_back(baseline_component.name);
    }
  }

  for (const auto &actual_component : actual_components) {
    const auto baseline_it = std::find_if(
        baseline_components.begin(),
        baseline_components.end(),
        [&](const auto &component) {
          return component.name == actual_component.name;
        }
    );
    if (baseline_it == baseline_components.end() ||
        !component_snapshots_equal(*baseline_it, actual_component)) {
      override_record.components.push_back(actual_component);
    }
  }

  const bool active_changed = actual_entity.active != baseline_entity.active;
  if (!active_changed && !override_record.name.has_value() &&
      override_record.removed_components.empty() &&
      override_record.components.empty()) {
    return std::nullopt;
  }

  return override_record;
}

std::unordered_map<std::string, serialization::EntitySnapshot>
collect_derived_entity_snapshot_map(const ecs::World &world) {
  std::unordered_map<std::string, serialization::EntitySnapshot> snapshots;

  world.each<scene::DerivedEntity>([&](
                                      EntityID entity_id,
                                      const scene::DerivedEntity &
                                  ) {
    auto entity = const_cast<ecs::World &>(world).entity(entity_id);
    auto snapshot = collect_derived_entity_snapshot(entity);
    const auto key = derived_entity_key(snapshot);
    if (!key.has_value()) {
      return;
    }

    snapshots.insert_or_assign(
        derived_entity_lookup_key(*key),
        std::move(snapshot)
    );
  });

  return snapshots;
}

void write_scene_buffer(SceneSerializer &serializer, const std::filesystem::path &path) {
  auto ctx = serializer.get_ctx();
  if (ctx == nullptr) {
    return;
  }

  std::filesystem::create_directories(path.parent_path());

  ElasticArena arena(KB(64));
  auto *block = ctx->to_buffer(arena);
  auto writer = FileStreamWriter(path, clone_stream_buffer(block));
  writer.write();
}

std::optional<std::filesystem::file_time_type>
artifact_write_time(const Scene &scene, SceneArtifactKind artifact_kind) {
  const auto path = scene_path(scene, artifact_kind);
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  return std::filesystem::last_write_time(path);
}

std::string utc_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const auto now_time_t = std::chrono::system_clock::to_time_t(now);
  const auto seconds =
      std::chrono::time_point_cast<std::chrono::seconds>(now);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds)
          .count();

  std::tm utc_time = *std::gmtime(&now_time_t);
  std::ostringstream stream;
  stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%S");
  if (milliseconds != 0) {
    stream << '.' << std::setfill('0') << std::setw(3) << milliseconds;
  }
  stream << 'Z';
  return stream.str();
}

} // namespace

Scene::Scene(std::string type) : m_scene_type(std::move(type)), m_serializer() {}

void Scene::bind_to_manifest_entry(const ProjectSceneEntryConfig &entry) {
  ASTRA_ENSURE(entry.id.empty(), "Scene manifest id is required");
  ASTRA_ENSURE(entry.type.empty(), "Scene manifest type is required");
  ASTRA_ENSURE(entry.source_path.empty(), "Scene manifest source path is required");
  ASTRA_ENSURE(entry.preview_path.empty(), "Scene manifest preview path is required");
  ASTRA_ENSURE(entry.runtime_path.empty(),
               "Scene manifest runtime path is required");
  ASTRA_ENSURE(m_scene_type != entry.type, "Scene type mismatch. Runtime type: ", m_scene_type, ", manifest type: ", entry.type);

  m_scene_id = entry.id;
  m_source_scene_path = entry.source_path;
  m_preview_scene_path = entry.preview_path;
  m_runtime_scene_path = entry.runtime_path;
}

void Scene::ensure_setup() {
  if (m_setup_complete) {
    return;
  }

  setup();
  m_setup_complete = true;
}

void Scene::update() {
  switch (m_session_kind) {
    case SceneSessionKind::Source:
      update_source();
      break;
    case SceneSessionKind::Preview:
      if (!is_playing()) {
        return;
      }
      update_preview();
      break;
    case SceneSessionKind::Runtime:
      if (!is_playing()) {
        return;
      }
      update_runtime();
      break;
  }
}

void Scene::serialize() {
  if (m_serializer == nullptr) {
    return;
  }

  SceneArtifactKind artifact_kind = SceneArtifactKind::Source;
  switch (m_session_kind) {
    case SceneSessionKind::Source:
      artifact_kind = SceneArtifactKind::Source;
      break;
    case SceneSessionKind::Preview:
      artifact_kind = SceneArtifactKind::Preview;
      break;
    case SceneSessionKind::Runtime:
      artifact_kind = SceneArtifactKind::Runtime;
      break;
  }

  m_serializer->set_artifact_kind(artifact_kind);
  m_serializer->serialize();
}

ecs::EntityRef Scene::spawn_scene_entity(std::string name, bool active) {
  auto entity = spawn_entity(std::move(name), active);
  entity.emplace<scene::SceneEntity>();
  return entity;
}

void Scene::refresh_source_overlay() {
  if (m_session_kind != SceneSessionKind::Source) {
    return;
  }

  SceneBuildContext ctx(m_world, &m_derived_state);
  evaluate_build(ctx);
  ctx.apply();
}

void Scene::capture_source_derived_state_from_overlay() {
  if (m_session_kind != SceneSessionKind::Source) {
    return;
  }

  m_serializer->set_artifact_kind(SceneArtifactKind::Source);
  auto authored_source_snapshots = m_serializer->collect_artifact_snapshots(
      m_world
  );
  ecs::World baseline_world;
  serialization::apply_scene_snapshots(
      baseline_world, authored_source_snapshots
  );

  SceneBuildContext baseline_ctx(baseline_world);
  evaluate_build(baseline_ctx);
  baseline_ctx.apply();

  const auto baseline_entities =
      collect_derived_entity_snapshot_map(baseline_world);
  const auto actual_entities = collect_derived_entity_snapshot_map(m_world);

  DerivedState derived_state;
  for (const auto &[lookup_key, baseline_entity] : baseline_entities) {
    const auto key = derived_entity_key(baseline_entity);
    if (!key.has_value()) {
      continue;
    }

    const auto actual_it = actual_entities.find(lookup_key);
    if (actual_it == actual_entities.end()) {
      derived_state.suppressions.push_back(
          DerivedSuppressionRecord{.key = *key}
      );
      continue;
    }

    auto override_record = build_derived_override_record(
        *key, baseline_entity, actual_it->second
    );
    if (override_record.has_value()) {
      derived_state.overrides.push_back(std::move(*override_record));
    }
  }

  const auto record_less = [](const auto &lhs, const auto &rhs) {
    if (lhs.key.generator_id != rhs.key.generator_id) {
      return lhs.key.generator_id < rhs.key.generator_id;
    }
    return lhs.key.stable_key < rhs.key.stable_key;
  };
  std::sort(
      derived_state.overrides.begin(),
      derived_state.overrides.end(),
      record_less
  );
  std::sort(
      derived_state.suppressions.begin(),
      derived_state.suppressions.end(),
      record_less
  );

  m_derived_state = std::move(derived_state);
}

void Scene::save_source() {
  if (m_serializer == nullptr || m_session_kind != SceneSessionKind::Source) {
    return;
  }

  capture_source_derived_state_from_overlay();
  m_serializer->set_artifact_kind(SceneArtifactKind::Source);
  m_serializer->serialize_world(m_world);
  write_scene_buffer(*m_serializer, scene_path(*this, SceneArtifactKind::Source));
  m_last_source_save_revision = m_world.revision();
  m_has_source_save_revision = true;
}

void Scene::save_preview() {
  if (m_serializer == nullptr || m_session_kind != SceneSessionKind::Preview) {
    return;
  }

  m_serializer->set_artifact_kind(SceneArtifactKind::Preview);
  m_serializer->serialize_world(m_world);
  write_scene_buffer(*m_serializer, scene_path(*this, SceneArtifactKind::Preview));
  remember_loaded_artifact(SceneArtifactKind::Preview);
}

void Scene::save_runtime() {
  if (m_serializer == nullptr || m_session_kind != SceneSessionKind::Runtime) {
    return;
  }

  m_serializer->set_artifact_kind(SceneArtifactKind::Runtime);
  m_serializer->serialize_world(m_world);
  write_scene_buffer(*m_serializer, scene_path(*this, SceneArtifactKind::Runtime));
  remember_loaded_artifact(SceneArtifactKind::Runtime);
}

bool Scene::load_artifact(SceneArtifactKind artifact_kind) {
  if (m_serializer == nullptr) {
    return false;
  }

  const auto path = scene_path(*this, artifact_kind);
  if (!std::filesystem::exists(path)) {
    return false;
  }

  auto ctx = m_serializer->get_ctx();
  if (ctx == nullptr) {
    return false;
  }

  auto reader = FileStreamReader(path);
  reader.read();
  m_serializer->set_artifact_kind(artifact_kind);
  ctx->from_buffer(reader.get_buffer());
  m_serializer->deserialize();

  if (artifact_kind == SceneArtifactKind::Source) {
    refresh_source_overlay();
    m_last_source_save_revision = m_world.revision();
    m_has_source_save_revision = true;
  }

  remember_loaded_artifact(artifact_kind);
  mark_world_ready(true);
  mark_session_reloaded();
  return true;
}

bool Scene::load_source() {
  return load_artifact(SceneArtifactKind::Source);
}

bool Scene::load_preview() {
  return load_artifact(SceneArtifactKind::Preview);
}

bool Scene::load_runtime() {
  return load_artifact(SceneArtifactKind::Runtime);
}

void Scene::build_preview(bool persist_to_disk) {
  if (m_serializer == nullptr || m_session_kind != SceneSessionKind::Source) {
    return;
  }

  capture_source_derived_state_from_overlay();
  m_serializer->set_artifact_kind(SceneArtifactKind::Source);
  auto source_snapshots = m_serializer->collect_artifact_snapshots(m_world);
  ecs::World preview_world;
  serialization::apply_scene_snapshots(preview_world, source_snapshots);

  SceneBuildContext build_ctx(preview_world, &m_derived_state);
  evaluate_build(build_ctx);
  build_ctx.apply();

  m_preview_build_info = ScenePreviewBuildInfo{
      .source_revision = m_world.revision(),
      .built_at_utc = utc_timestamp_now(),
  };
  m_serializer->set_artifact_kind(SceneArtifactKind::Preview);
  m_serializer->serialize_world(preview_world);
  if (persist_to_disk) {
    write_scene_buffer(*m_serializer, scene_path(*this, SceneArtifactKind::Preview));
  }
  m_last_preview_build_revision = m_world.revision();
  m_has_preview_build_revision = true;
}

bool Scene::promote_preview_to_runtime(bool persist_to_disk) {
  if (m_serializer == nullptr || m_session_kind != SceneSessionKind::Preview) {
    return false;
  }

  m_runtime_promotion_info = SceneRuntimePromotionInfo{
      .promoted_from_preview_revision =
          m_preview_build_info.has_value() ? m_preview_build_info->source_revision
                                           : 0u,
      .promoted_at_utc = utc_timestamp_now(),
  };
  m_serializer->set_artifact_kind(SceneArtifactKind::Runtime);
  m_serializer->serialize_world(m_world);
  if (persist_to_disk) {
    write_scene_buffer(*m_serializer, scene_path(*this, SceneArtifactKind::Runtime));
  }
  m_last_runtime_promotion_revision =
      m_preview_build_info.has_value() ? m_preview_build_info->source_revision
                                       : 0u;
  m_has_runtime_promotion_revision = true;
  return true;
}

void Scene::remember_loaded_artifact(SceneArtifactKind artifact_kind) {
  const auto write_time = artifact_write_time(*this, artifact_kind);
  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      break;
    case SceneArtifactKind::Preview:
      m_loaded_preview_write_time = write_time;
      break;
    case SceneArtifactKind::Runtime:
      m_loaded_runtime_write_time = write_time;
      break;
  }
}

bool Scene::reload_artifact_if_changed(SceneArtifactKind artifact_kind) {
  const auto current_write_time = artifact_write_time(*this, artifact_kind);
  if (!current_write_time.has_value()) {
    return false;
  }

  const auto *loaded_write_time =
      artifact_kind == SceneArtifactKind::Preview
          ? &m_loaded_preview_write_time
          : &m_loaded_runtime_write_time;

  if (loaded_write_time->has_value() &&
      *loaded_write_time == current_write_time) {
    return false;
  }

  return load_artifact(artifact_kind);
}

bool Scene::reload_preview_if_changed() {
  return reload_artifact_if_changed(SceneArtifactKind::Preview);
}

bool Scene::reload_runtime_if_changed() {
  return reload_artifact_if_changed(SceneArtifactKind::Runtime);
}

void Scene::play() {
  if (!supports_execution_controls()) {
    m_pending_reset = false;
    m_execution_state = SceneExecutionState::Static;
    return;
  }

  m_execution_state = SceneExecutionState::Playing;
}

void Scene::pause() {
  if (!supports_execution_controls()) {
    m_pending_reset = false;
    m_execution_state = SceneExecutionState::Static;
    return;
  }

  m_execution_state = SceneExecutionState::Paused;
}

bool Scene::stop() {
  if (!supports_execution_controls()) {
    m_pending_reset = false;
    m_execution_state = SceneExecutionState::Static;
    return false;
  }

  m_pending_reset = true;
  m_execution_state = SceneExecutionState::Stopped;
  return true;
}

bool Scene::flush_pending_reset() {
  if (!m_pending_reset) {
    return false;
  }

  if (!reload_current_session_from_serializer()) {
    return false;
  }

  m_pending_reset = false;
  return true;
}

bool Scene::reload_current_session_from_serializer() {
  if (!supports_execution_controls() || m_serializer == nullptr ||
      m_serializer->get_ctx() == nullptr) {
    return false;
  }

  const auto artifact_kind = m_session_kind == SceneSessionKind::Preview
                                 ? SceneArtifactKind::Preview
                                 : SceneArtifactKind::Runtime;
  m_serializer->set_artifact_kind(artifact_kind);
  m_serializer->deserialize();
  mark_world_ready(true);
  mark_session_reloaded();

  if (m_session_kind == SceneSessionKind::Preview) {
    after_preview_ready();
  } else {
    after_runtime_ready();
  }

  return true;
}

void Scene::mark_session_reloaded() { ++m_session_revision; }

} // namespace astralix
