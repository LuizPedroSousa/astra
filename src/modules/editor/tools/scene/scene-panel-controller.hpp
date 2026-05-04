#pragma once

#include "managers/scene-manager.hpp"
#include "panels/panel-controller.hpp"

#include <optional>
#include <string>

namespace astralix::editor {

class ScenePanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 460.0f,
      .height = 300.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &) override;
  std::optional<uint64_t> render_version() const override;

private:
  void refresh(bool force = false);
  void open_scene_menu(bool reset_query = true);
  bool scene_menu_open() const;
  void open_runtime_prompt(SceneSessionKind target_kind);
  void close_runtime_prompt();
  bool runtime_prompt_open() const;
  void switch_active_scene_session(SceneSessionKind kind);
  void play_active_scene();
  void pause_active_scene();
  void stop_active_scene();
  void save_active_scene();
  void promote_active_scene();
  void promote_source_to_preview_and_activate();
  void activate_scene(const std::string &scene_id);

  ui::im::Runtime *m_runtime = nullptr;
  ResourceDescriptorID m_default_font_id = "fonts::roboto";
  float m_default_font_size = 16.0f;

  ui::im::WidgetId m_scene_menu_button_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_scene_mode_toggle_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_scene_menu_search_input_widget =
      ui::im::k_invalid_widget_id;

  std::string m_scene_menu_query;
  bool m_scene_menu_is_open = false;
  bool m_focus_scene_menu_search = false;
  std::optional<SceneSessionKind> m_runtime_prompt_target_kind;
};

} // namespace astralix::editor
