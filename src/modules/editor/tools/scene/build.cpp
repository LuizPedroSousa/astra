#include "tools/scene/build.hpp"

namespace astralix::editor::scene_panel {
namespace {

ui::dsl::NodeSpec build_scene_menu_badge(
    std::string label,
    ui::UINodeId &text_node,
    const ScenePanelTheme &theme,
    bool emphasized = false
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  return view()
      .style(
          padding_xy(8.0f, 4.0f)
              .background(
                  emphasized ? theme.accent_soft : theme.panel_background
              )
              .items_center()
              .justify_center()
              .border(1.0f, emphasized ? theme.accent : theme.panel_border)
              .radius(12.0f)
      )
      .child(
          text(std::move(label))
              .bind(text_node)
              .style(
                  font_size(11.0f).text_color(
                      emphasized ? theme.text_primary : theme.text_muted
                  )
              )
      );
}

ui::dsl::NodeSpec build_status_pill(
    std::string label,
    ui::UINodeId &pill_node,
    ui::UINodeId &value_node,
    glm::vec4 color
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  return view()
      .bind(pill_node)
      .style(
          padding_xy(10.0f, 5.0f)
              .background(glm::vec4(color.r, color.g, color.b, 0.12f))
              .border(1.0f, glm::vec4(color.r, color.g, color.b, 0.48f))
              .radius(999.0f)
              .items_center()
              .justify_center()
      )
      .child(
          text(std::move(label))
              .bind(value_node)
              .style(font_size(12.0f).text_color(color))
      );
}

} // namespace

ui::dsl::StyleBuilder scene_menu_search_input_style(
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl::styles;

  return fill_x()
      .height(px(50.0f))
      .padding_xy(14.0f, 10.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .radius(12.0f)
      .text_color(theme.text_primary)
      .hover(state().background(theme.panel_raised_background))
      .focused(state().border(2.0f, theme.accent))
      .raw([theme](ui::UIStyle &style) {
        style.placeholder_text_color = theme.text_muted;
        style.selection_color =
            glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.22f);
        style.caret_color = theme.text_primary;
      });
}

ui::dsl::StyleBuilder lifecycle_action_button_style(
    const ScenePanelTheme &theme,
    bool emphasized
) {
  using namespace ui::dsl::styles;

  return padding_xy(14.0f, 9.0f)
      .radius(12.0f)
      .background(emphasized ? theme.accent_soft : theme.panel_background)
      .border(1.0f, emphasized ? theme.accent : theme.panel_border)
      .text_color(theme.text_primary)
      .cursor_pointer()
      .hover(
          state().background(
              emphasized ? theme.accent_soft : theme.panel_raised_background
          )
      )
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent))
      .disabled(state().opacity(0.45f));
}

ui::dsl::NodeSpec build_lifecycle_action_button(
    std::string label,
    bool enabled,
    bool emphasized,
    ui::UINodeId &button_node,
    std::function<void()> on_click,
    const ScenePanelTheme &theme
) {
  return ui::dsl::button(std::move(label), std::move(on_click))
      .bind(button_node)
      .enabled(enabled)
      .style(lifecycle_action_button_style(theme, emphasized));
}

