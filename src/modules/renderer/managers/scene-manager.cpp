#include "managers/scene-manager.hpp"

#include "arena.hpp"
#include "assert.hpp"
#include "bitwise.hpp"
#include "console.hpp"
#include "entities/serializers/scene-serializer.hpp"
#include "managers/project-manager.hpp"
#include "stream-buffer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace astralix {
namespace {

ConsoleCommandResult success_result(std::vector<std::string> lines = {}) {
  ConsoleCommandResult result;
  result.executed = true;
  result.success = true;
  result.lines = std::move(lines);
  return result;
}

ConsoleCommandResult error_result(std::string line) {
  ConsoleCommandResult result;
  result.executed = true;
  result.success = false;
  result.lines.push_back(std::move(line));
  return result;
}

bool is_flag_argument(std::string_view argument) {
  return argument.size() > 2u && argument[0] == '-' && argument[1] == '-';
}

std::optional<std::string> resolve_invocation_scene_id(
    SceneManager &scene_manager, const ConsoleCommandInvocation &invocation
) {
  if (!invocation.arguments.empty() &&
      !is_flag_argument(invocation.arguments.front())) {
    return invocation.arguments.front();
  }

  return scene_manager.get_active_scene_id();
}

std::string session_kind_label(SceneSessionKind kind) {
  switch (kind) {
    case SceneSessionKind::Source:
      return "source";
    case SceneSessionKind::Preview:
      return "preview";
    case SceneSessionKind::Runtime:
      return "runtime";
    default:
      ASTRA_EXCEPTION("Unknown scene session kind");
  }
}

Scope<StreamBuffer> clone_serialization_buffer(Ref<SerializationContext> ctx) {
  if (ctx == nullptr) {
    return nullptr;
  }

  ElasticArena arena(KB(64));
  auto *block = ctx->to_buffer(arena);
  if (block == nullptr) {
    return nullptr;
  }

  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

bool artifact_is_older(
    const std::filesystem::path &candidate_path,
    bool candidate_exists,
    const std::filesystem::path &reference_path,
    bool reference_exists
) {
  return candidate_exists && reference_exists &&
         std::filesystem::last_write_time(candidate_path) <
             std::filesystem::last_write_time(reference_path);
}

enum PreviewKeyBits : uint8_t {
  k_preview_has_source_session = 1u << 0,
  k_preview_current_in_memory = 1u << 1,
  k_preview_exists = 1u << 2,
  k_preview_has_session = 1u << 3,
  k_preview_source_dirty = 1u << 4,
  k_preview_older_than_source = 1u << 5,
};

constexpr ScenePreviewState preview_state_from_code(uint8_t code) noexcept {
  switch (code) {
    case 0u:
      return ScenePreviewState::Current;
    case 1u:
      return ScenePreviewState::Missing;
    case 2u:
      return ScenePreviewState::Stale;
    default:
      return ScenePreviewState::Current;
  }
}

consteval std::array<ScenePreviewState, 64> build_preview_state_lut() {
  std::array<ScenePreviewState, 64> lut{};

  for (size_t index = 0; index < lut.size(); ++index) {
    const uint8_t key = static_cast<uint8_t>(index);

    const uint8_t has_source_session =
        bit_is_set(key, k_preview_has_source_session);
    const uint8_t current_in_memory =
        bit_is_set(key, k_preview_current_in_memory);
    const uint8_t preview_exists = bit_is_set(key, k_preview_exists);
    const uint8_t has_preview_session =
        bit_is_set(key, k_preview_has_session);
    const uint8_t source_dirty = bit_is_set(key, k_preview_source_dirty);
    const uint8_t preview_older_than_source =
        bit_is_set(key, k_preview_older_than_source);

    const uint8_t missing_condition =
        not_one(preview_exists) & not_one(has_preview_session);

    const uint8_t stale_with_source_session =
        has_source_session &
        (has_preview_session | source_dirty | preview_older_than_source);

    const uint8_t stale_without_source_session =
        not_one(has_source_session) & preview_older_than_source;

    const uint8_t stale_condition =
        stale_with_source_session | stale_without_source_session;

    const uint8_t is_missing =
        not_one(current_in_memory) & missing_condition;

    const uint8_t is_stale =
        not_one(current_in_memory) & not_one(is_missing) & stale_condition;

    const uint8_t state_code =
        is_missing | static_cast<uint8_t>(is_stale << 1);

    lut[index] = preview_state_from_code(state_code);
  }

  return lut;
}

inline constexpr auto k_preview_state_lut = build_preview_state_lut();

constexpr uint8_t make_preview_key(
    bool has_source_session,
    bool current_in_memory,
    bool preview_exists,
    bool has_preview_session,
    bool source_dirty,
    bool preview_older_than_source
) noexcept {
  return static_cast<uint8_t>(
      (to_bit(has_source_session) << 0) | (to_bit(current_in_memory) << 1) |
      (to_bit(preview_exists) << 2) | (to_bit(has_preview_session) << 3) |
      (to_bit(source_dirty) << 4) |
      (to_bit(preview_older_than_source) << 5)
  );
}

enum RuntimeKeyBits : uint8_t {
  k_runtime_promoted_in_memory = 1u << 0,
  k_runtime_exists = 1u << 1,
  k_runtime_has_session = 1u << 2,
  k_runtime_source_has_current_preview = 1u << 3,
  k_runtime_source_has_current_promotion = 1u << 4,
  k_runtime_has_preview_session = 1u << 5,
  k_runtime_older_than_preview = 1u << 6,
};

constexpr SceneRuntimeState runtime_state_from_code(uint8_t code) noexcept {
  switch (code) {
    case 0u:
      return SceneRuntimeState::Promoted;
    case 1u:
      return SceneRuntimeState::Missing;
    case 2u:
      return SceneRuntimeState::BehindPreview;
    default:
      return SceneRuntimeState::Promoted;
  }
}

consteval std::array<SceneRuntimeState, 128> build_runtime_state_lut() {
  std::array<SceneRuntimeState, 128> lut{};

  for (size_t index = 0; index < lut.size(); ++index) {
    const uint8_t key = static_cast<uint8_t>(index);

    const uint8_t promoted_in_memory =
        bit_is_set(key, k_runtime_promoted_in_memory);
    const uint8_t runtime_exists = bit_is_set(key, k_runtime_exists);
    const uint8_t has_runtime_session =
        bit_is_set(key, k_runtime_has_session);
    const uint8_t source_has_current_preview =
        bit_is_set(key, k_runtime_source_has_current_preview);
    const uint8_t source_has_current_promotion =
        bit_is_set(key, k_runtime_source_has_current_promotion);
    const uint8_t has_preview_session =
        bit_is_set(key, k_runtime_has_preview_session);
    const uint8_t runtime_older_than_preview =
        bit_is_set(key, k_runtime_older_than_preview);

    const uint8_t missing_condition =
        not_one(runtime_exists) & not_one(has_runtime_session);

    const uint8_t behind_from_source_session =
        source_has_current_preview & not_one(source_has_current_promotion);

    const uint8_t behind_from_runtime_and_preview_sessions =
        has_runtime_session & has_preview_session;

    const uint8_t behind_without_source_session =
        not_one(source_has_current_preview) &
        (behind_from_runtime_and_preview_sessions |
         runtime_older_than_preview);

    const uint8_t behind_condition =
        behind_from_source_session | behind_without_source_session;

    const uint8_t is_missing =
        not_one(promoted_in_memory) & missing_condition;

    const uint8_t is_behind =
        not_one(promoted_in_memory) & not_one(is_missing) & behind_condition;

    const uint8_t state_code =
        is_missing | static_cast<uint8_t>(is_behind << 1);

    lut[index] = runtime_state_from_code(state_code);
  }

  return lut;
}

inline constexpr auto k_runtime_state_lut = build_runtime_state_lut();

constexpr uint8_t make_runtime_key(
    bool promoted_in_memory,
    bool runtime_exists,
    bool has_runtime_session,
    bool source_has_current_preview,
    bool source_has_current_promotion,
    bool has_preview_session,
    bool runtime_older_than_preview
) noexcept {
  return static_cast<uint8_t>(
      (to_bit(promoted_in_memory) << 0) | (to_bit(runtime_exists) << 1) |
      (to_bit(has_runtime_session) << 2) |
      (to_bit(source_has_current_preview) << 3) |
      (to_bit(source_has_current_promotion) << 4) |
      (to_bit(has_preview_session) << 5) |
      (to_bit(runtime_older_than_preview) << 6)
  );
}

} // namespace

SceneManager::SceneManager() { register_console_commands(); }

void SceneManager::register_scene_type(std::string type, SceneFactory factory) {
  ASTRA_ENSURE(type.empty(), "Scene type is required");
  ASTRA_ENSURE(!factory, "Scene factory is required for type ", type);

  m_scene_factories.insert_or_assign(std::move(type), std::move(factory));
}

void SceneManager::unregister_scene_type(std::string_view type) {
  m_scene_factories.erase(std::string(type));
}

void SceneManager::reset_scene_instances() {
  m_source_scene_instances.clear();
  m_preview_scene_instances.clear();
  m_runtime_scene_instances.clear();
  ++m_scene_instance_generation;
}

void SceneManager::set_scene_activation_enabled(bool enabled) {
  m_scene_activation_enabled = enabled;
}

uint64_t SceneManager::scene_instance_generation() const {
  return m_scene_instance_generation;
}

void SceneManager::clear_scene_state() {
  m_active_scene.reset();
  reset_scene_instances();
}

void SceneManager::ensure_project_state() {
  auto project = active_project();
  if (project == nullptr) {
    if (!m_loaded_project_id.has_value() && !m_active_scene.has_value() &&
        m_source_scene_instances.empty() &&
        m_preview_scene_instances.empty() &&
        m_runtime_scene_instances.empty()) {
      return;
    }

    m_loaded_project_id.reset();
    clear_scene_state();
    return;
  }

  if (m_loaded_project_id.has_value() &&
      *m_loaded_project_id == static_cast<uint64_t>(project->get_project_id())) {
    return;
  }

  validate_project_scenes(project->get_config());

  m_loaded_project_id = static_cast<uint64_t>(project->get_project_id());
  clear_scene_state();
}

void SceneManager::validate_project_scenes(const ProjectConfig &config) const {
  std::unordered_set<std::string> scene_ids;

  for (const auto &entry : config.scenes.entries) {
    ASTRA_ENSURE(entry.id.empty(), "Scene manifest entry id is required");
    ASTRA_ENSURE(entry.type.empty(), "Scene manifest entry type is required");
    ASTRA_ENSURE(entry.source_path.empty(), "Scene manifest entry source path is required");
    ASTRA_ENSURE(entry.preview_path.empty(), "Scene manifest entry preview path is required");
    ASTRA_ENSURE(entry.runtime_path.empty(), "Scene manifest entry runtime path is required");
    ASTRA_ENSURE(!scene_ids.insert(entry.id).second, "Duplicate scene manifest entry id: ", entry.id);
  }

  if (config.scenes.entries.empty()) {
    ASTRA_ENSURE(!config.scenes.startup.empty(), "Startup scene cannot be set when no scenes are declared");
    return;
  }

  ASTRA_ENSURE(config.scenes.startup.empty(), "Startup scene is required when project scenes are declared");

  bool startup_exists = false;
  for (const auto &entry : config.scenes.entries) {
    if (entry.id == config.scenes.startup) {
      startup_exists = true;
      break;
    }
  }

  ASTRA_ENSURE(!startup_exists, "Startup scene not found in manifest: ", config.scenes.startup);
}

SceneSessionKind
SceneManager::startup_session_kind(const ProjectConfig &config) const {
  switch (config.scenes.startup_target) {
    case SceneStartupTarget::Source:
      return SceneSessionKind::Source;
    case SceneStartupTarget::Preview:
      return SceneSessionKind::Preview;
    case SceneStartupTarget::Runtime:
      return SceneSessionKind::Runtime;
    default:
      ASTRA_EXCEPTION("Unknown startup session kind");
  }
}

Ref<Scene> SceneManager::instantiate_scene(
    const ProjectSceneEntryConfig &entry,
    SceneSessionKind kind,
    bool allow_missing_artifact
) {
  auto factory_it = m_scene_factories.find(entry.type);
  ASTRA_ENSURE(factory_it == m_scene_factories.end(), "No scene factory registered for type ", entry.type);

  Ref<Scene> scene = factory_it->second();
  ASTRA_ENSURE(scene == nullptr, "Scene factory returned null for type ", entry.type);

  scene->bind_to_manifest_entry(entry);
  scene->set_session_kind(kind);
  scene->set_serializer(create_ref<SceneSerializer>(scene));
  scene->ensure_setup();

  switch (kind) {
    case SceneSessionKind::Source:
      if (!scene->load_source()) {
        scene->world() = ecs::World();
        scene->build_source_world();
        scene->refresh_source_overlay();
        scene->mark_world_ready(true);
      }
      scene->after_source_ready();
      break;

    case SceneSessionKind::Preview:
      if (scene->load_preview()) {
        scene->after_preview_ready();
        break;
      }

      ASTRA_ENSURE(!allow_missing_artifact, "Preview scene file is missing: ", entry.preview_path);
      break;

    case SceneSessionKind::Runtime:
      if (scene->load_runtime()) {
        scene->after_runtime_ready();
        break;
      }

      ASTRA_ENSURE(!allow_missing_artifact, "Runtime scene file is missing: ", entry.runtime_path);
      break;
  }

  return scene;
}

Scene *SceneManager::ensure_instance(
    std::string_view scene_id,
    SceneSessionKind kind,
    bool allow_missing_artifact
) {
  if (!m_scene_activation_enabled) {
    return nullptr;
  }

  auto *entry = find_scene_entry(scene_id);
  ASTRA_ENSURE(entry == nullptr, "Unknown scene id: ", scene_id);

  auto *instances = &m_source_scene_instances;
  switch (kind) {
    case SceneSessionKind::Source:
      instances = &m_source_scene_instances;
      break;
    case SceneSessionKind::Preview:
      instances = &m_preview_scene_instances;
      break;
    case SceneSessionKind::Runtime:
      instances = &m_runtime_scene_instances;
      break;
  }

  auto instance_it = instances->find(std::string(scene_id));
  if (instance_it == instances->end()) {
    instance_it =
        instances
            ->emplace(
                std::string(scene_id),
                instantiate_scene(*entry, kind, allow_missing_artifact)
            )
            .first;
  }

  return instance_it->second.get();
}

bool SceneManager::sync_preview_session(
    std::string_view scene_id, bool persist_to_disk
) {
  Scene *source_scene = ensure_instance(scene_id, SceneSessionKind::Source);
  Scene *preview_scene =
      ensure_instance(scene_id, SceneSessionKind::Preview, true);
  if (source_scene == nullptr || preview_scene == nullptr) {
    return false;
  }

  source_scene->build_preview(persist_to_disk);

  auto source_serializer = source_scene->get_serializer();
  auto preview_serializer = preview_scene->get_serializer();
  if (source_serializer == nullptr || preview_serializer == nullptr) {
    return false;
  }

  auto preview_buffer =
      clone_serialization_buffer(source_serializer->get_ctx());
  if (preview_buffer == nullptr) {
    return false;
  }

  preview_serializer->set_artifact_kind(SceneArtifactKind::Preview);
  preview_serializer->get_ctx()->from_buffer(std::move(preview_buffer));
  if (!preview_scene->reload_current_session_from_serializer()) {
    return false;
  }
  if (persist_to_disk) {
    preview_scene->remember_loaded_artifact(SceneArtifactKind::Preview);
  }

  return true;
}

bool SceneManager::sync_runtime_session(
    std::string_view scene_id, bool persist_to_disk
) {
  Scene *preview_scene =
      ensure_instance(scene_id, SceneSessionKind::Preview, true);
  Scene *runtime_scene =
      ensure_instance(scene_id, SceneSessionKind::Runtime, true);
  if (preview_scene == nullptr || runtime_scene == nullptr ||
      !preview_scene->is_ready()) {
    return false;
  }

  auto preview_serializer = preview_scene->get_serializer();
  auto runtime_serializer = runtime_scene->get_serializer();
  if (preview_serializer == nullptr || runtime_serializer == nullptr) {
    return false;
  }

  auto preview_session_buffer =
      clone_serialization_buffer(preview_serializer->get_ctx());
  if (preview_session_buffer == nullptr) {
    return false;
  }

  if (!preview_scene->promote_preview_to_runtime(persist_to_disk)) {
    return false;
  }

  auto runtime_buffer =
      clone_serialization_buffer(preview_serializer->get_ctx());
  preview_serializer->set_artifact_kind(SceneArtifactKind::Preview);
  preview_serializer->get_ctx()->from_buffer(std::move(preview_session_buffer));
  if (runtime_buffer == nullptr) {
    return false;
  }

  runtime_serializer->set_artifact_kind(SceneArtifactKind::Runtime);
  runtime_serializer->get_ctx()->from_buffer(std::move(runtime_buffer));
  if (!runtime_scene->reload_current_session_from_serializer()) {
    return false;
  }
  if (persist_to_disk) {
    runtime_scene->remember_loaded_artifact(SceneArtifactKind::Runtime);
  }

  if (Scene *source_scene = ensure_instance(scene_id, SceneSessionKind::Source);
      source_scene != nullptr) {
    if (const auto &preview_info = preview_scene->get_preview_build_info();
        preview_info.has_value()) {
      source_scene->m_last_runtime_promotion_revision =
          preview_info->source_revision;
      source_scene->m_has_runtime_promotion_revision = true;
    } else {
      source_scene->m_last_runtime_promotion_revision = 0u;
      source_scene->m_has_runtime_promotion_revision = false;
    }
  }

  return true;
}

Scene *SceneManager::ensure_preview_session(std::string_view scene_id) {
  Scene *preview_scene =
      ensure_instance(scene_id, SceneSessionKind::Preview, true);
  if (preview_scene == nullptr) {
    return nullptr;
  }

  const bool was_ready = preview_scene->is_ready();
  if (!was_ready && !preview_scene->load_preview()) {
    return nullptr;
  }

  if (!was_ready) {
    preview_scene->after_preview_ready();
  } else {
    if (preview_scene->reload_preview_if_changed()) {
      preview_scene->after_preview_ready();
    }
  }

  return preview_scene;
}

Scene *SceneManager::ensure_runtime_session(std::string_view scene_id) {
  Scene *runtime_scene =
      ensure_instance(scene_id, SceneSessionKind::Runtime, true);
  if (runtime_scene == nullptr) {
    return nullptr;
  }

  const bool was_ready = runtime_scene->is_ready();
  if (!was_ready && !runtime_scene->load_runtime()) {
    return nullptr;
  }

  if (!was_ready) {
    runtime_scene->after_runtime_ready();
  } else {
    if (runtime_scene->reload_runtime_if_changed()) {
      runtime_scene->after_runtime_ready();
    }
  }

  return runtime_scene;
}

Scene *SceneManager::activate(std::string scene_id, SceneSessionKind kind) {
  ensure_project_state();

  if (!m_scene_activation_enabled) {
    return nullptr;
  }

  Scene *scene = nullptr;
  switch (kind) {
    case SceneSessionKind::Source:
      scene = ensure_instance(scene_id, kind);
      break;
    case SceneSessionKind::Preview:
      scene = ensure_preview_session(scene_id);
      break;
    case SceneSessionKind::Runtime:
      scene = ensure_runtime_session(scene_id);
      break;
  }

  if (scene != nullptr) {
    m_active_scene = ActiveSceneRef{.id = scene_id, .kind = kind};
  }

  return scene;
}

Scene *SceneManager::activate(std::string scene_id) {
  ensure_project_state();

  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "No active project loaded");
  return activate(std::move(scene_id), startup_session_kind(project->get_config()));
}

