#pragma once

#include "astralix/modules/renderer/entities/scene.hpp"
#include <glm/glm.hpp>
#include <guid.hpp>

using namespace astralix;

class RenderBenchmark : public Scene {
public:
  RenderBenchmark();

  void start() override;
  void update() override;
  void request_spawn_cube(uint32_t count = 1u) {
    m_spawn_cube_requests += count;
  }
  void request_reset_scene() { m_should_reset_scene = true; }

private:
  uint32_t m_spawn_cube_requests = 0u;
  bool m_should_reset_scene = false;
  uint32_t m_spawned_cube_count = 0u;
};
