#include "prologue.hpp"

#include "arena.hpp"
#include "commands.hpp"
#include "hud.hpp"

#include <managers/window-manager.hpp>

#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/renderer/components/camera.hpp"
#include "astralix/modules/renderer/components/light.hpp"
#include "astralix/modules/renderer/components/skybox.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/components/transform.hpp"
#include "astralix/modules/ui/components/ui.hpp"
#include "astralix/modules/window/time.hpp"
#include "astralix/shared/foundation/console.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace astralix;

namespace {

const glm::vec3 camera_local_position(-14.0f, 10.0f, 14.0f);
const glm::vec3 camera_local_target(0.0f, 2.0f, 0.0f);
const glm::vec3 sun_local_position(-8.0f, 12.0f, -6.0f);
const glm::vec3 sun_color(1.0f);
constexpr float sun_intensity = 1.0f;
constexpr float sun_shadow_ortho_extent = 28.0f;
constexpr float sun_shadow_near_plane = 0.5f;
constexpr float sun_shadow_far_plane = 80.0f;

rendering::Camera make_camera(glm::vec3 position, glm::vec3 target) {
  const glm::vec3 front = glm::normalize(target - position);

  rendering::Camera camera;
  camera.front = front;
  camera.direction = front;
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  return camera;
}

scene::CameraController make_camera_controller(glm::vec3 front) {
  front = glm::normalize(front);

  scene::CameraController controller;
  controller.mode = scene::CameraControllerMode::Free;
  controller.speed = 8.0f;
  controller.sensitivity = 0.1f;
  controller.yaw = glm::degrees(std::atan2(front.z, front.x));
  controller.pitch = glm::degrees(std::asin(std::clamp(front.y, -1.0f, 1.0f)));
  return controller;
}

} // namespace

Prologue::Prologue() : Scene("prologue") {}

void Prologue::start() {
  auto camera = spawn_entity("camera");
  const glm::vec3 camera_position = camera_local_position + arena_offset;
  const glm::vec3 camera_target = camera_local_target + arena_offset;
  auto camera_component = make_camera(camera_position, camera_target);

  auto hud = spawn_entity("hud");
  hud.emplace<scene::SceneEntity>();

  const auto hud_document_state = build_hud_document(this);

  hud.emplace<rendering::UIRoot>(rendering::UIRoot{
      .document = hud_document_state.document,
      .default_font_id = "fonts::roboto",
      .default_font_size = 18.0f,
      .sort_order = 100,
      .input_enabled = true,
      .visible = true,
  });

  m_hud_document = hud_document_state.document;
  m_fps_text_node = hud_document_state.fps;
  m_entities_text_node = hud_document_state.entities;
  m_bodies_text_node = hud_document_state.bodies;
  m_resizable_split_demo_node = hud_document_state.resizable_split_demo;
  m_fps_elapsed = 0.0f;
  m_fps_frame_count = 0u;
  m_spawn_cube_requests = 0u;

  m_console.init({
      .document = hud_document_state.document,
      .root = hud_document_state.console_root,
      .filters_row = hud_document_state.console_settings,
      .severity = hud_document_state.console_severity,
      .source_filters = hud_document_state.console_sources,
      .log_scroll = hud_document_state.console_log_scroll,
      .input = hud_document_state.console_input,
  });
  m_console.reset();

  register_console_commands(*this);

  camera.emplace<scene::SceneEntity>();
  camera.emplace<rendering::MainCamera>();
  camera.emplace<scene::Transform>(make_transform(camera_position));
  camera.emplace<rendering::Camera>(camera_component);
  camera.emplace<scene::CameraController>(
      make_camera_controller(camera_component.front)
  );

  auto directional_light = spawn_entity("directional_light");
  directional_light.emplace<scene::SceneEntity>();
  directional_light.emplace<scene::Transform>(
      make_transform(sun_local_position + arena_offset)
  );
  directional_light.emplace<rendering::Light>(rendering::Light{
      .type = rendering::LightType::Directional,
      .color = sun_color,
      .intensity = sun_intensity,
      .casts_shadows = true,
  });
  directional_light.emplace<rendering::DirectionalShadowSettings>(
      rendering::DirectionalShadowSettings{
          .ortho_extent = sun_shadow_ortho_extent,
          .near_plane = sun_shadow_near_plane,
          .far_plane = sun_shadow_far_plane,
      }
  );

  init_arena(*this);

  auto skybox = spawn_entity("skybox");
  skybox.emplace<scene::SceneEntity>();
  skybox.emplace<rendering::SkyboxBinding>(rendering::SkyboxBinding{
      .cubemap = "cubemaps::skybox",
      .shader = "shaders::skybox",
  });
}

void Prologue::update() {
  auto &scene_world = world();
  const float dt = Time::get() != nullptr ? Time::get()->get_deltatime() : 0.0f;
  auto window = window_manager()->active_window();
  auto &console_manager = ConsoleManager::get();

  if (input::IS_KEY_RELEASED(input::KeyCode::GraveAccent)) {
    m_console.set_open(!console_manager.is_open());
  } else if (console_manager.is_open() && input::IS_KEY_RELEASED(input::KeyCode::Escape)) {
    m_console.set_open(false);
  }

  if (!console_manager.captures_input() &&
      input::IS_KEY_RELEASED(input::KeyCode::F5)) {
    m_should_reset_scene = true;
  }

  if (m_request_toggle_split_view) {
    m_is_resizable_split_view_open = !m_is_resizable_split_view_open;
    m_hud_document->set_visible(m_resizable_split_demo_node, m_is_resizable_split_view_open);
    m_request_toggle_split_view = false;
  }

  if (m_should_reset_scene) {
    std::vector<EntityID> entity_ids;
    entity_ids.reserve(scene_world.count<scene::SceneEntity>());

    scene_world.each<scene::SceneEntity>(
        [&](EntityID entity_id, scene::SceneEntity &) {
          entity_ids.push_back(entity_id);
        }
    );

    for (EntityID entity_id : entity_ids) {
      scene_world.destroy(entity_id);
    }

    start();
    m_should_reset_scene = false;
    return;
  }

  m_fps_elapsed += dt;
  m_fps_frame_count++;

  if (m_hud_document != nullptr) {
    if (m_fps_elapsed >= fps_update_interval) {
      const float fps =
          m_fps_elapsed > 0.0f
              ? static_cast<float>(m_fps_frame_count) / m_fps_elapsed
              : 0.0f;
      m_hud_document->set_text(m_fps_text_node, std::string(fps_text_prefix) + std::to_string(std::lround(fps)));

      m_fps_elapsed = 0.0f;
      m_fps_frame_count = 0u;
    }

    m_hud_document->set_text(
        m_entities_text_node,
        std::string(entities_count_text_prefix) +
            std::to_string(scene_world.count<rendering::Renderable>())
    );

    m_hud_document->set_text(
        m_bodies_text_node,
        std::string(bodies_text_prefix) +
            std::to_string(scene_world.count<physics::RigidBody>())
    );
  }

  m_console.update();

  while (m_spawn_cube_requests > 0u) {
    const std::string cube_name =
        "ui_spawned_cube_" + std::to_string(m_spawned_cube_count++);
    const float offset = static_cast<float>(m_spawned_cube_count % 5u) * 0.85f;
    spawn_arena_cube(*this, cube_name, glm::vec3(-2.0f + offset, 8.0f, 1.5f), glm::vec3(0.85f), physics::RigidBodyMode::Dynamic, 1.0f);
    --m_spawn_cube_requests;
  }
}
