

#include "arena.hpp"
#include "commands.hpp"
#include "shared/spawn.hpp"

#include "astralix/modules/renderer/components/camera.hpp"
#include "astralix/modules/renderer/components/light.hpp"
#include "astralix/modules/renderer/components/skybox.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/components/transform.hpp"
#include "astralix/shared/foundation/console.hpp"

#include "astralix/modules/physics/components/collider.hpp"
#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/renderer/components/material.hpp"
#include "astralix/modules/renderer/components/mesh.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/resources/mesh.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <managers/window-manager.hpp>
#include <resources/mesh.hpp>
#include <string_view>
#include <string>
#include <vector>

using namespace astralix;

namespace {
const glm::vec3 arena_offset(0.0f, 1.0f, 0.0f);
constexpr float cube_mesh_size = 1.0f;
constexpr float minimum_collider_half_extent = 0.05f;

const glm::vec3 inner_floor_position(0.0f, -0.25f, 0.0f);
const glm::vec3 inner_floor_scale(14.0f, 0.5f, 14.0f);
constexpr float inner_wall_height = 4.0f;
constexpr float inner_wall_thickness = 0.5f;
constexpr float inner_wall_center_y = 2.0f;

const glm::vec3 outer_floor_position(0.0f, -0.75f, 0.0f);
const glm::vec3 outer_floor_scale(40.0f, 0.5f, 40.0f);
constexpr float outer_wall_height = 15.0f;
constexpr float outer_wall_thickness = 1.0f;
constexpr float outer_wall_center_y = 2.75f;

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

Arena::Arena() : Scene("sandbox.arena") {}

void Arena::setup() {
  register_console_commands(*this);
}

void Arena::build_default_world() {
  auto camera = spawn_entity("camera");
  const glm::vec3 camera_position = camera_local_position + arena_offset;
  const glm::vec3 camera_target = camera_local_target + arena_offset;
  auto camera_component = make_camera(camera_position, camera_target);

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

  spawn();

  auto skybox = spawn_entity("skybox");
  skybox.emplace<scene::SceneEntity>();
  skybox.emplace<rendering::SkyboxBinding>(rendering::SkyboxBinding{
      .cubemap = "cubemaps::skybox",
      .shader = "shaders::skybox",
  });
}

void Arena::after_world_ready() {
  constexpr std::string_view spawned_cube_prefix = "ui_spawned_cube_";

  m_spawn_cube_requests = 0u;
  m_should_reset_scene = false;
  m_spawned_cube_count = 0u;

  world().each<scene::SceneEntity>([&](EntityID entity_id, scene::SceneEntity &) {
    auto entity = world().entity(entity_id);
    const std::string name(entity.name());

    if (!name.starts_with(spawned_cube_prefix)) {
      return;
    }

    const std::string suffix(name.substr(spawned_cube_prefix.size()));
    if (suffix.empty()) {
      m_spawned_cube_count = std::max(m_spawned_cube_count, 1u);
      return;
    }

    uint32_t restored_index = 0u;
    const char *suffix_begin = suffix.data();
    const char *suffix_end = suffix_begin + suffix.size();
    auto parse_result =
        std::from_chars(suffix_begin, suffix_end, restored_index);

    if (parse_result.ec == std::errc{} && parse_result.ptr == suffix_end) {
      const auto restored_count = restored_index + 1u;
      m_spawned_cube_count = std::max(m_spawned_cube_count, restored_count);
    } else {
      m_spawned_cube_count = std::max(m_spawned_cube_count, 1u);
    }
  });
}

void Arena::update() {
  auto &scene_world = world();
  auto &console_manager = ConsoleManager::get();

  if (!console_manager.captures_input() &&
      input::IS_KEY_RELEASED(input::KeyCode::F5)) {
    m_should_reset_scene = true;
  }

  if (m_should_reset_scene) {
    scene_world = ecs::World();
    build_default_world();
    after_world_ready();
    return;
  }

  while (m_spawn_cube_requests > 0u) {
    const std::string cube_name =
        "ui_spawned_cube_" + std::to_string(m_spawned_cube_count++);
    const float offset = static_cast<float>(m_spawned_cube_count % 5u) * 0.85f;
    spawn_arena_cube(cube_name, glm::vec3(-2.0f + offset, 8.0f, 1.5f), glm::vec3(0.85f), physics::RigidBodyMode::Dynamic, 1.0f);
    --m_spawn_cube_requests;
  }
}

void Arena::spawn() {
  const float inner_wall_z =
      inner_floor_scale.z * 0.5f + inner_wall_thickness * 0.5f;
  const float inner_wall_x =
      inner_floor_scale.x * 0.5f + inner_wall_thickness * 0.5f;
  const float outer_wall_z =
      outer_floor_scale.z * 0.5f + outer_wall_thickness * 0.5f;
  const float outer_wall_x =
      outer_floor_scale.x * 0.5f + outer_wall_thickness * 0.5f;

  spawn_arena_cube("arena_floor", inner_floor_position, inner_floor_scale, physics::RigidBodyMode::Static);
  spawn_arena_cube(
      "arena_wall_north", glm::vec3(0.0f, inner_wall_center_y, -inner_wall_z), glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "arena_wall_south", glm::vec3(0.0f, inner_wall_center_y, inner_wall_z), glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "arena_wall_west", glm::vec3(-inner_wall_x, inner_wall_center_y, 0.0f), glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "arena_wall_east", glm::vec3(inner_wall_x, inner_wall_center_y, 0.0f), glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z), physics::RigidBodyMode::Static
  );

