#include "tools/scene/scene-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "tools/scene/build.hpp"
#include "tools/scene/helpers.hpp"

#include <algorithm>

namespace astralix::editor {
namespace {

const ScenePanelTheme &scene_panel_theme() {
  static const ScenePanelTheme theme{};
  return theme;
}

void mutate_status_pill(
    ui::UIDocument &document,
    ui::UINodeId pill_node,
    ui::UINodeId value_node,
    glm::vec4 color
) {
  if (pill_node != ui::k_invalid_node_id) {
    document.mutate_style(pill_node, [&](ui::UIStyle &style) {
      style.background_color = glm::vec4(color.r, color.g, color.b, 0.12f);
      style.border_color = glm::vec4(color.r, color.g, color.b, 0.48f);
    });
  }

  if (value_node != ui::k_invalid_node_id) {
    document.mutate_style(value_node, [&](ui::UIStyle &style) {
      style.text_color = color;
    });
  }
}

} // namespace

ui::dsl::NodeSpec ScenePanelController::build() {
  using namespace ui::dsl;
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
  const auto lifecycle_status =
      active_scene_status.value_or(SceneLifecycleStatus{});
  const auto execution_state = active_execution_state.value_or(
      active_scene_session_kind == SceneSessionKind::Source
          ? SceneExecutionState::Static
          : SceneExecutionState::Stopped
  );
  const auto runtime_hint = scene_panel::scene_runtime_status_hint(
      lifecycle_status, active_scene_session_kind, active_execution_state
  );

  auto scene_menu_button = scene_panel::build_scene_menu_trigger_button(
      scene_button_label,
      m_scene_menu_button_node,
      m_scene_menu_trigger_icon_node,
      m_scene_menu_trigger_label_node,
      scene_menu_open(),
      [this]() { open_scene_menu(); },
      theme
  );

  auto scene_mode_toggle = scene_panel::build_scene_mode_toggle(
      active_scene_session_kind,
      active_scene_id.has_value(),
      m_scene_mode_toggle_node,
      [this](size_t index, const std::string &) {
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
      },
      theme
  );

  auto scene_menu = scene_panel::build_scene_menu_popover(
      m_scene_menu_node,
      m_scene_menu_search_input_node,
      m_scene_menu_result_count_node,
      m_scene_menu_list_node,
      m_scene_menu_content_node,
      m_scene_menu_empty_state_node,
      m_scene_menu_empty_title_node,
      m_scene_menu_empty_body_node,
      m_scene_menu_query,
      [this](const std::string &value) {
        if (m_scene_menu_query == value) {
          return;
        }

        m_scene_menu_query = value;
        rebuild_scene_menu_content();
      },
      theme
  );

  auto status_card = scene_panel::build_scene_status_card(
      scene_button_label,
      lifecycle_status.source,
      lifecycle_status.preview,
      lifecycle_status.runtime,
      execution_state,
      runtime_hint,
      active_scene_session_kind == SceneSessionKind::Preview ||
          active_scene_session_kind == SceneSessionKind::Runtime,
      m_scene_status_execution_chip_node,
      m_scene_status_execution_value_node,
      m_scene_playback_controls_node,
      (active_scene_session_kind == SceneSessionKind::Preview ||
       active_scene_session_kind == SceneSessionKind::Runtime) &&
          active_execution_state != SceneExecutionState::Playing,
      m_scene_play_button_node,
      [this]() { play_active_scene(); },
      (active_scene_session_kind == SceneSessionKind::Preview ||
       active_scene_session_kind == SceneSessionKind::Runtime) &&
          active_execution_state == SceneExecutionState::Playing,
      m_scene_pause_button_node,
      [this]() { pause_active_scene(); },
      (active_scene_session_kind == SceneSessionKind::Preview ||
       active_scene_session_kind == SceneSessionKind::Runtime) &&
          active_execution_state != SceneExecutionState::Stopped,
      m_scene_stop_button_node,
      [this]() { stop_active_scene(); },
      active_scene_session_kind == SceneSessionKind::Source ||
          active_scene_session_kind == SceneSessionKind::Preview ||
          active_scene_session_kind == SceneSessionKind::Runtime,
      m_scene_save_button_node,
      [this]() { save_active_scene(); },
      scene_panel::can_promote_from_active_session(
          active_scene_id.has_value(), active_scene_session_kind
      ),
      m_scene_promote_button_node,
      [this]() { promote_active_scene(); },
      m_scene_status_card_node,
      m_scene_status_scene_value_node,
      m_scene_status_source_chip_node,
      m_scene_status_source_value_node,
      m_scene_status_preview_chip_node,
      m_scene_status_preview_value_node,
      m_scene_status_runtime_chip_node,
      m_scene_status_runtime_value_node,
      m_scene_status_runtime_hint_node,
      theme
  );

  auto runtime_prompt = scene_panel::build_runtime_prompt(
      m_runtime_prompt_node,
      m_runtime_prompt_title_node,
      m_runtime_prompt_body_node,
      [this]() { promote_source_to_preview_and_activate(); },
      [this]() { close_runtime_prompt(); },
      theme
  );

  auto root = ui::dsl::view().style(fill().background(theme.shell_background));
  root.child(
      ui::dsl::column()
          .style(fill().padding(14.0f).gap(12.0f))
          .children(
              ui::dsl::view()
                  .style(
                      fill_x()
                          .padding(14.0f)
                          .background(theme.card_background)
                          .border(1.0f, theme.card_border)
                          .radius(16.0f)
                  )
                  .child(
                      ui::dsl::column()
                          .style(fill_x().items_start().gap(10.0f))
                          .children(
                              std::move(scene_menu_button),
                              std::move(scene_mode_toggle)
                          )
                  ),
              std::move(status_card)
          )
  );
  root.child(std::move(scene_menu));
  root.child(std::move(runtime_prompt));

  return root;
}

void ScenePanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;