Scene *SceneManager::activate_source(std::string scene_id) {
  return activate(std::move(scene_id), SceneSessionKind::Source);
}

Scene *SceneManager::activate_preview(std::string scene_id) {
  return activate(std::move(scene_id), SceneSessionKind::Preview);
}

Scene *SceneManager::activate_runtime(std::string scene_id) {
  return activate(std::move(scene_id), SceneSessionKind::Runtime);
}

Scene *SceneManager::get_active_scene() {
  ensure_project_state();

  if (!m_active_scene.has_value()) {
    if (!m_scene_activation_enabled) {
      return nullptr;
    }

    auto project = active_project();
    if (project == nullptr || project->get_config().scenes.startup.empty()) {
      return nullptr;
    }

    return activate(project->get_config().scenes.startup);
  }

  const auto &active = *m_active_scene;
  auto *instances = &m_source_scene_instances;
  switch (active.kind) {
    case SceneSessionKind::Source:
      instances = &m_source_scene_instances;
      break;
    case SceneSessionKind::Preview:
      instances = &m_preview_scene_instances;
      break;
    case SceneSessionKind::Runtime:
      instances = &m_runtime_scene_instances;
      break;
  }

  auto it = instances->find(active.id);
  if (it != instances->end()) {
    return it->second.get();
  }

  if (!m_scene_activation_enabled) {
    return nullptr;
  }

  return activate(active.id, active.kind);
}

