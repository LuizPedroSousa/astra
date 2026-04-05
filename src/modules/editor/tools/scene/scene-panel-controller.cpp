#include "tools/scene/scene-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "tools/scene/styles.hpp"
#include "tools/scene/helpers.hpp"
#include "trace.hpp"

#include <algorithm>

namespace astralix::editor {
namespace {

const ScenePanelTheme &scene_panel_theme() {
  static const ScenePanelTheme theme{};
  return theme;
}

} // namespace

void ScenePanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("ScenePanel::render");
  using namespace ui::dsl::styles;

  const auto &theme = scene_panel_theme();
  auto scene_manager = SceneManager::get();
  const auto active_scene_id =
      scene_manager != nullptr ? scene_manager->get_active_scene_id()
                               : std::optional<std::string>{};
  const auto active_scene_session_kind =
      scene_manager != nullptr ? scene_manager->get_active_scene_session_kind()
                               : std::optional<SceneSessionKind>{};
  const auto active_execution_state =
      scene_manager != nullptr ? scene_manager->get_active_scene_execution_state()
                               : std::optional<SceneExecutionState>{};
  const auto active_scene_status =
      scene_manager != nullptr ? scene_manager->get_active_scene_lifecycle_status()
                               : std::optional<SceneLifecycleStatus>{};
  const std::string scene_button_label =
      active_scene_id.has_value() ? *active_scene_id : std::string("Scenes");
  const bool has_active_scene = active_scene_id.has_value();
  const auto lifecycle_status =
      active_scene_status.value_or(SceneLifecycleStatus{});
  const auto execution_state = active_execution_state.value_or(
      active_scene_session_kind == SceneSessionKind::Source
          ? SceneExecutionState::Static
          : SceneExecutionState::Stopped
  );

  if (!active_scene_status.has_value()) {
    m_runtime_prompt_target_kind.reset();
  }

  auto root = ui.view("root").style(fill().background(theme.shell_background));
  auto content = root.column("content").style(fill().padding(14.0f).gap(12.0f));
  {
  ASTRA_PROFILE_N("ScenePanel::controls");
  auto controls_shell = content.view("controls-shell").style(
      fill_x()
          .padding(14.0f)
          .background(theme.card_background)
          .border(1.0f, theme.card_border)
          .radius(16.0f)
  );
  auto controls = controls_shell.column("controls").style(
      fill_x().items_start().gap(10.0f)
  );

  auto menu_button = controls.pressable("scene-menu-button")
                         .on_click([this]() { open_scene_menu(); })
                         .style(
                             row()
                                 .fill_x()
                                 .items_center()
                                 .justify_between()
                                 .gap(8.0f)
                                 .padding_xy(14.0f, 12.0f)
                                 .radius(12.0f)
                                 .background(theme.panel_background)
                                 .border(1.0f, theme.panel_border)
                                 .cursor_pointer()
                                 .hover(
                                     state().background(
                                         theme.panel_raised_background
                                     )
                                 )
                                 .pressed(state().background(theme.accent_soft))
                                 .focused(state().border(2.0f, theme.accent))
                         );
  m_scene_menu_button_widget = menu_button.widget_id();
  auto menu_button_body = menu_button.row("body").style(
      items_center().gap(8.0f).grow(1.0f).min_width(px(0.0f))
  );
  menu_button_body
      .image(
          "icon", scene_panel::scene_menu_trigger_icon_texture(scene_menu_open())
      )
      .style(width(px(12.0f)).height(px(12.0f)).shrink(0.0f));
  menu_button_body.text("label", scene_button_label)
      .style(font_size(13.0f).text_color(theme.text_primary));
  menu_button.text("caption", "Scene")
      .style(font_size(11.0f).text_color(theme.text_muted));

  const auto selected_index = [&]() -> size_t {
    switch (active_scene_session_kind.value_or(SceneSessionKind::Source)) {
      case SceneSessionKind::Source:
        return 0u;
      case SceneSessionKind::Preview:
        return 1u;
      case SceneSessionKind::Runtime:
        return 2u;
    }

    return 0u;
  }();

  auto scene_mode_toggle =
      controls.segmented_control(
                  "scene-mode-toggle", {"Source", "Preview", "Runtime"}, selected_index
              )
          .enabled(has_active_scene)
          .accent_colors({theme.accent, theme.accent, theme.success})
          .on_select([this](size_t index, const std::string &) {
            if (index == 0u) {
              switch_active_scene_session(SceneSessionKind::Source);
              return;
            }

            if (index == 1u) {
              switch_active_scene_session(SceneSessionKind::Preview);
              return;
            }

            if (index == 2u) {
              switch_active_scene_session(SceneSessionKind::Runtime);
            }
          })
          .style(
              fill_x()
                  .height(px(40.0f))
                  .background(theme.panel_background)
                  .border(1.0f, theme.panel_border)
                  .radius(12.0f)
          );
  m_scene_mode_toggle_widget = scene_mode_toggle.widget_id();

  }