  if (m_document != nullptr &&
      m_scene_menu_search_input_node != ui::k_invalid_node_id) {
    m_document->set_text(m_scene_menu_search_input_node, m_scene_menu_query);
    m_document->mutate_style(
        m_scene_menu_search_input_node,
        [this](ui::UIStyle &style) {
          style.font_id = m_default_font_id;
          style.font_size = std::max(13.0f, m_default_font_size * 0.78f);
        }
    );
  }

  rebuild_scene_menu_content();
  refresh(true);
}

void ScenePanelController::unmount() {
  m_runtime_prompt_target_kind.reset();
  m_document = nullptr;
}

void ScenePanelController::update(const PanelUpdateContext &) { refresh(); }

void ScenePanelController::refresh(bool force) {
  if (m_document == nullptr) {
    return;
  }

  auto scene_manager = SceneManager::get();
  const auto active_scene_id =
      scene_manager != nullptr ? scene_manager->get_active_scene_id()
                               : std::optional<std::string>{};
  const auto active_session_kind =
      scene_manager != nullptr ? scene_manager->get_active_scene_session_kind()
                               : std::optional<SceneSessionKind>{};
  const auto active_execution_state =
      scene_manager != nullptr ? scene_manager->get_active_scene_execution_state()
                               : std::optional<SceneExecutionState>{};
  const auto active_status =
      scene_manager != nullptr ? scene_manager->get_active_scene_lifecycle_status()
                               : std::optional<SceneLifecycleStatus>{};
  const std::string scene_button_label =
      active_scene_id.has_value() ? *active_scene_id : std::string("Scenes");
  const bool has_active_scene = active_scene_id.has_value();
  const auto lifecycle_status =
      active_status.value_or(SceneLifecycleStatus{});
  const auto &theme = scene_panel_theme();

  if (m_scene_menu_trigger_label_node != ui::k_invalid_node_id) {
    m_document->set_text(m_scene_menu_trigger_label_node, scene_button_label);
  }

  if (m_scene_menu_trigger_icon_node != ui::k_invalid_node_id) {
    m_document->set_texture(
        m_scene_menu_trigger_icon_node,
        scene_panel::scene_menu_trigger_icon_texture(scene_menu_open())
    );
  }

  if (m_scene_mode_toggle_node != ui::k_invalid_node_id) {
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
    m_document->set_enabled(m_scene_mode_toggle_node, has_active_scene);
    m_document->set_segmented_selected_index(
        m_scene_mode_toggle_node, selected_index
    );
  }

  if (m_scene_save_button_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_scene_save_button_node,
        active_session_kind == SceneSessionKind::Source ||
            active_session_kind == SceneSessionKind::Preview ||
            active_session_kind == SceneSessionKind::Runtime
    );
  }

  if (m_scene_promote_button_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_scene_promote_button_node,
        scene_panel::can_promote_from_active_session(
            has_active_scene, active_session_kind
        )
    );
  }

  if (m_scene_playback_controls_node != ui::k_invalid_node_id) {
    m_document->set_visible(
        m_scene_playback_controls_node,
        active_session_kind == SceneSessionKind::Preview ||
            active_session_kind == SceneSessionKind::Runtime
    );
  }

  if (m_scene_play_button_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_scene_play_button_node,
        (active_session_kind == SceneSessionKind::Preview ||
         active_session_kind == SceneSessionKind::Runtime) &&
            active_execution_state != SceneExecutionState::Playing
    );
  }

  if (m_scene_pause_button_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_scene_pause_button_node,
        (active_session_kind == SceneSessionKind::Preview ||
         active_session_kind == SceneSessionKind::Runtime) &&
            active_execution_state == SceneExecutionState::Playing
    );
  }

  if (m_scene_stop_button_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_scene_stop_button_node,
        (active_session_kind == SceneSessionKind::Preview ||
         active_session_kind == SceneSessionKind::Runtime) &&
            active_execution_state != SceneExecutionState::Stopped
    );
  }

  if (m_scene_status_card_node != ui::k_invalid_node_id) {
    m_document->set_visible(m_scene_status_card_node, has_active_scene);
  }

  if (has_active_scene && active_status.has_value()) {
    if (m_scene_status_scene_value_node != ui::k_invalid_node_id) {
      m_document->set_text(m_scene_status_scene_value_node, *active_scene_id);
    }

    if (m_scene_status_source_value_node != ui::k_invalid_node_id) {
      m_document->set_text(
          m_scene_status_source_value_node,
          scene_panel::scene_source_save_state_label(active_status->source)
      );
    }

    if (m_scene_status_preview_value_node != ui::k_invalid_node_id) {
      m_document->set_text(
          m_scene_status_preview_value_node,
          scene_panel::scene_preview_state_label(active_status->preview)
      );
    }

    if (m_scene_status_runtime_value_node != ui::k_invalid_node_id) {
      m_document->set_text(
          m_scene_status_runtime_value_node,
          scene_panel::scene_runtime_state_label(active_status->runtime)
      );
    }

    if (m_scene_status_execution_value_node != ui::k_invalid_node_id) {
      const auto execution_state = active_execution_state.value_or(
          active_session_kind == SceneSessionKind::Source
              ? SceneExecutionState::Static
              : SceneExecutionState::Stopped
      );
      m_document->set_text(
          m_scene_status_execution_value_node,
          scene_panel::scene_execution_state_label(execution_state)
      );
      mutate_status_pill(
          *m_document,
          m_scene_status_execution_chip_node,
          m_scene_status_execution_value_node,
          scene_panel::scene_execution_state_color(theme, execution_state)
      );
    }

    const auto runtime_hint =
        scene_panel::scene_runtime_status_hint(
            *active_status, active_session_kind, active_execution_state
        );
    if (m_scene_status_runtime_hint_node != ui::k_invalid_node_id) {
      m_document->set_text(m_scene_status_runtime_hint_node, runtime_hint);
      m_document->set_visible(
          m_scene_status_runtime_hint_node, !runtime_hint.empty()
      );
    }

    mutate_status_pill(
        *m_document,
        m_scene_status_source_chip_node,
        m_scene_status_source_value_node,
        scene_panel::scene_source_save_state_color(theme, active_status->source)
    );
    mutate_status_pill(
        *m_document,
        m_scene_status_preview_chip_node,
        m_scene_status_preview_value_node,
        scene_panel::scene_preview_state_color(theme, active_status->preview)
    );
    mutate_status_pill(
        *m_document,
        m_scene_status_runtime_chip_node,
        m_scene_status_runtime_value_node,
        scene_panel::scene_runtime_state_color(theme, active_status->runtime)
    );
  } else if (runtime_prompt_open()) {
    close_runtime_prompt();
  }

  if (m_runtime_prompt_title_node != ui::k_invalid_node_id &&
      active_status.has_value()) {
    m_document->set_text(
        m_runtime_prompt_title_node,
        scene_panel::runtime_prompt_title(
            m_runtime_prompt_target_kind, *active_status
        )
    );
  }

  if (m_runtime_prompt_body_node != ui::k_invalid_node_id &&
      active_status.has_value()) {
    m_document->set_text(
        m_runtime_prompt_body_node,
        scene_panel::runtime_prompt_body(
            active_session_kind, m_runtime_prompt_target_kind, *active_status
        )
    );
  }

  if (force || scene_menu_open()) {
    rebuild_scene_menu_content();
  }
}