std::optional<std::string> SceneManager::get_active_scene_id() {
  ensure_project_state();

  if (!m_active_scene.has_value()) {
    (void)get_active_scene();
  }

  return m_active_scene.has_value() ? std::optional<std::string>(m_active_scene->id)
                                    : std::nullopt;
}

std::optional<SceneSessionKind> SceneManager::get_active_scene_session_kind() {
  ensure_project_state();

  if (!m_active_scene.has_value()) {
    (void)get_active_scene();
  }

  return m_active_scene.has_value()
             ? std::optional<SceneSessionKind>(m_active_scene->kind)
             : std::nullopt;
}

std::optional<SceneExecutionState>
SceneManager::get_active_scene_execution_state() {
  ensure_project_state();

  Scene *scene = get_active_scene();
  if (scene == nullptr) {
    return std::nullopt;
  }

  return scene->get_execution_state();
}

std::optional<SceneLifecycleStatus>
SceneManager::get_scene_lifecycle_status(std::string_view scene_id) {
  ensure_project_state();

  const auto *entry = find_scene_entry(scene_id);
  if (entry == nullptr) {
    return std::nullopt;
  }

  auto project = active_project();
  if (project == nullptr) {
    return std::nullopt;
  }

  const auto source_path = project->resolve_path(entry->source_path);
  const auto preview_path = project->resolve_path(entry->preview_path);
  const auto runtime_path = project->resolve_path(entry->runtime_path);
  const bool source_exists =
      !entry->source_path.empty() && std::filesystem::exists(source_path);
  const bool preview_exists =
      !entry->preview_path.empty() && std::filesystem::exists(preview_path);
  const bool runtime_exists =
      !entry->runtime_path.empty() && std::filesystem::exists(runtime_path);

  SceneLifecycleStatus status;

  auto source_it = m_source_scene_instances.find(std::string(scene_id));
  auto preview_it = m_preview_scene_instances.find(std::string(scene_id));
  auto runtime_it = m_runtime_scene_instances.find(std::string(scene_id));
  const Scene *source_scene =
      source_it != m_source_scene_instances.end() ? source_it->second.get()
                                                  : nullptr;
  const Scene *preview_scene =
      preview_it != m_preview_scene_instances.end() ? preview_it->second.get()
                                                    : nullptr;
  const Scene *runtime_scene =
      runtime_it != m_runtime_scene_instances.end() ? runtime_it->second.get()
                                                    : nullptr;
  const bool has_source_session = source_scene != nullptr;
  const bool has_preview_session =
      preview_scene != nullptr && preview_scene->is_ready();
  const bool has_runtime_session =
      runtime_scene != nullptr && runtime_scene->is_ready();
  const auto &preview_info =
      preview_scene != nullptr ? preview_scene->get_preview_build_info()
                               : std::optional<ScenePreviewBuildInfo>{};
  const auto &runtime_info =
      runtime_scene != nullptr ? runtime_scene->get_runtime_promotion_info()
                               : std::optional<SceneRuntimePromotionInfo>{};
  const bool source_dirty =
      source_scene != nullptr && source_scene->is_source_dirty();
  const bool preview_is_older_than_source =
      artifact_is_older(preview_path, preview_exists, source_path, source_exists);
  const bool runtime_is_older_than_preview =
      artifact_is_older(runtime_path, runtime_exists, preview_path, preview_exists);
  const bool preview_matches_source_revision =
      source_scene != nullptr && has_preview_session && preview_info.has_value() &&
      preview_info->source_revision == source_scene->world().revision();
  const bool source_has_current_preview_build =
      source_scene != nullptr && source_scene->has_current_preview_build();
  const bool source_has_current_runtime_promotion =
      source_scene != nullptr && source_scene->has_current_runtime_promotion();
  const bool preview_current_in_memory =
      source_has_current_preview_build || preview_matches_source_revision;

  if (source_scene != nullptr) {
    status.source =
        source_dirty ? SceneSourceSaveState::Dirty : SceneSourceSaveState::Saved;
  } else {
    status.source =
        source_exists ? SceneSourceSaveState::Saved : SceneSourceSaveState::Dirty;
  }

  const uint8_t preview_key = make_preview_key(
      has_source_session,
      preview_current_in_memory,
      preview_exists,
      has_preview_session,
      source_dirty,
      preview_is_older_than_source
  );
  status.preview = k_preview_state_lut[preview_key];

  const bool has_in_memory_runtime_promotion =
      has_runtime_session && runtime_info.has_value() && has_preview_session &&
      preview_info.has_value() &&
      runtime_info->promoted_from_preview_revision == preview_info->source_revision;

  const uint8_t runtime_key = make_runtime_key(
      has_in_memory_runtime_promotion,
      runtime_exists,
      has_runtime_session,
      source_has_current_preview_build,
      source_has_current_runtime_promotion,
      has_preview_session,
      runtime_is_older_than_preview
  );
  status.runtime = k_runtime_state_lut[runtime_key];

  return status;
}

