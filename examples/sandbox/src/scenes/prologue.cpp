#include "prologue.hpp"

#include <managers/window-manager.hpp>

#include "astralix/modules/physics/components/collider.hpp"
#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/renderer/components/camera.hpp"
#include "astralix/modules/renderer/components/light.hpp"
#include "astralix/modules/renderer/components/material.hpp"
#include "astralix/modules/renderer/components/mesh.hpp"
#include "astralix/modules/renderer/components/skybox.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/components/transform.hpp"
#include "astralix/modules/renderer/resources/mesh.hpp"
#include "astralix/modules/window/time.hpp"
#include "glm/gtx/quaternion.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <components/text.hpp>
#include <vector>

namespace {

const glm::vec3 arena_offset(0.0f, 1.0f, 0.0f);
const glm::vec3 camera_local_position(-14.0f, 10.0f, 14.0f);
const glm::vec3 camera_local_target(0.0f, 2.0f, 0.0f);
const glm::vec3 sun_local_position(-8.0f, 12.0f, -6.0f);
const glm::vec3 sun_color(1.0f);
constexpr float sun_intensity = 1.0f;
constexpr float sun_shadow_ortho_extent = 28.0f;
constexpr float sun_shadow_near_plane = 0.5f;
constexpr float sun_shadow_far_plane = 80.0f;

constexpr float hud_line_spacing = 50.0f;
constexpr int hud_line_count = 4;
constexpr float hud_text_x = 32.0f;
const glm::vec3 hud_text_color(0.85f, 1.0f, 0.8f);
const glm::vec3 hud_controls_color(0.9f, 0.9f, 1.0f);
constexpr const char *fps_text_label = "FPS: --";
constexpr const char *entities_count_text_label = "Entities Count: --";
constexpr const char *bodies_text_label = "Bodies: --";
constexpr const char *controls_text_label =
    "Press F5 to reset the scene | Physics live press F6 pause";
constexpr const char *fps_text_prefix = "FPS: ";
constexpr const char *entities_count_text_prefix = "Entities Count: ";
constexpr const char *bodies_text_prefix = "Bodies: ";
constexpr float fps_update_interval = 0.25f;

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

constexpr float cube_mesh_size = 1.0f;
constexpr float default_cube_mass = 1.0f;
constexpr float minimum_collider_half_extent = 0.05f;
const glm::vec3 ramp_rotation_axis(0.0f, 0.0f, 1.0f);

struct cube_spawn_spec {
  const char *name;
  glm::vec3 position;
  glm::vec3 scale;
  float mass = default_cube_mass;
  glm::vec3 rotation_axis = glm::vec3(0.0f);
  float rotation_degrees = 0.0f;
};

const std::array<cube_spawn_spec, 2> ramp_specs{{
    {
        .name = "arena_ramp_left",
        .position = glm::vec3(-3.75f, 0.9f, -1.5f),
        .scale = glm::vec3(4.0f, 0.5f, 2.5f),
        .mass = default_cube_mass,
        .rotation_axis = ramp_rotation_axis,
        .rotation_degrees = -25.0f,
    },
    {
        .name = "arena_ramp_right",
        .position = glm::vec3(3.5f, 0.9f, 2.5f),
        .scale = glm::vec3(3.5f, 0.5f, 2.0f),
        .mass = default_cube_mass,
        .rotation_axis = ramp_rotation_axis,
        .rotation_degrees = 22.0f,
    },
}};

const std::array<cube_spawn_spec, 9> dynamic_cube_specs{{
    {
        .name = "drop_cube_left_0",
        .position = glm::vec3(-4.75f, 4.5f, -1.5f),
        .scale = glm::vec3(0.8f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_left_1",
        .position = glm::vec3(-4.5f, 6.0f, -1.0f),
        .scale = glm::vec3(0.8f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_left_2",
        .position = glm::vec3(-4.0f, 7.5f, -1.8f),
        .scale = glm::vec3(0.8f),
        .mass = 1.2f,
    },
    {
        .name = "stack_cube_center_0",
        .position = glm::vec3(0.0f, 1.0f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_1",
        .position = glm::vec3(0.0f, 2.25f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_2",
        .position = glm::vec3(0.0f, 3.5f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_3",
        .position = glm::vec3(1.1f, 1.0f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_4",
        .position = glm::vec3(1.1f, 2.25f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_right_heavy",
        .position = glm::vec3(3.75f, 6.5f, 2.5f),
        .scale = glm::vec3(1.15f),
        .mass = 2.5f,
    },
}};

glm::vec3 normalize_or_default(glm::vec3 axis,
                               glm::vec3 fallback = glm::vec3(0.0f, 1.0f,
                                                              0.0f)) {
  if (glm::length(axis) == 0.0f) {
    return fallback;
  }

  return glm::normalize(axis);
}

void rotate_transform(astralix::scene::Transform &transform, glm::vec3 axis,
                      float degrees) {
  const glm::vec3 normalized_axis = normalize_or_default(axis);
  const glm::quat delta =
      glm::angleAxis(glm::radians(degrees), normalized_axis);

  transform.rotation = glm::normalize(delta * transform.rotation);
  transform.dirty = true;
}

astralix::scene::Transform make_transform(glm::vec3 position,
                                          glm::vec3 scale = glm::vec3(1.0f)) {
  return astralix::scene::Transform{
      .position = position,
      .scale = scale,
      .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      .matrix = glm::mat4(1.0f),
      .dirty = true,
  };
}

astralix::rendering::Camera make_camera(glm::vec3 position, glm::vec3 target) {
  const glm::vec3 front = glm::normalize(target - position);

  astralix::rendering::Camera camera;
  camera.front = front;
  camera.direction = front;
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  return camera;
}

astralix::rendering::TextSprite
make_text(std::string value, glm::vec2 position = glm::vec2(32.0f, 64.0f),
          glm::vec3 color = glm::vec3(1.0f)) {
  astralix::rendering::TextSprite text;
  text.text = value;
  text.font_id = "fonts::roboto";
  text.position = position;
  text.color = color;
  return text;
}

astralix::scene::CameraController make_camera_controller(glm::vec3 front) {
  front = glm::normalize(front);

  astralix::scene::CameraController controller;
  controller.mode = astralix::scene::CameraControllerMode::Free;
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

  auto fps_text = spawn_entity("fps");
  auto entities_count_text = spawn_entity("entities_count");
  auto bodies_text = spawn_entity("bodies");
  auto controls_text = spawn_entity("controls");

  auto text_y = [&](int line) {
    return (hud_line_count - 1 - line) * hud_line_spacing + hud_line_spacing;
  };

  fps_text.emplace<scene::SceneEntity>();
  fps_text.emplace<scene::Transform>();
  fps_text.emplace<rendering::TextSprite>(
      make_text(fps_text_label, glm::vec2(hud_text_x, text_y(0)),
                hud_text_color));

  entities_count_text.emplace<scene::SceneEntity>();
  entities_count_text.emplace<scene::Transform>();
  entities_count_text.emplace<rendering::TextSprite>(
      make_text(entities_count_text_label, glm::vec2(hud_text_x, text_y(1)),
                hud_text_color));

  bodies_text.emplace<scene::SceneEntity>();
  bodies_text.emplace<scene::Transform>();
  bodies_text.emplace<rendering::TextSprite>(make_text(
      bodies_text_label, glm::vec2(hud_text_x, text_y(2)), hud_text_color));

  controls_text.emplace<scene::SceneEntity>();
  controls_text.emplace<scene::Transform>();
  controls_text.emplace<rendering::TextSprite>(
      make_text(controls_text_label, glm::vec2(hud_text_x, text_y(3)),
                hud_controls_color));

  m_entities_text_count = entities_count_text.id();
  m_bodies_text_count = bodies_text.id();
  m_fps_text_entity = fps_text.id();
  m_fps_elapsed = 0.0f;
  m_fps_frame_count = 0u;

  camera.emplace<scene::SceneEntity>();
  camera.emplace<rendering::MainCamera>();
  camera.emplace<scene::Transform>(make_transform(camera_position));
  camera.emplace<rendering::Camera>(camera_component);
  camera.emplace<scene::CameraController>(
      make_camera_controller(camera_component.front));

  auto directional_light = spawn_entity("directional_light");
  directional_light.emplace<scene::SceneEntity>();
  directional_light.emplace<scene::Transform>(
      make_transform(sun_local_position + arena_offset));
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
      });

  const Mesh cube_mesh = Mesh::cube(cube_mesh_size);

  auto spawn_cube = [&](const char *name, glm::vec3 position, glm::vec3 scale,
                        physics::RigidBodyMode mode, float mass = default_cube_mass,
                        glm::vec3 rotation_axis = glm::vec3(0.0f),
                        float rotation_degrees = 0.0f) {
    auto cube = spawn_entity(name);
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
        rendering::ShaderBinding{.shader = "shaders::lighting_forward"});
    cube.emplace<rendering::MaterialSlots>(
        rendering::MaterialSlots{.materials = {"materials::wood"}});
    cube.emplace<physics::RigidBody>(
        physics::RigidBody{.mode = mode, .mass = mass});
    cube.emplace<physics::BoxCollider>(physics::BoxCollider{
        .half_extents = glm::max(scale * 0.5f,
                                 glm::vec3(minimum_collider_half_extent)),
        .center = glm::vec3(0.0f),
    });
  };

  const float inner_wall_z =
      inner_floor_scale.z * 0.5f + inner_wall_thickness * 0.5f;
  const float inner_wall_x =
      inner_floor_scale.x * 0.5f + inner_wall_thickness * 0.5f;
  const float outer_wall_z =
      outer_floor_scale.z * 0.5f + outer_wall_thickness * 0.5f;
  const float outer_wall_x =
      outer_floor_scale.x * 0.5f + outer_wall_thickness * 0.5f;

  spawn_cube("arena_floor", inner_floor_position, inner_floor_scale,
             physics::RigidBodyMode::Static);
  spawn_cube(
      "arena_wall_north", glm::vec3(0.0f, inner_wall_center_y, -inner_wall_z),
      glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "arena_wall_south", glm::vec3(0.0f, inner_wall_center_y, inner_wall_z),
      glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "arena_wall_west", glm::vec3(-inner_wall_x, inner_wall_center_y, 0.0f),
      glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "arena_wall_east", glm::vec3(inner_wall_x, inner_wall_center_y, 0.0f),
      glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z),
      physics::RigidBodyMode::Static);

  spawn_cube("outer_floor", outer_floor_position, outer_floor_scale,
             physics::RigidBodyMode::Static);
  spawn_cube(
      "outer_wall_north", glm::vec3(0.0f, outer_wall_center_y, -outer_wall_z),
      glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "outer_wall_south", glm::vec3(0.0f, outer_wall_center_y, outer_wall_z),
      glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "outer_wall_west", glm::vec3(-outer_wall_x, outer_wall_center_y, 0.0f),
      glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z),
      physics::RigidBodyMode::Static);
  spawn_cube(
      "outer_wall_east", glm::vec3(outer_wall_x, outer_wall_center_y, 0.0f),
      glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z),
      physics::RigidBodyMode::Static);

  for (const auto &ramp_spec : ramp_specs) {
    spawn_cube(ramp_spec.name, ramp_spec.position, ramp_spec.scale,
               physics::RigidBodyMode::Static, ramp_spec.mass,
               ramp_spec.rotation_axis, ramp_spec.rotation_degrees);
  }

  for (const auto &cube_spec : dynamic_cube_specs) {
    spawn_cube(cube_spec.name, cube_spec.position, cube_spec.scale,
               physics::RigidBodyMode::Dynamic, cube_spec.mass,
               cube_spec.rotation_axis, cube_spec.rotation_degrees);
  }

  auto skybox = spawn_entity("skybox");
  skybox.emplace<scene::SceneEntity>();
  skybox.emplace<rendering::SkyboxBinding>(rendering::SkyboxBinding{
      .cubemap = "cubemaps::skybox",
      .shader = "shaders::skybox",
  });
}

void Prologue::update() {
  auto &scene_world = world();
  const float dt = astralix::Time::get() != nullptr
                       ? astralix::Time::get()->get_deltatime()
                       : 0.0f;

  if (input::IS_KEY_RELEASED(input::KeyCode::F5)) {
    std::vector<EntityID> entity_ids;
    entity_ids.reserve(scene_world.count<scene::SceneEntity>());

    scene_world.each<scene::SceneEntity>(
        [&](EntityID entity_id, scene::SceneEntity &) {
          entity_ids.push_back(entity_id);
        });

    for (EntityID entity_id : entity_ids) {
      scene_world.destroy(entity_id);
    }

    start();
    return;
  }

  m_fps_elapsed += dt;
  m_fps_frame_count++;

  if (m_fps_elapsed >= fps_update_interval &&
      scene_world.contains(m_fps_text_entity)) {
    auto fps_text = scene_world.entity(m_fps_text_entity);
    auto *sprite = fps_text.get<rendering::TextSprite>();
    if (sprite != nullptr) {
      const float fps =
          m_fps_elapsed > 0.0f
              ? static_cast<float>(m_fps_frame_count) / m_fps_elapsed
              : 0.0f;
      sprite->text = std::string(fps_text_prefix) + std::to_string(std::lround(fps));
    }

    m_fps_elapsed = 0.0f;
    m_fps_frame_count = 0u;
  }

  if (scene_world.contains(m_entities_text_count)) {
    auto entities_count_text = scene_world.entity(m_entities_text_count);
    auto *sprite = entities_count_text.get<rendering::TextSprite>();

    if (sprite != nullptr) {
      sprite->text = std::string(entities_count_text_prefix) +
                     std::to_string(scene_world.count<rendering::Renderable>());
    }
  }

  if (scene_world.contains(m_bodies_text_count)) {
    auto bodies_text = scene_world.entity(m_bodies_text_count);
    auto *sprite = bodies_text.get<rendering::TextSprite>();

    if (sprite != nullptr) {
      sprite->text = std::string(bodies_text_prefix) +
                     std::to_string(scene_world.count<physics::RigidBody>());
    }
  }
}