void ScenePanelController::rebuild_scene_menu_content() {
  if (m_document == nullptr ||
      m_scene_menu_content_node == ui::k_invalid_node_id) {
    return;
  }

  auto scene_manager = SceneManager::get();
  const auto scene_entries =
      scene_manager != nullptr ? scene_manager->get_scene_entries()
                               : std::vector<ProjectSceneEntryConfig>{};
  const auto active_scene_id =
      scene_manager != nullptr ? scene_manager->get_active_scene_id()
                               : std::optional<std::string>{};
  const auto &theme = scene_panel_theme();

  std::vector<const ProjectSceneEntryConfig *> filtered_entries;
  filtered_entries.reserve(scene_entries.size());
  for (const auto &entry : scene_entries) {
    if (scene_panel::scene_entry_matches_query(entry, m_scene_menu_query)) {
      filtered_entries.push_back(&entry);
    }
  }

  if (m_scene_menu_result_count_node != ui::k_invalid_node_id) {
    m_document->set_text(
        m_scene_menu_result_count_node,
        scene_panel::scene_result_count_label(filtered_entries.size())
    );
  }

  m_document->clear_children(m_scene_menu_content_node);

  const bool show_list = !filtered_entries.empty();
  if (m_scene_menu_list_node != ui::k_invalid_node_id) {
    m_document->set_visible(m_scene_menu_list_node, show_list);
  }

  if (m_scene_menu_empty_state_node != ui::k_invalid_node_id) {
    m_document->set_visible(m_scene_menu_empty_state_node, !show_list);
  }

  if (filtered_entries.empty()) {
    if (m_scene_menu_empty_title_node != ui::k_invalid_node_id) {
      m_document->set_text(
          m_scene_menu_empty_title_node,
          scene_entries.empty() ? "No scenes declared" : "No matching scenes"
      );
    }
    if (m_scene_menu_empty_body_node != ui::k_invalid_node_id) {
      m_document->set_text(
          m_scene_menu_empty_body_node,
          scene_entries.empty()
              ? "Add scene entries to the project manifest to switch them here."
              : "Try a scene id, scene type, or part of the path."
      );
    }
    return;
  }

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

    ui::dsl::append(
        *m_document,
        m_scene_menu_content_node,
        scene_panel::build_scene_menu_entry_item(
            *entry,
            active,
            presentation,
            [this, scene_id = entry->id]() { activate_scene(scene_id); },
            theme
        )
    );
  }
}