std::optional<SceneLifecycleStatus>
SceneManager::get_active_scene_lifecycle_status() {
  ensure_project_state();

  const auto active_scene_id = get_active_scene_id();
  if (!active_scene_id.has_value()) {
    return std::nullopt;
  }

  return get_scene_lifecycle_status(*active_scene_id);
}

std::vector<ProjectSceneEntryConfig> SceneManager::get_scene_entries() {
  ensure_project_state();

  auto project = active_project();
  if (project == nullptr) {
    return {};
  }

  return project->get_config().scenes.entries;
}

const ProjectSceneEntryConfig *
SceneManager::find_scene_entry(std::string_view scene_id) {
  ensure_project_state();

  auto project = active_project();
  if (project == nullptr) {
    return nullptr;
  }

  return project->find_scene_entry(scene_id);
}

bool SceneManager::build_preview(std::string scene_id) {
  ensure_project_state();
  return sync_preview_session(scene_id, true);
}

bool SceneManager::promote_source_to_preview(std::string scene_id) {
  ensure_project_state();
  return sync_preview_session(scene_id, false);
}

bool SceneManager::promote_preview(std::string scene_id) {
  ensure_project_state();
  return sync_runtime_session(scene_id, false);
}

bool SceneManager::play_active_scene() {
  ensure_project_state();

  Scene *scene = get_active_scene();
  if (scene == nullptr || !scene->supports_execution_controls()) {
    return false;
  }

  scene->play();
  return true;
}

