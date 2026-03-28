#pragma once

#include "console.hpp"

#include "astralix/modules/renderer/entities/scene.hpp"
#include "astralix/modules/ui/document.hpp"
#include <glm/glm.hpp>
#include <guid.hpp>

using namespace astralix;

class Prologue : public Scene {
public:
  Prologue();

  void start() override;
  void update() override;
  void request_spawn_cube(uint32_t count = 1u) {
    m_spawn_cube_requests += count;
  }
  void request_reset_scene() { m_should_reset_scene = true; }
  void request_toggle_resizable_split_view() {
    m_request_toggle_split_view = true;
  }

  ConsoleController &console() { return m_console; }

private:
  ConsoleController m_console;

  Ref<ui::UIDocument> m_hud_document = nullptr;
  ui::UINodeId m_fps_text_node = ui::k_invalid_node_id;
  ui::UINodeId m_entities_text_node = ui::k_invalid_node_id;
  ui::UINodeId m_bodies_text_node = ui::k_invalid_node_id;
  ui::UINodeId m_resizable_split_demo_node = ui::k_invalid_node_id;
  float m_fps_elapsed = 0.0f;
  uint32_t m_fps_frame_count = 0u;
  uint32_t m_spawn_cube_requests = 0u;
  bool m_should_reset_scene = false;
  uint32_t m_spawned_cube_count = 0u;
  bool m_request_toggle_split_view = false;
  bool m_is_resizable_split_view_open = false;
};
