#pragma once

#include "components/ui.hpp"
#include "events/key-event.hpp"
#include "events/mouse-event.hpp"
#include "systems/system.hpp"
#include "systems/ui-system/core.hpp"
#include <array>
#include <optional>
#include <vector>

namespace astralix {

namespace ecs {
class World;
}

class Scene;

class UISystem : public System<UISystem> {
public:
  using Target = ui_system_core::Target;

  struct PanelResizeDrag {
    Target target;
    ui::UIHitPart part = ui::UIHitPart::Body;
    glm::vec2 start_pointer = glm::vec2(0.0f);
    ui::UIRect start_bounds;
  };

  struct PanelMoveDrag {
    Target panel_target;
    Target handle_target;
    glm::vec2 start_pointer = glm::vec2(0.0f);
    ui::UIRect start_bounds;
  };

  struct SplitterResizeDrag {
    Target target;
    glm::vec2 start_pointer = glm::vec2(0.0f);
    ui::UINodeId previous_node_id = ui::k_invalid_node_id;
    ui::UINodeId next_node_id = ui::k_invalid_node_id;
    float previous_start_size = 0.0f;
    float next_start_size = 0.0f;
    ui::FlexDirection parent_direction = ui::FlexDirection::Row;
  };

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  const std::optional<PanelMoveDrag> &active_panel_move_drag() const {
    return m_panel_move_drag;
  }
  const std::optional<PanelResizeDrag> &active_panel_resize_drag() const {
    return m_panel_resize_drag;
  }
  std::optional<PanelMoveDrag> &active_panel_move_drag_mut() {
    return m_panel_move_drag;
  }

  glm::vec2 last_pointer_position() const { return m_last_pointer_position; }
  bool has_last_pointer_position() const { return m_has_last_pointer_position; }
  bool keyboard_focus_captures_editor_shortcuts() const;

private:
  struct SecondaryClickPress {
    Target target;
    glm::vec2 pointer = glm::vec2(0.0f);
    input::KeyModifiers modifiers;
  };

  struct TextSelectionDrag {
    Target target;
    size_t anchor_index = 0u;
  };

  struct ScrollbarDrag {
    Target target;
    ui::UIHitPart part = ui::UIHitPart::Body;
    float grab_offset = 0.0f;
  };

  struct SliderDrag {
    Target target;
  };

  struct PointerButtonState {
    std::optional<Target> pressed_target;
    std::optional<Target> capture_target;
    std::optional<Target> last_routed_target;
    ui::UIHitPart pressed_part = ui::UIHitPart::Body;
    std::optional<size_t> pressed_item_index;
    std::optional<ui::UICustomHitData> pressed_custom;
    glm::vec2 press_pointer = glm::vec2(0.0f);
    glm::vec2 last_pointer = glm::vec2(0.0f);
    bool drag_active = false;
    bool view_transform_pan = false;
  };

  struct QueuedKeyInput {
    ui::UIKeyInputEvent event;
  };

  struct QueuedCharacterInput {
    ui::UICharacterInputEvent event;
  };

  struct QueuedMouseWheelInput {
    ui::UIMouseWheelInputEvent event;
  };