  if (auto status_card = content.view("status-card").visible(has_active_scene && active_scene_status.has_value()).style(
      fill_x()
          .padding(14.0f)
          .gap(12.0f)
          .background(theme.card_background)
          .border(1.0f, theme.card_border)
          .radius(16.0f)
  )) {
    ASTRA_PROFILE_N("ScenePanel::status_card");
    auto card_content = status_card.column("content").style(
        fill_x().items_start().gap(12.0f)
    );
    auto heading = card_content.column("heading").style(
        fill_x().items_start().gap(4.0f)
    );
    heading.text("label", "Scene State")
        .style(font_size(11.0f).text_color(theme.text_muted));
    heading.text("scene", scene_button_label)
        .style(font_size(16.0f).text_color(theme.text_primary));

    auto render_status_pill =
        [&](ui::im::Children &parent,
            std::string_view local_name,
            std::string_view title,
            std::string value,
            glm::vec4 color) {
          auto column_node = parent.column(local_name).style(
              items_start().gap(6.0f).shrink(0.0f)
          );
          column_node.text("title", std::string(title))
              .style(font_size(11.0f).text_color(theme.text_muted));
          auto pill = column_node.view("pill").style(
              padding_xy(10.0f, 5.0f)
                  .background(glm::vec4(color.r, color.g, color.b, 0.12f))
                  .border(1.0f, glm::vec4(color.r, color.g, color.b, 0.48f))
                  .radius(999.0f)
                  .items_center()
                  .justify_center()
          );
          pill.text("value", std::move(value))
              .style(font_size(12.0f).text_color(color));
        };

    auto pills_row = card_content.row("lifecycle").style(
        fill_x().items_center().gap(10.0f)
    );
    render_status_pill(
        pills_row,
        "source",
        "Source",
        scene_panel::scene_source_save_state_label(lifecycle_status.source),
        scene_panel::scene_source_save_state_color(theme, lifecycle_status.source)
    );
    render_status_pill(
        pills_row,
        "preview",
        "Preview",
        scene_panel::scene_preview_state_label(lifecycle_status.preview),
        scene_panel::scene_preview_state_color(
            theme, lifecycle_status.preview
        )
    );
    render_status_pill(
        pills_row,
        "runtime",
        "Runtime",
        scene_panel::scene_runtime_state_label(lifecycle_status.runtime),
        scene_panel::scene_runtime_state_color(
            theme, lifecycle_status.runtime
        )
    );

    auto session_row = card_content.row("session").style(
        fill_x().items_center().justify_between().gap(10.0f)
    );
    render_status_pill(
        session_row,
        "execution",
        "Session State",
        scene_panel::scene_execution_state_label(execution_state),
        scene_panel::scene_execution_state_color(theme, execution_state)
    );

    const bool playback_controls_enabled =
        active_scene_session_kind == SceneSessionKind::Preview ||
        active_scene_session_kind == SceneSessionKind::Runtime;
    if (playback_controls_enabled) {
      auto playback = session_row.row("playback").style(items_center().gap(8.0f));
      auto render_action_button =
          [&](ui::im::Children &parent,
              std::string_view local_name,
              std::string label,
              bool enabled,
              bool emphasized,
              std::function<void()> on_click) {
            auto button = parent.pressable(local_name)
                              .enabled(enabled)
                              .on_click(std::move(on_click))
                              .style(
                                  scene_panel::lifecycle_action_button_style(
                                      theme, emphasized
                                  )
                              );
            button.text("label", std::move(label))
                .style(font_size(13.0f).text_color(theme.text_primary));
          };

      render_action_button(
          playback,
          "play",
          "Play",
          active_execution_state != SceneExecutionState::Playing,
          execution_state == SceneExecutionState::Playing,
          [this]() { play_active_scene(); }
      );
      render_action_button(
          playback,
          "pause",
          "Pause",
          active_execution_state == SceneExecutionState::Playing,
          execution_state == SceneExecutionState::Paused,
          [this]() { pause_active_scene(); }
      );
      render_action_button(
          playback,
          "stop",
          "Stop",
          active_execution_state != SceneExecutionState::Stopped,
          execution_state == SceneExecutionState::Stopped,
          [this]() { stop_active_scene(); }
      );
    }

    const auto runtime_hint = scene_panel::scene_runtime_status_hint(
        lifecycle_status,
        active_scene_session_kind,
        active_execution_state
    );
    if (!runtime_hint.empty()) {
      card_content.text("runtime-hint", runtime_hint)
          .style(font_size(11.5f).text_color(theme.text_muted));
    }

    auto actions = card_content.row("actions").style(
        fill_x().items_center().gap(8.0f)
    );
    auto render_primary_action =
        [&](std::string_view local_name,
            std::string label,
            bool enabled,
            std::function<void()> on_click) {
          auto slot = actions.view(std::string(local_name) + "-slot").style(grow(1.0f));
          auto button = slot.pressable(local_name)
                            .enabled(enabled)
                            .on_click(std::move(on_click))
                            .style(
                                scene_panel::lifecycle_action_button_style(
                                    theme, false
                                )
                            );
          button.text("label", std::move(label))
              .style(font_size(13.0f).text_color(theme.text_primary));
        };

    render_primary_action(
        "save",
        "Save",
        active_scene_session_kind == SceneSessionKind::Source ||
            active_scene_session_kind == SceneSessionKind::Preview ||
            active_scene_session_kind == SceneSessionKind::Runtime,
        [this]() { save_active_scene(); }
    );
    render_primary_action(
        "promote",
        "Promote",
        scene_panel::can_promote_from_active_session(
            has_active_scene, active_scene_session_kind
        ),
        [this]() { promote_active_scene(); }
    );
  }

