#pragma once

#include "astralix/modules/renderer/entities/scene.hpp"
#include <components/rigidbody.hpp>
#include <glm/glm.hpp>
#include <guid.hpp>

using namespace astralix;

class Arena : public Scene {
public:
  Arena();

  void request_spawn_cube(uint32_t count = 1u) {
    m_spawn_cube_requests += count;
  }
  void request_reset_scene() { m_should_reset_scene = true; }

private:
  void setup() override;
  void update_runtime() override;
  void build_source_world() override;
  void after_source_ready() override;
  void after_runtime_ready() override;
  void evaluate_build(SceneBuildContext &ctx) override;
  void spawn();
  void sync_spawned_cube_state();
  void spawn_arena_cube(std::string name, glm::vec3 position, glm::vec3 scale, physics::RigidBodyMode mode, float mass = 1.0f, glm::vec3 rotation_axis = glm::vec3(0.0f), float rotation_degrees = 0.0f);

  uint32_t m_spawn_cube_requests = 0u;
  bool m_should_reset_scene = false;
  uint32_t m_spawned_cube_count = 0u;
};
