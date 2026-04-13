#include "scene-extraction-pass.hpp"
#include "components/audio-emitter.hpp"
#include "components/audio-listener.hpp"
#include "components/camera.hpp"
#include "components/transform.hpp"
#include "managers/scene-manager.hpp"

namespace astralix::audio {

void SceneExtractionPass::process(AudioFrame &frame, AudioBackend &backend) {
  (void)backend;
  (void)SceneManager::get()->flush_pending_active_scene_state();

  auto *active_scene = SceneManager::get()->get_active_scene();
  if (active_scene == nullptr) {
    frame.scene_state = {.active = false};
    m_tracked_scene = nullptr;
    m_tracked_revision = 0;
    return;
  }

  bool valid_session =
      active_scene->get_session_kind() == SceneSessionKind::Preview ||
      active_scene->get_session_kind() == SceneSessionKind::Runtime;

  if (!valid_session) {
    frame.scene_state = {.active = false};
    m_tracked_scene = nullptr;
    m_tracked_revision = 0;
    return;
  }

  bool scene_changed = (m_tracked_scene != active_scene) ||
                       (m_tracked_revision != active_scene->get_session_revision());

  m_tracked_scene = active_scene;
  m_tracked_revision = active_scene->get_session_revision();

  frame.scene_state = {
      .active = true,
      .playing = active_scene->is_playing(),
      .session_revision = m_tracked_revision,
      .scene_changed = scene_changed,
  };

  auto &world = active_scene->world();

  bool listener_found = false;
  world.each<scene::Transform, AudioListener>(
      [&](EntityID entity_id, scene::Transform &transform, AudioListener &listener) {
        if (!listener.enabled || !world.active(entity_id)) {
          return;
        }

        if (listener_found) {
          LOG_WARN("[SCENE EXTRACTION] multiple enabled AudioListeners, using first");
          return;
        }
        listener_found = true;

        frame.listener.position = transform.position;
        frame.listener.gain = listener.gain;
        frame.listener.valid = true;

        auto entity = world.entity(entity_id);
        auto *camera = entity.get<rendering::Camera>();

        if (camera != nullptr) {
          frame.listener.forward = camera->front;
          frame.listener.up = camera->up;
        } else {
          frame.listener.forward =
              glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
          frame.listener.up =
              glm::normalize(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        }
      });

  world.each<scene::Transform, AudioEmitter>(
      [&](EntityID entity_id, scene::Transform &transform, AudioEmitter &emitter) {
        if (!emitter.enabled || !world.active(entity_id)) {
          return;
        }

        frame.emitters.push_back({
            .entity_id = entity_id,
            .position = transform.position,
            .clip_id = emitter.clip_id,
            .enabled = emitter.enabled,
            .play_on_awake = emitter.play_on_awake,
            .looping = emitter.looping,
            .spatial = emitter.spatial,
            .gain = emitter.gain,
            .pitch = emitter.pitch,
            .min_distance = emitter.min_distance,
            .max_distance = emitter.max_distance,
            .rolloff = emitter.rolloff,
        });
      });
}

} // namespace astralix::audio