void ScenePanelController::open_scene_menu(bool reset_query) {
  if (m_document == nullptr || m_scene_menu_node == ui::k_invalid_node_id ||
      m_scene_menu_button_node == ui::k_invalid_node_id) {
    return;
  }

  if (reset_query && !m_scene_menu_query.empty()) {
    m_scene_menu_query.clear();
    if (m_scene_menu_search_input_node != ui::k_invalid_node_id) {
      m_document->set_text(m_scene_menu_search_input_node, {});
    }
    rebuild_scene_menu_content();
  }

  if (scene_menu_open()) {
    m_document->close_popover(m_scene_menu_node);
    return;
  }

  m_document->close_all_popovers();
  rebuild_scene_menu_content();
  m_document->open_popover_anchored_to(
      m_scene_menu_node,
      m_scene_menu_button_node,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
  if (m_scene_menu_search_input_node != ui::k_invalid_node_id) {
    m_document->request_focus(m_scene_menu_search_input_node);
  }
}

bool ScenePanelController::scene_menu_open() const {
  if (m_document == nullptr || m_scene_menu_node == ui::k_invalid_node_id) {
    return false;
  }

  const auto *node = m_document->node(m_scene_menu_node);
  return node != nullptr && node->type == ui::NodeType::Popover &&
         node->popover.open;
}

void ScenePanelController::open_runtime_prompt(SceneSessionKind target_kind) {
  auto scene_manager = SceneManager::get();
  const auto active_status =
      scene_manager != nullptr ? scene_manager->get_active_scene_lifecycle_status()
                               : std::optional<SceneLifecycleStatus>{};
  const auto current_kind =
      scene_manager != nullptr ? scene_manager->get_active_scene_session_kind()
                               : std::optional<SceneSessionKind>{};
  if (m_document == nullptr || m_runtime_prompt_node == ui::k_invalid_node_id ||
      m_scene_mode_toggle_node == ui::k_invalid_node_id ||
      !active_status.has_value()) {
    return;
  }

  m_runtime_prompt_target_kind = target_kind;
  if (m_runtime_prompt_title_node != ui::k_invalid_node_id) {
    m_document->set_text(
        m_runtime_prompt_title_node,
        scene_panel::runtime_prompt_title(
            m_runtime_prompt_target_kind, *active_status
        )
    );
  }

  if (m_runtime_prompt_body_node != ui::k_invalid_node_id) {
    m_document->set_text(
        m_runtime_prompt_body_node,
        scene_panel::runtime_prompt_body(
            current_kind, m_runtime_prompt_target_kind, *active_status
        )
    );
  }

  m_document->close_all_popovers();
  m_document->open_popover_anchored_to(
      m_runtime_prompt_node,
      m_scene_mode_toggle_node,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void ScenePanelController::close_runtime_prompt() {
  if (m_document == nullptr || m_runtime_prompt_node == ui::k_invalid_node_id) {
    return;
  }

  m_runtime_prompt_target_kind.reset();
  m_document->close_popover(m_runtime_prompt_node);
}

bool ScenePanelController::runtime_prompt_open() const {
  if (m_document == nullptr || m_runtime_prompt_node == ui::k_invalid_node_id) {
    return false;
  }

  const auto *node = m_document->node(m_runtime_prompt_node);
  return node != nullptr && node->type == ui::NodeType::Popover &&
         node->popover.open;
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
  if (m_document != nullptr && m_scene_menu_node != ui::k_invalid_node_id) {
    m_document->close_popover(m_scene_menu_node);
  }
  refresh(true);
}

} // namespace astralix::editor