ui::dsl::NodeSpec build_scene_menu_trigger_button(
    std::string label,
    ui::UINodeId &button_node,
    ui::UINodeId &icon_node,
    ui::UINodeId &label_node,
    bool open,
    std::function<void()> on_click,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  return pressable()
      .bind(button_node)
      .on_click(std::move(on_click))
      .style(
          ui::dsl::styles::row()
              .fill_x()
              .items_center()
              .justify_between()
              .gap(8.0f)
              .padding_xy(14.0f, 12.0f)
              .radius(12.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .cursor_pointer()
              .hover(state().background(theme.panel_raised_background))
              .pressed(state().background(theme.accent_soft))
              .focused(state().border(2.0f, theme.accent))
      )
      .children(
          ui::dsl::row()
              .style(items_center().gap(8.0f).grow(1.0f).min_width(px(0.0f)))
              .children(
                  image(scene_menu_trigger_icon_texture(open))
                      .bind(icon_node)
                      .style(
                          width(px(12.0f))
                              .height(px(12.0f))
                              .shrink(0.0f)
                      ),
                  text(std::move(label))
                      .bind(label_node)
                      .style(font_size(13.0f).text_color(theme.text_primary))
              ),
          text("Scene")
              .style(font_size(11.0f).text_color(theme.text_muted))
      );
}

ui::dsl::NodeSpec build_scene_mode_toggle(
    std::optional<SceneSessionKind> active_session_kind,
    bool enabled,
    ui::UINodeId &toggle_node,
    std::function<void(size_t, const std::string &)> on_select,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  const auto selected_index = [&]() -> size_t {
    switch (active_session_kind.value_or(SceneSessionKind::Source)) {
      case SceneSessionKind::Source:
        return 0u;
      case SceneSessionKind::Preview:
        return 1u;
      case SceneSessionKind::Runtime:
        return 2u;
    }

    return 0u;
  }();

  return segmented_control(
             {"Source", "Preview", "Runtime"}, selected_index
  )
      .bind(toggle_node)
      .enabled(enabled)
      .accent_colors({theme.accent, theme.accent, theme.success})
      .on_select(std::move(on_select))
      .style(
          fill_x()
              .height(px(40.0f))
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .radius(12.0f)
      );
}

ui::dsl::NodeSpec build_scene_menu_popover(
    ui::UINodeId &popover_node,
    ui::UINodeId &search_input_node,
    ui::UINodeId &result_count_node,
    ui::UINodeId &list_node,
    ui::UINodeId &content_node,
    ui::UINodeId &empty_state_node,
    ui::UINodeId &empty_title_node,
    ui::UINodeId &empty_body_node,
    const std::string &query,
    std::function<void(const std::string &)> on_search_change,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  return popover()
      .bind(popover_node)
      .style(
          items_start()
              .gap(12.0f)
              .width(px(520.0f))
              .padding(14.0f)
              .background(theme.shell_background)
              .border(1.0f, theme.panel_border)
              .radius(18.0f)
      )
      .children(
          text_input(query, "Search scenes by id, type, or path")
              .bind(search_input_node)
              .select_all_on_focus(true)
              .on_change(std::move(on_search_change))
              .style(scene_menu_search_input_style(theme)),
          ui::dsl::row()
              .style(fill_x().items_center().justify_between().padding_xy(2.0f, 0.0f))
              .children(
                  text("Scenes")
                      .style(font_size(11.0f).text_color(theme.text_muted)),
                  build_scene_menu_badge(
                      "0 results", result_count_node, theme, false
                  )
              ),
          scroll_view()
              .bind(list_node)
              .style(
                  fill_x()
                      .max_height(px(360.0f))
                      .min_height(px(220.0f))
                      .padding_xy(8.0f, 12.0f)
                      .background(theme.panel_background)
                      .border(1.0f, theme.panel_border)
                      .radius(14.0f)
                      .overflow_hidden()
                      .raw([](ui::UIStyle &style) {
                        style.scroll_mode = ui::ScrollMode::Vertical;
                        style.scrollbar_thickness = 8.0f;
                      })
              )
              .child(
                  ui::dsl::column()
                      .bind(content_node)
                      .style(fill_x().gap(8.0f).padding(2.0f))
              ),
          ui::dsl::column()
              .bind(empty_state_node)
              .visible(false)
              .style(
                  fill_x()
                      .items_center()
                      .gap(4.0f)
                      .padding(16.0f)
                      .background(theme.panel_background)
                      .border(1.0f, theme.panel_border)
                      .radius(12.0f)
              )
              .children(
                  text("No scenes declared")
                      .bind(empty_title_node)
                      .style(font_size(13.0f).text_color(theme.text_primary)),
                  text("Add scene entries to the project manifest to switch them here.")
                      .bind(empty_body_node)
                      .style(font_size(11.5f).text_color(theme.text_muted))
              )
      );
}

ui::dsl::NodeSpec build_scene_menu_entry_item(
    const ProjectSceneEntryConfig &entry,
    bool active,
    const SceneMenuEntryPresentation &presentation,
    std::function<void()> on_activate_scene,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  const glm::vec4 active_surface =
      glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.08f);
  const glm::vec4 active_border =
      glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.52f);
  const glm::vec4 icon_shell_active =
      glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.14f);
  const glm::vec4 row_background =
      active ? active_surface : theme.panel_background;
  const glm::vec4 row_border = active ? active_border : theme.panel_border;
  const glm::vec4 icon_background =
      active ? icon_shell_active : theme.panel_raised_background;

  return ui::dsl::pressable()
      .on_click(std::move(on_activate_scene))
      .style(
          ui::dsl::styles::row()
              .fill_x()
              .items_center()
              .justify_between()
              .gap(12.0f)
              .padding_xy(14.0f, 12.0f)
              .radius(12.0f)
              .background(row_background)
              .border(1.0f, row_border)
              .cursor_pointer()
              .hover(
                  state()
                      .background(theme.panel_raised_background)
                      .border(1.0f, active ? active_border : theme.accent)
              )
              .pressed(state().background(theme.accent_soft))
              .focused(state().border(2.0f, theme.accent))
      )
      .children(
          ui::dsl::row()
              .style(
                  items_center().gap(12.0f).grow(1.0f).min_width(px(0.0f))
              )
              .children(
                  view()
                      .style(
                          width(px(30.0f))
                              .height(px(30.0f))
                              .shrink(0.0f)
                              .items_center()
                              .justify_center()
                              .background(icon_background)
                              .border(1.0f, row_border)
                              .radius(8.0f)
                      )
                      .child(
                          image("icons::directory")
                              .style(
                                  width(px(13.0f))
                                      .height(px(13.0f))
                              )
                      ),
                  ui::dsl::column()
                      .style(grow(1.0f).min_width(px(0.0f)).items_start().gap(1.0f))
                      .children(
                          text(entry.id)
                              .style(
                                  font_size(13.5f).text_color(theme.text_primary)
                              ),
                          text(entry.type)
                              .style(
                                  font_size(11.5f).text_color(theme.text_muted)
                              )
                      )
              ),
          text(std::move(presentation.status_text))
              .visible(!presentation.status_text.empty())
              .style(
                  font_size(11.0f)
                      .text_color(presentation.status_color)
                      .shrink(0.0f)
              )
      );
}

