#include "render_benchmark.hpp"

#include "commands.hpp"
#include "shared/spawn.hpp"

#include "astralix/modules/renderer/components/camera.hpp"
#include "astralix/modules/renderer/components/light.hpp"
#include "astralix/modules/renderer/components/model.hpp"
#include "astralix/modules/renderer/components/skybox.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/entities/scene-build-context.hpp"
#include "astralix/modules/window/managers/window-manager.hpp"
#include "astralix/shared/foundation/console.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace astralix;

namespace {

constexpr char k_bistro_model_id[] = "models::bistro_exterior";
constexpr char k_sponza_curtains_id[] = "models::sponza_curtains";

const glm::vec3 benchmark_camera_position(0.0f, 5.5f, 14.0f);
const glm::vec3 benchmark_camera_target(0.0f, 4.0f, 0.0f);
const glm::vec3 benchmark_sun_position(-10.0f, 14.0f, -6.0f);
const glm::vec3 benchmark_sun_color(1.0f);
const glm::vec3 benchmark_sponza_position(0.0f, 0.0f, 0.0f);
const glm::vec3 benchmark_sponza_scale(1.0f);
const glm::vec3 benchmark_bistro_position(-19.0f, 0.0f, 8.0f);
const glm::vec3 benchmark_bistro_scale(0.01f);

constexpr float benchmark_sun_intensity = 1.15f;
constexpr float benchmark_shadow_ortho_extent = 24.0f;
constexpr float benchmark_shadow_near_plane = 0.5f;
constexpr float benchmark_shadow_far_plane = 64.0f;

rendering::Camera make_camera(glm::vec3 position, glm::vec3 target) {
  const glm::vec3 front = glm::normalize(target - position);

  rendering::Camera camera;
  camera.front = front;
  camera.direction = front;
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.far_plane = 96.0f;
  return camera;
}

scene::CameraController make_camera_controller(glm::vec3 front) {
  front = glm::normalize(front);

  scene::CameraController controller;
  controller.mode = scene::CameraControllerMode::Free;
  controller.speed = 8.0f;
  controller.sensitivity = 0.1f;
  controller.yaw = glm::degrees(std::atan2(front.z, front.x));
  controller.pitch =
      glm::degrees(std::asin(std::clamp(front.y, -1.0f, 1.0f)));
  return controller;
}

} // namespace

RenderBenchmark::RenderBenchmark() : Scene("sandbox.render_benchmark") {}

void RenderBenchmark::setup() {
  register_console_commands(*this);
}

void RenderBenchmark::build_source_world() { m_should_reset_scene = false; }

void RenderBenchmark::after_preview_ready() { m_should_reset_scene = false; }

void RenderBenchmark::after_runtime_ready() { m_should_reset_scene = false; }

void RenderBenchmark::evaluate_build(SceneBuildContext &ctx) {
  auto pass = ctx.begin_pass("sandbox.render_benchmark");

  auto camera_component =
      make_camera(benchmark_camera_position, benchmark_camera_target);

  auto camera = pass.entity("camera", "camera");
  camera.component(rendering::MainCamera{});
  camera.component(make_transform(benchmark_camera_position));
  camera.component(camera_component);
  camera.component(make_camera_controller(camera_component.front));

  auto directional_light = pass.entity("directional_light", "directional_light");
  directional_light.component(make_transform(benchmark_sun_position));
  directional_light.component(rendering::Light{
      .type = rendering::LightType::Directional,
      .color = benchmark_sun_color,
      .intensity = benchmark_sun_intensity,
      .casts_shadows = true,
  });
  directional_light.component(rendering::DirectionalShadowSettings{
      .ortho_extent = benchmark_shadow_ortho_extent,
      .near_plane = benchmark_shadow_near_plane,
      .far_plane = benchmark_shadow_far_plane,
  });

  auto skybox = pass.entity("skybox", "skybox");
  skybox.component(rendering::SkyboxBinding{
      .cubemap = "cubemaps::skybox",
      .shader = "shaders::skybox",
  });

  const bool use_bistro = m_active_model_id == k_bistro_model_id;
  const auto model_position = use_bistro ? benchmark_bistro_position : benchmark_sponza_position;
  const auto model_scale = use_bistro ? benchmark_bistro_scale : benchmark_sponza_scale;

  auto model_transform = make_transform(model_position, model_scale);
  if (use_bistro) {
    rotate_transform(model_transform, glm::vec3(1.0f, 0.0f, 0.0f), -90.0f);
  }

  auto scene_model = pass.entity("scene_model", "scene_model");
  scene_model.component(rendering::Renderable{});
  scene_model.component(rendering::ShadowCaster{});
  scene_model.component(model_transform);
  scene_model.component(
      rendering::ModelRef{.resource_ids = {m_active_model_id}}
  );
  scene_model.component(
      rendering::ShaderBinding{.shader = "shaders::g_buffer"}
  );

  if (!use_bistro) {
    auto curtains = pass.entity("curtains", "curtains");
    curtains.component(rendering::Renderable{});
    curtains.component(rendering::ShadowCaster{});
    curtains.component(model_transform);
    curtains.component(
        rendering::ModelRef{.resource_ids = {k_sponza_curtains_id}}
    );
    curtains.component(
        rendering::ShaderBinding{.shader = "shaders::g_buffer"}
    );
  }
}

void RenderBenchmark::update_runtime() {
  auto &console_manager = ConsoleManager::get();

  if (!console_manager.captures_input() &&
      input::IS_KEY_RELEASED(input::KeyCode::F5)) {
    m_should_reset_scene = true;
  }

  if (!m_should_reset_scene) {
    return;
  }

  switch (get_session_kind()) {
    case SceneSessionKind::Preview:
      (void)load_preview();
      after_preview_ready();
      break;

    case SceneSessionKind::Runtime:
      (void)load_runtime();
      after_runtime_ready();
      break;

    case SceneSessionKind::Source:
      break;
  }
}