  spawn_arena_cube("outer_floor", outer_floor_position, outer_floor_scale, physics::RigidBodyMode::Static);
  spawn_arena_cube(
      "outer_wall_north", glm::vec3(0.0f, outer_wall_center_y, -outer_wall_z), glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "outer_wall_south", glm::vec3(0.0f, outer_wall_center_y, outer_wall_z), glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "outer_wall_west", glm::vec3(-outer_wall_x, outer_wall_center_y, 0.0f), glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z), physics::RigidBodyMode::Static
  );
  spawn_arena_cube(
      "outer_wall_east", glm::vec3(outer_wall_x, outer_wall_center_y, 0.0f), glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z), physics::RigidBodyMode::Static
  );

  for (const auto &ramp_spec : ramp_specs) {
    spawn_arena_cube(ramp_spec.name, ramp_spec.position, ramp_spec.scale, physics::RigidBodyMode::Static, ramp_spec.mass, ramp_spec.rotation_axis, ramp_spec.rotation_degrees);
  }

  for (const auto &cube_spec : dynamic_cube_specs) {
    spawn_arena_cube(cube_spec.name, cube_spec.position, cube_spec.scale, physics::RigidBodyMode::Dynamic, cube_spec.mass, cube_spec.rotation_axis, cube_spec.rotation_degrees);
  }
}

void Arena::spawn_arena_cube(std::string name, glm::vec3 position, glm::vec3 scale, physics::RigidBodyMode mode, float mass, glm::vec3 rotation_axis, float rotation_degrees) {
  const Mesh cube_mesh = Mesh::cube(cube_mesh_size);

  auto cube = spawn_entity(std::move(name));
  auto transform = make_transform(position + arena_offset, scale);

  if (glm::length(rotation_axis) > 0.0f && rotation_degrees != 0.0f) {
    rotate_transform(transform, rotation_axis, rotation_degrees);
  }

  cube.emplace<scene::SceneEntity>();
  cube.emplace<rendering::Renderable>();
  cube.emplace<rendering::ShadowCaster>();
  cube.emplace<scene::Transform>(transform);
  cube.emplace<rendering::MeshSet>(rendering::MeshSet{.meshes = {cube_mesh}});
  cube.emplace<rendering::ShaderBinding>(
      rendering::ShaderBinding{.shader = "shaders::deffered"}
  );
  cube.emplace<rendering::MaterialSlots>(
      rendering::MaterialSlots{.materials = {"materials::wood"}}
  );
  cube.emplace<physics::RigidBody>(
      physics::RigidBody{.mode = mode, .mass = mass}
  );
  cube.emplace<physics::BoxCollider>(physics::BoxCollider{
      .half_extents =
          glm::max(scale * 0.5f, glm::vec3(minimum_collider_half_extent)),
      .center = glm::vec3(0.0f),
  });
}