ui::dsl::NodeSpec build_scene_status_card(
    std::string scene_label,
    SceneSourceSaveState source_state,
    ScenePreviewState preview_state,
    SceneRuntimeState runtime_state,
    SceneExecutionState active_execution_state,
    std::string runtime_hint,
    bool playback_controls_enabled,
    ui::UINodeId &execution_chip_node,
    ui::UINodeId &execution_value_node,
    ui::UINodeId &playback_controls_node,
    bool play_enabled,
    ui::UINodeId &play_button_node,
    std::function<void()> on_play_click,
    bool pause_enabled,
    ui::UINodeId &pause_button_node,
    std::function<void()> on_pause_click,
    bool stop_enabled,
    ui::UINodeId &stop_button_node,
    std::function<void()> on_stop_click,
    bool save_enabled,
    ui::UINodeId &save_button_node,
    std::function<void()> on_save_click,
    bool promote_enabled,
    ui::UINodeId &promote_button_node,
    std::function<void()> on_promote_click,
    ui::UINodeId &card_node,
    ui::UINodeId &scene_value_node,
    ui::UINodeId &source_chip_node,
    ui::UINodeId &source_value_node,
    ui::UINodeId &preview_chip_node,
    ui::UINodeId &preview_value_node,
    ui::UINodeId &runtime_chip_node,
    ui::UINodeId &runtime_value_node,
    ui::UINodeId &runtime_hint_node,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  const glm::vec4 source_color =
      scene_source_save_state_color(theme, source_state);
  const glm::vec4 preview_color =
      scene_preview_state_color(theme, preview_state);
  const glm::vec4 runtime_color =
      scene_runtime_state_color(theme, runtime_state);
  const glm::vec4 execution_color =
      scene_execution_state_color(theme, active_execution_state);

  return view()
      .bind(card_node)
      .style(
          fill_x()
              .padding(14.0f)
              .gap(12.0f)
              .background(theme.card_background)
              .border(1.0f, theme.card_border)
              .radius(16.0f)
      )
      .child(
          ui::dsl::column()
              .style(fill_x().items_start().gap(12.0f))
              .children(
                  ui::dsl::column()
                      .style(fill_x().items_start().gap(4.0f))
                      .children(
                          text("Scene State")
                              .style(font_size(11.0f).text_color(theme.text_muted)),
                          text(std::move(scene_label))
                              .bind(scene_value_node)
                              .style(font_size(16.0f).text_color(theme.text_primary))
                      ),
                  ui::dsl::row()
                      .style(fill_x().items_center().gap(10.0f))
                      .children(
                          ui::dsl::column()
                              .style(items_start().gap(6.0f).shrink(0.0f))
                              .children(
                                  text("Source")
                                      .style(font_size(11.0f).text_color(theme.text_muted)),
                                  build_status_pill(
                                      scene_source_save_state_label(source_state),
                                      source_chip_node,
                                      source_value_node,
                                      source_color
                                  )
                              ),
                          ui::dsl::column()
                              .style(items_start().gap(6.0f).shrink(0.0f))
                              .children(
                                  text("Preview")
                                      .style(font_size(11.0f).text_color(theme.text_muted)),
                                  build_status_pill(
                                      scene_preview_state_label(preview_state),
                                      preview_chip_node,
                                      preview_value_node,
                                      preview_color
                                  )
                              ),
                          ui::dsl::column()
                              .style(items_start().gap(6.0f).shrink(0.0f))
                              .children(
                                  text("Runtime")
                                      .style(font_size(11.0f).text_color(theme.text_muted)),
                                  build_status_pill(
                                      scene_runtime_state_label(runtime_state),
                                      runtime_chip_node,
                                      runtime_value_node,
                                      runtime_color
                                  )
                              )
                      ),
                  ui::dsl::row()
                      .style(fill_x().items_center().justify_between().gap(10.0f))
                      .children(
                          ui::dsl::column()
                              .style(items_start().gap(6.0f).shrink(0.0f))
                              .children(
                                  text("Session State")
                                      .style(font_size(11.0f).text_color(theme.text_muted)),
                                  build_status_pill(
                                      scene_execution_state_label(
                                          active_execution_state
                                      ),
                                      execution_chip_node,
                                      execution_value_node,
                                      execution_color
                                  )
                              ),
                          ui::dsl::row()
                              .bind(playback_controls_node)
                              .visible(playback_controls_enabled)
                              .style(items_center().gap(8.0f))
                              .children(
                                  build_lifecycle_action_button(
                                      "Play",
                                      play_enabled,
                                      active_execution_state ==
                                          SceneExecutionState::Playing,
                                      play_button_node,
                                      std::move(on_play_click),
                                      theme
                                  ),
                                  build_lifecycle_action_button(
                                      "Pause",
                                      pause_enabled,
                                      active_execution_state ==
                                          SceneExecutionState::Paused,
                                      pause_button_node,
                                      std::move(on_pause_click),
                                      theme
                                  ),
                                  build_lifecycle_action_button(
                                      "Stop",
                                      stop_enabled,
                                      active_execution_state ==
                                          SceneExecutionState::Stopped,
                                      stop_button_node,
                                      std::move(on_stop_click),
                                      theme
                                  )
                              )
                      ),
                  text(std::move(runtime_hint))
                      .bind(runtime_hint_node)
                      .visible(!runtime_hint.empty())
                      .style(font_size(11.5f).text_color(theme.text_muted)),
                  ui::dsl::row()
                      .style(fill_x().items_center().gap(8.0f))
                      .children(
                          ui::dsl::view()
                              .style(grow(1.0f))
                              .child(build_lifecycle_action_button(
                                  "Save",
                                  save_enabled,
                                  false,
                                  save_button_node,
                                  std::move(on_save_click),
                                  theme
                              )),
                          ui::dsl::view()
                              .style(grow(1.0f))
                              .child(build_lifecycle_action_button(
                                  "Promote",
                                  promote_enabled,
                                  false,
                                  promote_button_node,
                                  std::move(on_promote_click),
                                  theme
                              ))
                      )
              )
      );
}