bool SceneManager::pause_active_scene() {
  ensure_project_state();

  Scene *scene = get_active_scene();
  if (scene == nullptr || !scene->supports_execution_controls()) {
    return false;
  }

  scene->pause();
  return true;
}

bool SceneManager::stop_active_scene() {
  ensure_project_state();

  Scene *scene = get_active_scene();
  if (scene == nullptr || !scene->supports_execution_controls()) {
    return false;
  }

  return scene->stop();
}

bool SceneManager::flush_pending_active_scene_state() {
  ensure_project_state();

  Scene *scene = get_active_scene();
  if (scene == nullptr || !scene->supports_execution_controls() ||
      !scene->has_pending_reset()) {
    return false;
  }

  return scene->flush_pending_reset();
}

void SceneManager::register_console_commands() {
  auto &console = ConsoleManager::get();
  console.register_command(
      "scene_activate",
      "Activate a scene using the project's default startup target.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_entries = get_scene_entries();
        if (scene_entries.empty()) {
          return error_result("no scenes declared in the active project");
        }

        if (invocation.arguments.empty()) {
          std::vector<std::string> lines;
          const auto active_scene_id = get_active_scene_id();
          const auto active_kind = get_active_scene_session_kind();

          for (const auto &entry : scene_entries) {
            const bool is_active =
                active_scene_id.has_value() && *active_scene_id == entry.id;
            lines.push_back(
                std::string(is_active ? "* " : "  ") + entry.id + " (" +
                entry.type + ", " +
                (is_active && active_kind.has_value()
                     ? session_kind_label(*active_kind)
                     : "inactive") +
                ")"
            );
          }

          return success_result(std::move(lines));
        }

        Scene *scene = activate(invocation.arguments.front());
        if (scene == nullptr) {
          return error_result("scene could not be activated for the configured startup target");
        }

        return success_result(
            {std::string("activated ") +
             session_kind_label(scene->get_session_kind()) + " scene " +
             scene->get_scene_id()}
        );
      },
      {"sa"}
  );

  console.register_command(
      "scene_activate_source",
      "Activate a scene's source session.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        Scene *scene = activate_source(*scene_id);
        if (scene == nullptr) {
          return error_result("source scene is not available");
        }

        return success_result({std::string("activated source scene ") + scene->get_scene_id()});
      },
      {"sas"}
  );

  console.register_command(
      "scene_activate_preview",
      "Activate a scene's preview session.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        Scene *scene = activate_preview(*scene_id);
        if (scene == nullptr) {
          return error_result("preview scene is not available");
        }

        return success_result({std::string("activated preview scene ") + scene->get_scene_id()});
      },
      {"sap"}
  );

  console.register_command(
      "scene_activate_runtime",
      "Activate a scene's runtime session from the promoted artifact.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        Scene *scene = activate_runtime(*scene_id);
        if (scene == nullptr) {
          return error_result("runtime scene is not available");
        }

        return success_result({std::string("activated runtime scene ") + scene->get_scene_id()});
      },
      {"sar"}
  );

  console.register_command(
      "scene_source_save",
      "Save a source scene to disk.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        Scene *scene = activate_source(*scene_id);
        if (scene == nullptr) {
          return error_result("source scene is not available");
        }

        scene->save_source();
        return success_result(
            {std::string("saved source scene ") + scene->get_scene_id()}
        );
      },
      {"sss"}
  );

  console.register_command(
      "scene_preview_save",
      "Save the current preview session to disk.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        Scene *scene = activate_preview(*scene_id);
        if (scene == nullptr) {
          return error_result("preview scene is not available");
        }

        scene->save_preview();
        return success_result(
            {std::string("saved preview scene ") + scene->get_scene_id()}
        );
      },
      {"sps"}
  );

  console.register_command(
      "scene_runtime_save",
      "Save the current runtime session to disk.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        const auto status = get_scene_lifecycle_status(*scene_id);
        if (!status.has_value() || status->runtime == SceneRuntimeState::Missing) {
          return error_result("runtime scene is not available");
        }

        Scene *scene = activate_runtime(*scene_id);
        if (scene == nullptr) {
          return error_result("runtime scene is not available");
        }

        scene->save_runtime();
        return success_result(
            {std::string("saved runtime scene ") + scene->get_scene_id()}
        );
      },
      {"srs"}
  );

  console.register_command(
      "scene_preview_build",
      "Build a source scene into its preview artifact.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        if (!build_preview(*scene_id)) {
          return error_result("preview build failed");
        }

        return success_result(
            {std::string("built preview for scene ") + *scene_id}
        );
      },
      {"spb", "sb"}
  );

  console.register_command(
      "scene_preview_promote",
      "Promote a preview artifact to runtime.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        if (!promote_preview(*scene_id)) {
          return error_result("preview scene is not available");
        }

        return success_result(
            {std::string("promoted preview for scene ") + *scene_id}
        );
      },
      {"spp"}
  );

  console.register_command(
      "scene_bake",
      "Compatibility alias for scene_preview_build.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_id = resolve_invocation_scene_id(*this, invocation);
        if (!scene_id.has_value()) {
          return error_result("no active scene");
        }

        if (!build_preview(*scene_id)) {
          return error_result("preview build failed");
        }

        return success_result(
            {std::string("built preview for scene ") + *scene_id}
        );
      }
  );
}

} // namespace astralix