  {
  ASTRA_PROFILE_N("ScenePanel::scene_menu");
  const auto scene_entries =
      scene_manager != nullptr ? scene_manager->get_scene_entries()
                               : std::vector<ProjectSceneEntryConfig>{};
  std::vector<const ProjectSceneEntryConfig *> filtered_entries;
  filtered_entries.reserve(scene_entries.size());
  for (const auto &entry : scene_entries) {
    if (scene_panel::scene_entry_matches_query(entry, m_scene_menu_query)) {
      filtered_entries.push_back(&entry);
    }
  }

  auto scene_menu =
      static_cast<ui::im::Children &>(root).popover("scene-menu").popover(
          ui::im::PopoverState{
      .open = m_scene_menu_is_open,
      .anchor_widget_id = m_scene_menu_button_widget,
      .placement = ui::UIPopupPlacement::BottomStart,
      .depth = 0u,
          }
      );
  scene_menu.style(
      items_start()
          .gap(12.0f)
          .width(px(520.0f))
          .padding(14.0f)
          .background(theme.shell_background)
          .border(1.0f, theme.panel_border)
          .radius(18.0f)
  );

  auto search_input =
      scene_menu.text_input(
                   "search-input",
                   m_scene_menu_query,
                   "Search scenes by id, type, or path"
               )
          .select_all_on_focus(true)
          .on_change([this](const std::string &value) {
            if (m_scene_menu_query != value) {
              m_scene_menu_query = value;
            }
          })
          .style(
              scene_panel::scene_menu_search_input_style(theme),
              font_id(m_default_font_id),
              font_size(std::max(13.0f, m_default_font_size * 0.78f))
          );
  m_scene_menu_search_input_widget = search_input.widget_id();
  if (m_focus_scene_menu_search && m_scene_menu_is_open) {
    ui.request_focus(m_scene_menu_search_input_widget);
    m_focus_scene_menu_search = false;
  }

  auto result_row = scene_menu.row("result-row").style(
      fill_x().items_center().justify_between().padding_xy(2.0f, 0.0f)
  );
  result_row.text("label", "Scenes")
      .style(font_size(11.0f).text_color(theme.text_muted));
  auto result_badge = result_row.view("count-badge").style(
      padding_xy(8.0f, 4.0f)
          .background(theme.panel_background)
          .items_center()
          .justify_center()
          .border(1.0f, theme.panel_border)
          .radius(12.0f)
  );
  result_badge.text(
      "value", scene_panel::scene_result_count_label(filtered_entries.size())
  )
      .style(font_size(11.0f).text_color(theme.text_muted));

  const bool show_list = !filtered_entries.empty();
  if (auto list = scene_menu.scroll_view("list").visible(show_list).style(
      fill_x()
          .max_height(px(360.0f))
          .min_height(px(220.0f))
          .padding_xy(8.0f, 12.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
          .radius(14.0f)
          .overflow_hidden()
          .scroll_vertical()
          .scrollbar_thickness(8.0f)
  )) {
    auto list_content = list.column("content").style(fill_x().gap(8.0f).padding(2.0f));
    for (const auto *entry : filtered_entries) {
      const bool active =
          active_scene_id.has_value() && *active_scene_id == entry->id;
      const auto status =
          scene_manager != nullptr
              ? scene_manager->get_scene_lifecycle_status(entry->id)
              : std::optional<SceneLifecycleStatus>{};
      const auto presentation =
          status.has_value()
              ? scene_panel::describe_scene_menu_entry(*status, theme)
              : scene_panel::SceneMenuEntryPresentation{
                    .status_text =
                        scene_panel::scene_runtime_artifact_exists(*entry)
                            ? "Ready"
                            : "Needs build",
                    .status_color =
                        scene_panel::scene_runtime_artifact_exists(*entry)
                            ? theme.success
                            : theme.accent,
                };

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

      auto item_scope = list_content.item_scope("scene-entry", entry->id);
      auto item = item_scope.pressable("row")
                      .on_click([this, scene_id = entry->id]() {
                        activate_scene(scene_id);
                      })
                      .style(
                          row()
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
                                      .border(
                                          1.0f,
                                          active ? active_border : theme.accent
                                      )
                              )
                              .pressed(state().background(theme.accent_soft))
                              .focused(state().border(2.0f, theme.accent))
                      );
      auto item_main = item.row("main").style(
          items_center().gap(12.0f).grow(1.0f).min_width(px(0.0f))
      );
      auto icon_shell = item_main.view("icon-shell").style(
          width(px(30.0f))
              .height(px(30.0f))
              .shrink(0.0f)
              .items_center()
              .justify_center()
              .background(icon_background)
              .border(1.0f, row_border)
              .radius(8.0f)
      );
      icon_shell.image("icon", "icons::directory")
          .style(width(px(13.0f)).height(px(13.0f)));
      auto labels = item_main.column("labels").style(
          grow(1.0f).min_width(px(0.0f)).items_start().gap(1.0f)
      );
      labels.text("title", entry->id)
          .style(font_size(13.5f).text_color(theme.text_primary));
      labels.text("subtitle", entry->type)
          .style(font_size(11.5f).text_color(theme.text_muted));
      item.text("status", presentation.status_text)
          .visible(!presentation.status_text.empty())
          .style(
              font_size(11.0f)
                  .text_color(presentation.status_color)
                  .shrink(0.0f)
          );
    }
  }

  if (auto empty = scene_menu.column("empty").visible(!show_list).style(
      fill_x()
          .items_center()
          .gap(4.0f)
          .padding(16.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
          .radius(12.0f)
  )) {
    empty.text(
        "title",
        scene_entries.empty() ? "No scenes declared" : "No matching scenes"
    )
        .style(font_size(13.0f).text_color(theme.text_primary));
    empty.text(
        "body",
        scene_entries.empty()
            ? "Add scene entries to the project manifest to switch them here."
            : "Try a scene id, scene type, or part of the path."
    )
        .style(font_size(11.5f).text_color(theme.text_muted));
  }

  }

  {
  ASTRA_PROFILE_N("ScenePanel::runtime_prompt");
  const bool runtime_prompt_is_open =
      m_runtime_prompt_target_kind.has_value() && active_scene_status.has_value();
  auto runtime_prompt =
      static_cast<ui::im::Children &>(root)
          .popover("runtime-prompt")
          .popover(ui::im::PopoverState{
          .open = runtime_prompt_is_open,
          .anchor_widget_id = m_scene_mode_toggle_widget,
          .placement = ui::UIPopupPlacement::BottomStart,
          .depth = 0u,
          });
  runtime_prompt.style(
      items_start()
          .gap(12.0f)
          .width(px(360.0f))
          .padding(14.0f)
          .background(theme.shell_background)
          .border(1.0f, theme.panel_border)
          .radius(16.0f)
  );
  auto prompt_body = runtime_prompt.column("body").style(
      fill_x().items_start().gap(4.0f)
  );
  prompt_body.text(
      "title",
      active_scene_status.has_value()
          ? scene_panel::runtime_prompt_title(
                m_runtime_prompt_target_kind, *active_scene_status
            )
          : std::string("Preview is missing")
  )
      .style(font_size(15.0f).text_color(theme.text_primary));
  prompt_body.text(
      "copy",
      active_scene_status.has_value()
          ? scene_panel::runtime_prompt_body(
                active_scene_session_kind,
                m_runtime_prompt_target_kind,
                *active_scene_status
            )
          : std::string(
                "Entering preview builds the current source scene in memory."
            )
  )
      .style(font_size(12.0f).text_color(theme.text_muted));
  auto prompt_actions = runtime_prompt.row("actions").style(
      fill_x().items_center().gap(8.0f)
  );
  auto promote_enter = prompt_actions.pressable("promote-enter")
                           .on_click([this]() {
                             promote_source_to_preview_and_activate();
                           })
                           .style(
                               scene_panel::lifecycle_action_button_style(
                                   theme, true
                               )
                           );
  promote_enter.text("label", "Promote + Enter")
      .style(font_size(13.0f).text_color(theme.text_primary));
  auto cancel = prompt_actions.pressable("cancel")
                    .on_click([this]() { close_runtime_prompt(); })
                    .style(
                        scene_panel::lifecycle_action_button_style(
                            theme, false
                        )
                    );
  cancel.text("label", "Cancel")
      .style(font_size(13.0f).text_color(theme.text_primary));
  }
}

void ScenePanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
}