ui::dsl::NodeSpec build_runtime_prompt(
    ui::UINodeId &popover_node,
    ui::UINodeId &title_node,
    ui::UINodeId &body_node,
    std::function<void()> on_build_and_enter,
    std::function<void()> on_cancel,
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  return popover()
      .bind(popover_node)
      .style(
          items_start()
              .gap(12.0f)
              .width(px(360.0f))
              .padding(14.0f)
              .background(theme.shell_background)
              .border(1.0f, theme.panel_border)
              .radius(16.0f)
      )
      .children(
          ui::dsl::column()
              .style(fill_x().items_start().gap(4.0f))
              .children(
                  text("Preview is missing")
                      .bind(title_node)
                      .style(font_size(15.0f).text_color(theme.text_primary)),
                  text("Entering preview builds the current source scene in memory.")
                      .bind(body_node)
                      .style(font_size(12.0f).text_color(theme.text_muted))
              ),
          ui::dsl::row()
              .style(fill_x().items_center().gap(8.0f))
              .children(
                  ui::dsl::button("Promote + Enter", std::move(on_build_and_enter))
                      .style(lifecycle_action_button_style(theme, true)),
                  ui::dsl::button("Cancel", std::move(on_cancel))
                      .style(lifecycle_action_button_style(theme, false))
              )
      );
}

} // namespace astralix::editor::scene_panel