  std::vector<ui_system_core::RootEntry>
  gather_root_entries(ecs::World &world, const glm::vec2 &viewport_size) const;
  void process_programmatic_focus(
      const std::vector<ui_system_core::RootEntry> &roots,
      const glm::vec2 &viewport_size
  );
  void validate_targets(const std::vector<ui_system_core::RootEntry> &roots, bool scene_changed, bool pointer_enabled);
  std::optional<ui_system_core::PointerHit>
  hit_test(const std::vector<ui_system_core::RootEntry> &roots, bool pointer_enabled, glm::vec2 &pointer) const;
  void process_pointer_capture_requests(
      const std::vector<ui_system_core::RootEntry> &roots
  );
  void dispatch_pointer_motion(
      const std::vector<ui_system_core::RootEntry> &roots,
      bool pointer_enabled,
      const std::optional<ui_system_core::PointerHit> &deepest_hit,
      const glm::vec2 &pointer
  );
  void
  update_active_drags(const std::vector<ui_system_core::RootEntry> &roots, bool pointer_enabled, const glm::vec2 &pointer, const glm::vec2 &viewport_size);
  void resolve_hot_target(
      const std::vector<ui_system_core::RootEntry> &roots,
      const std::optional<ui_system_core::PointerHit> &deepest_hit
  );
  void handle_pointer_press(
      const std::vector<ui_system_core::RootEntry> &roots,
      const std::optional<ui_system_core::PointerHit> &deepest_hit,
      bool pointer_enabled, const glm::vec2 &pointer,
      const glm::vec2 &viewport_size
  );
  void handle_pointer_release(
      const std::vector<ui_system_core::RootEntry> &roots,
      bool pointer_enabled,
      const std::optional<ui_system_core::PointerHit> &deepest_hit,
      const glm::vec2 &pointer
  );
  void dispatch_key_input(const std::vector<ui_system_core::RootEntry> &roots, const glm::vec2 &viewport_size);
  void dispatch_character_input(
      const std::vector<ui_system_core::RootEntry> &roots,
      const glm::vec2 &viewport_size
  );
  void dispatch_scroll_input(
      const std::vector<ui_system_core::RootEntry> &roots,
      const std::optional<ui_system_core::PointerHit> &deepest_hit
  );
  void tick_caret_blink(double dt);
  void flush_and_relayout(const std::vector<ui_system_core::RootEntry> &roots, const glm::vec2 &viewport_size);
  void apply_visual_state(
      const std::vector<ui_system_core::RootEntry> &roots,
      const std::optional<ui_system_core::PointerHit> &deepest_hit
  );
  void resolve_cursor_icon(
      const std::vector<ui_system_core::RootEntry> &roots,
      const std::optional<ui_system_core::PointerHit> &deepest_hit,
      bool pointer_enabled
  ) const;
  void build_draw_lists(const std::vector<ui_system_core::RootEntry> &roots, const glm::vec2 &viewport_size) const;
  void clear_queued_inputs();

  void clear_hot_target();
  void clear_active_target(bool queue_release);
  void clear_focused_target(bool queue_blur);
  void clear_secondary_click_state();
  void clear_pointer_button_state(input::MouseButton button);
  void clear_text_selection_drag();
  void clear_scrollbar_drag();
  void clear_slider_drag();
  void clear_panel_move_drag();
  void clear_panel_resize_drag();
  void clear_splitter_resize_drag();
  void clear_drag_state();
  void clear_pointer_state();
  void set_focused_target(const std::optional<Target> &target);
  void dispatch_pointer_event(
      const ui_system_core::PointerHit &hit,
      ui::UIPointerEventPhase phase,
      const glm::vec2 &pointer,
      const glm::vec2 &delta,
      const glm::vec2 &total_delta,
      std::optional<input::MouseButton> button,
      input::KeyModifiers modifiers
  );

  std::optional<Target> m_hot_target;
  std::optional<Target> m_active_target;
  ui::UIHitPart m_active_item_part = ui::UIHitPart::Body;
  std::optional<size_t> m_active_item_index;
  std::optional<Target> m_focused_target;
  std::optional<SecondaryClickPress> m_secondary_click_press;
  std::optional<TextSelectionDrag> m_text_selection_drag;
  std::optional<ScrollbarDrag> m_scrollbar_drag;
  std::optional<SliderDrag> m_slider_drag;
  std::optional<PanelMoveDrag> m_panel_move_drag;
  std::optional<PanelResizeDrag> m_panel_resize_drag;
  std::optional<SplitterResizeDrag> m_splitter_resize_drag;
  std::array<PointerButtonState, static_cast<size_t>(input::MouseButton::Count)>
      m_pointer_button_states;
  std::optional<ui_system_core::PointerHit> m_last_pointer_move_hit;
  glm::vec2 m_last_pointer_position = glm::vec2(0.0f);
  bool m_has_last_pointer_position = false;
  std::vector<QueuedKeyInput> m_key_inputs;
  std::vector<QueuedCharacterInput> m_character_inputs;
  std::vector<QueuedMouseWheelInput> m_mouse_wheel_inputs;
  Scene *m_last_scene = nullptr;
  uint64_t m_last_scene_generation = 0u;
};

} // namespace astralix