std::optional<uint64_t> ScenePanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_string(m_scene_menu_query, hash);
  hash = fnv1a64_append_value(hash, m_scene_menu_is_open);
  hash = fnv1a64_append_value(hash, m_focus_scene_menu_search);

  const bool has_runtime_prompt = m_runtime_prompt_target_kind.has_value();
  hash = fnv1a64_append_value(hash, has_runtime_prompt);
  if (has_runtime_prompt) {
    hash = fnv1a64_append_value(hash, *m_runtime_prompt_target_kind);
  }

  auto scene_manager = SceneManager::get();
  const auto active_scene_id =
      scene_manager != nullptr ? scene_manager->get_active_scene_id()
                               : std::optional<std::string>{};
  const auto active_scene_session_kind =
      scene_manager != nullptr ? scene_manager->get_active_scene_session_kind()
                               : std::optional<SceneSessionKind>{};
  const auto active_execution_state =
      scene_manager != nullptr ? scene_manager->get_active_scene_execution_state()
                               : std::optional<SceneExecutionState>{};
  const auto active_scene_status =
      scene_manager != nullptr ? scene_manager->get_active_scene_lifecycle_status()
                               : std::optional<SceneLifecycleStatus>{};

  hash = fnv1a64_append_value(hash, active_scene_id.has_value());
  if (active_scene_id.has_value()) {
    hash = fnv1a64_append_string(*active_scene_id, hash);
  }

  hash = fnv1a64_append_value(hash, active_scene_session_kind.has_value());
  if (active_scene_session_kind.has_value()) {
    hash = fnv1a64_append_value(hash, *active_scene_session_kind);
  }

  hash = fnv1a64_append_value(hash, active_execution_state.has_value());
  if (active_execution_state.has_value()) {
    hash = fnv1a64_append_value(hash, *active_execution_state);
  }

  hash = fnv1a64_append_value(hash, active_scene_status.has_value());
  if (active_scene_status.has_value()) {
    hash = fnv1a64_append_value(hash, active_scene_status->source);
    hash = fnv1a64_append_value(hash, active_scene_status->preview);
    hash = fnv1a64_append_value(hash, active_scene_status->runtime);
  }

  if (m_scene_menu_is_open && scene_manager != nullptr) {
    const auto scene_entries = scene_manager->get_scene_entries();
    hash = fnv1a64_append_value(hash, scene_entries.size());
    for (const auto &entry : scene_entries) {
      hash = fnv1a64_append_string(entry.id, hash);
      hash = fnv1a64_append_string(entry.type, hash);
      hash = fnv1a64_append_string(entry.source_path, hash);
      hash = fnv1a64_append_string(entry.preview_path, hash);
      hash = fnv1a64_append_string(entry.runtime_path, hash);

      const auto status = scene_manager->get_scene_lifecycle_status(entry.id);
      hash = fnv1a64_append_value(hash, status.has_value());
      if (status.has_value()) {
        hash = fnv1a64_append_value(hash, status->source);
        hash = fnv1a64_append_value(hash, status->preview);
        hash = fnv1a64_append_value(hash, status->runtime);
      }
    }
  }

  return hash;
}

