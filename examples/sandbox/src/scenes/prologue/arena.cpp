#include "arena.hpp"

#include "astralix/modules/physics/components/collider.hpp"
#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/renderer/components/material.hpp"
#include "astralix/modules/renderer/components/mesh.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/modules/renderer/components/transform.hpp"
#include "astralix/modules/renderer/resources/mesh.hpp"
#include "glm/gtx/quaternion.hpp"

using namespace astralix;

const glm::vec3 arena_offset(0.0f, 1.0f, 0.0f);

namespace {

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

const glm::vec3 ramp_rotation_axis(0.0f, 0.0f, 1.0f);

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

} // namespace

const std::array<cube_spawn_spec, 2> ramp_specs{{
    {
        .name = "arena_ramp_left",
        .position = glm::vec3(-3.75f, 0.9f, -1.5f),
        .scale = glm::vec3(4.0f, 0.5f, 2.5f),
        .mass = 1.0f,
        .rotation_axis = ramp_rotation_axis,
        .rotation_degrees = -25.0f,
    },
    {
        .name = "arena_ramp_right",
        .position = glm::vec3(3.5f, 0.9f, 2.5f),
        .scale = glm::vec3(3.5f, 0.5f, 2.0f),
        .mass = 1.0f,
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

astralix::scene::Transform make_transform(glm::vec3 position, glm::vec3 scale) {
  return astralix::scene::Transform{
      .position = position,
      .scale = scale,
      .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      .matrix = glm::mat4(1.0f),
      .dirty = true,
  };
}

void spawn_arena_cube(Scene &scene, std::string name, glm::vec3 position,
                      glm::vec3 scale, physics::RigidBodyMode mode, float mass,
                      glm::vec3 rotation_axis, float rotation_degrees) {
  const Mesh cube_mesh = Mesh::cube(cube_mesh_size);

  auto cube = scene.spawn_entity(std::move(name));
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
      .half_extents =
          glm::max(scale * 0.5f, glm::vec3(minimum_collider_half_extent)),
      .center = glm::vec3(0.0f),
  });
}

void init_arena(Scene &scene) {
  const float inner_wall_z =
      inner_floor_scale.z * 0.5f + inner_wall_thickness * 0.5f;
  const float inner_wall_x =
      inner_floor_scale.x * 0.5f + inner_wall_thickness * 0.5f;
  const float outer_wall_z =
      outer_floor_scale.z * 0.5f + outer_wall_thickness * 0.5f;
  const float outer_wall_x =
      outer_floor_scale.x * 0.5f + outer_wall_thickness * 0.5f;

  spawn_arena_cube(scene, "arena_floor", inner_floor_position,
                   inner_floor_scale, physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "arena_wall_north",
      glm::vec3(0.0f, inner_wall_center_y, -inner_wall_z),
      glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "arena_wall_south",
      glm::vec3(0.0f, inner_wall_center_y, inner_wall_z),
      glm::vec3(inner_floor_scale.x, inner_wall_height, inner_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "arena_wall_west",
      glm::vec3(-inner_wall_x, inner_wall_center_y, 0.0f),
      glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "arena_wall_east",
      glm::vec3(inner_wall_x, inner_wall_center_y, 0.0f),
      glm::vec3(inner_wall_thickness, inner_wall_height, inner_floor_scale.z),
      physics::RigidBodyMode::Static);

  spawn_arena_cube(scene, "outer_floor", outer_floor_position,
                   outer_floor_scale, physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "outer_wall_north",
      glm::vec3(0.0f, outer_wall_center_y, -outer_wall_z),
      glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "outer_wall_south",
      glm::vec3(0.0f, outer_wall_center_y, outer_wall_z),
      glm::vec3(outer_floor_scale.x, outer_wall_height, outer_wall_thickness),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "outer_wall_west",
      glm::vec3(-outer_wall_x, outer_wall_center_y, 0.0f),
      glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z),
      physics::RigidBodyMode::Static);
  spawn_arena_cube(
      scene, "outer_wall_east",
      glm::vec3(outer_wall_x, outer_wall_center_y, 0.0f),
      glm::vec3(outer_wall_thickness, outer_wall_height, outer_floor_scale.z),
      physics::RigidBodyMode::Static);

  for (const auto &ramp_spec : ramp_specs) {
    spawn_arena_cube(scene, ramp_spec.name, ramp_spec.position, ramp_spec.scale,
                     physics::RigidBodyMode::Static, ramp_spec.mass,
                     ramp_spec.rotation_axis, ramp_spec.rotation_degrees);
  }

  for (const auto &cube_spec : dynamic_cube_specs) {
    spawn_arena_cube(scene, cube_spec.name, cube_spec.position, cube_spec.scale,
                     physics::RigidBodyMode::Dynamic, cube_spec.mass,
                     cube_spec.rotation_axis, cube_spec.rotation_degrees);
  }
}
