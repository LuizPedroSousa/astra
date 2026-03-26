#pragma once

#include "astralix/modules/renderer/entities/scene.hpp"
#include <glm/glm.hpp>
#include <guid.hpp>

using namespace astralix;

class Prologue : public Scene {
public:
  Prologue();

  void start() override;
  void update() override;

private:
  EntityID m_fps_text_entity;
  EntityID m_entities_text_count;
  EntityID m_bodies_text_count;
  float m_fps_elapsed = 0.0f;
  uint32_t m_fps_frame_count = 0u;
};