void ScenePanelController::unmount() {
  m_runtime = nullptr;
  m_scene_menu_is_open = false;
  m_focus_scene_menu_search = false;
  m_scene_menu_button_widget = ui::im::k_invalid_widget_id;
  m_scene_mode_toggle_widget = ui::im::k_invalid_widget_id;
  m_scene_menu_search_input_widget = ui::im::k_invalid_widget_id;
  m_runtime_prompt_target_kind.reset();
}

void ScenePanelController::update(const PanelUpdateContext &) { refresh(); }

void ScenePanelController::refresh(bool force) {
  static_cast<void>(force);
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr ||
      !scene_manager->get_active_scene_lifecycle_status().has_value()) {
    m_runtime_prompt_target_kind.reset();
  }
}

void ScenePanelController::open_scene_menu(bool reset_query) {
  if (reset_query && !m_scene_menu_query.empty()) {
    m_scene_menu_query.clear();
  }

  if (scene_menu_open()) {
    m_scene_menu_is_open = false;
    m_focus_scene_menu_search = false;
    return;
  }

  close_runtime_prompt();
  m_scene_menu_is_open = true;
  m_focus_scene_menu_search = true;
}

bool ScenePanelController::scene_menu_open() const { return m_scene_menu_is_open; }

void ScenePanelController::open_runtime_prompt(SceneSessionKind target_kind) {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr ||
      !scene_manager->get_active_scene_lifecycle_status().has_value()) {
    return;
  }

  m_runtime_prompt_target_kind = target_kind;
  m_scene_menu_is_open = false;
  m_focus_scene_menu_search = false;
}

void ScenePanelController::close_runtime_prompt() {
  m_runtime_prompt_target_kind.reset();
}

bool ScenePanelController::runtime_prompt_open() const {
  return m_runtime_prompt_target_kind.has_value();
}

void ScenePanelController::switch_active_scene_session(
    SceneSessionKind kind
) {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  const auto active_scene_id = scene_manager->get_active_scene_id();
  if (!active_scene_id.has_value()) {
    refresh(true);
    return;
  }

  const auto current_session_kind =
      scene_manager->get_active_scene_session_kind();

  switch (kind) {
    case SceneSessionKind::Source:
      scene_manager->activate_source(*active_scene_id);
      close_runtime_prompt();
      refresh(true);
      return;

    case SceneSessionKind::Preview:
      if (scene_manager->activate_preview(*active_scene_id) == nullptr) {
        if (current_session_kind == SceneSessionKind::Source) {
          open_runtime_prompt(SceneSessionKind::Preview);
        } else {
          refresh(true);
        }
        return;
      }

      close_runtime_prompt();
      refresh(true);
      return;

    case SceneSessionKind::Runtime: {
      const auto status = scene_manager->get_active_scene_lifecycle_status();
      if (!status.has_value() ||
          status->runtime == SceneRuntimeState::Missing ||
          status->runtime == SceneRuntimeState::Error) {
        if (current_session_kind == SceneSessionKind::Preview) {
          open_runtime_prompt(SceneSessionKind::Runtime);
        } else {
          refresh(true);
        }
        return;
      }

      scene_manager->activate_runtime(*active_scene_id);
      close_runtime_prompt();
      refresh(true);
      return;
    }
  }
}

void ScenePanelController::play_active_scene() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr || !scene_manager->play_active_scene()) {
    refresh(true);
    return;
  }

  close_runtime_prompt();
  refresh(true);
}

void ScenePanelController::pause_active_scene() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr || !scene_manager->pause_active_scene()) {
    refresh(true);
    return;
  }

  close_runtime_prompt();
  refresh(true);
}

void ScenePanelController::stop_active_scene() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr || !scene_manager->stop_active_scene()) {
    refresh(true);
    return;
  }

  close_runtime_prompt();
  refresh(true);
}

void ScenePanelController::save_active_scene() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  const auto active_session_kind = scene_manager->get_active_scene_session_kind();
  if (!active_session_kind.has_value() ||
      (*active_session_kind != SceneSessionKind::Source &&
       *active_session_kind != SceneSessionKind::Preview &&
       *active_session_kind != SceneSessionKind::Runtime)) {
    return;
  }

  if (Scene *scene = scene_manager->get_active_scene(); scene != nullptr) {
    if (*active_session_kind == SceneSessionKind::Source) {
      scene->save_source();
    } else if (*active_session_kind == SceneSessionKind::Preview) {
      scene->save_preview();
    } else {
      scene->save_runtime();
    }
  }

  close_runtime_prompt();
  refresh(true);
}

void ScenePanelController::promote_active_scene() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  const auto active_scene_id = scene_manager->get_active_scene_id();
  if (!active_scene_id.has_value()) {
    return;
  }

  const auto active_session_kind = scene_manager->get_active_scene_session_kind();
  if (!active_session_kind.has_value()) {
    return;
  }

  if (*active_session_kind == SceneSessionKind::Source) {
    (void)scene_manager->promote_source_to_preview(*active_scene_id);
  } else if (*active_session_kind == SceneSessionKind::Preview) {
    (void)scene_manager->promote_preview(*active_scene_id);
  } else {
    return;
  }

  close_runtime_prompt();
  refresh(true);
}

void ScenePanelController::promote_source_to_preview_and_activate() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  const auto active_scene_id = scene_manager->get_active_scene_id();
  if (!active_scene_id.has_value()) {
    return;
  }

  const auto active_session_kind = scene_manager->get_active_scene_session_kind();
  if (!active_session_kind.has_value() ||
      !m_runtime_prompt_target_kind.has_value()) {
    refresh(true);
    return;
  }

  if (m_runtime_prompt_target_kind == SceneSessionKind::Preview &&
      *active_session_kind == SceneSessionKind::Source) {
    if (!scene_manager->promote_source_to_preview(*active_scene_id)) {
      refresh(true);
      return;
    }

    if (scene_manager->activate_preview(*active_scene_id) == nullptr) {
      refresh(true);
      return;
    }

    close_runtime_prompt();
    refresh(true);
    return;
  }

  if (m_runtime_prompt_target_kind == SceneSessionKind::Runtime &&
      *active_session_kind == SceneSessionKind::Preview) {
    if (!scene_manager->promote_preview(*active_scene_id)) {
      refresh(true);
      return;
    }

    if (scene_manager->activate_runtime(*active_scene_id) == nullptr) {
      refresh(true);
      return;
    }

    close_runtime_prompt();
    refresh(true);
    return;
  }

  refresh(true);
}

void ScenePanelController::activate_scene(const std::string &scene_id) {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  scene_manager->activate_source(scene_id);
  close_runtime_prompt();
  m_scene_menu_is_open = false;
  m_focus_scene_menu_search = false;
  refresh(true);
}

} // namespace astralix::editor
