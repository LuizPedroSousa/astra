#include "tools/scene/scene-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "managers/project-manager.hpp"
#include "tools/scene/helpers.hpp"
#include "tools/scene/styles.hpp"
#include "trace.hpp"

#include <algorithm>
#include <cmath>

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
  Scene *active_scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  const auto *scene_entry =
      has_active_scene && scene_manager != nullptr
          ? scene_manager->find_scene_entry(*active_scene_id)
          : nullptr;
  const auto lifecycle_status =
      active_scene_status.value_or(SceneLifecycleStatus{});
  const bool show_footer = has_active_scene;
  const bool show_status_bar =
      has_active_scene && active_scene_status.has_value() &&
      scene_panel::status_bar_visible(lifecycle_status);

  if (!active_scene_status.has_value()) {
    m_runtime_prompt_target_kind.reset();
  }

  auto root = ui.column("root").style(
      fill()
          .background(theme.shell_background)
          .gap(0.0f)
          .padding(0.0f)
          .overflow_hidden()
  );
  auto body = root.scroll_view("body").style(
      fill_x()
          .flex(1.0f)
          .min_height(px(0.0f))
          .background(theme.shell_background)
          .scroll_vertical()
          .scrollbar_auto()
          .scrollbar_thickness(8.0f)
          .scrollbar_track_color(glm::vec4(
              theme.panel_raised_background.r,
              theme.panel_raised_background.g,
              theme.panel_raised_background.b,
              0.92f
          ))
          .scrollbar_thumb_color(glm::vec4(
              theme.panel_border.r,
              theme.panel_border.g,
              theme.panel_border.b,
              0.96f
          ))
          .scrollbar_thumb_hovered_color(glm::vec4(
              theme.accent.r,
              theme.accent.g,
              theme.accent.b,
              0.78f
          ))
          .scrollbar_thumb_active_color(glm::vec4(
              theme.accent.r,
              theme.accent.g,
              theme.accent.b,
              0.86f
          ))
  );
  auto content = body.column("content").style(
      fill_x().padding_xy(8.0f, 6.0f).gap(6.0f)
  );

  {
    ASTRA_PROFILE_N("ScenePanel::controls");

    auto menu_button = content.pressable("scene-menu-button")
                           .on_click([this]() { open_scene_menu(); })
                           .style(scene_panel::selector_button_style(theme));
    m_scene_menu_button_widget = menu_button.widget_id();
    menu_button.text("label", std::string("\xe2\x96\xbe  ") + scene_button_label)
        .style(font_size(11.0f).text_color(theme.text_primary));
    menu_button.text("caption", "Scene")
        .style(font_size(10.0f).text_color(theme.accent));

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
        content.segmented_control(
                    "scene-mode-toggle", {"Source", "Preview", "Runtime"}, selected_index
                )
            .enabled(has_active_scene)
            .accent_colors({theme.accent, theme.accent, theme.accent})
            .on_select([this](size_t index, const std::string &) {
              if (index == 0u) {
                switch_active_scene_session(SceneSessionKind::Source);
              } else if (index == 1u) {
                switch_active_scene_session(SceneSessionKind::Preview);
              } else if (index == 2u) {
                switch_active_scene_session(SceneSessionKind::Runtime);
              }
            })
            .style(
                fill_x()
                    .height(px(26.0f))
                    .background(theme.panel_background)
                    .border(1.0f, glm::vec4(theme.panel_border.r, theme.panel_border.g, theme.panel_border.b, 0.6f))
                    .radius(6.0f)
            );
    m_scene_mode_toggle_widget = scene_mode_toggle.widget_id();
  }

  if (has_active_scene && active_scene_status.has_value()) {
    ASTRA_PROFILE_N("ScenePanel::pipeline");

    content.view("sep-pipeline").style(scene_panel::separator_style(theme));

    auto pipeline_section = content.column("pipeline").style(fill_x().gap(6.0f));
    pipeline_section.text("header", "Pipeline")
        .style(font_size(10.0f).text_color(theme.text_muted));

    const auto render_pipeline_row = [&](const char *row_name,
                                         const char *label,
                                         SceneSessionKind kind,
                                         const char *state_label,
                                         const glm::vec4 &state_color) {
      auto row = pipeline_section.row(row_name).style(
          fill_x().items_center().gap(8.0f)
      );
      row.text("label", label).style(
          width(px(44.0f)).shrink(0.0f).font_size(11.0f).text_color(theme.text_primary)
      );
      auto pill = row.view("pill").style(scene_panel::pipeline_pill_style(state_color));
      pill.text("value", state_label)
          .style(font_size(10.0f).text_color(state_color));
      row.spacer("spacer").style(grow(1.0f));

      auto revision_slot = row.view("revision-slot").style(
          width(px(72.0f)).shrink(0.0f).items_start().justify_start()
      );
      const auto revision_text =
          scene_panel::scene_session_revision_label(active_scene, kind);
      if (!revision_text.empty()) {
        revision_slot.text("value", revision_text).style(
            font_size(11.0f).text_color(theme.text_muted)
        );
      }

      auto detail_slot = row.view("detail-slot").style(
          width(px(120.0f)).shrink(0.0f).items_end().justify_end()
      );
      const auto detail_text = scene_entry != nullptr
                                   ? scene_panel::scene_artifact_activity_label(
                                         *scene_entry, kind
                                     )
                                   : std::string{};
      if (!detail_text.empty()) {
        detail_slot.text("value", detail_text).style(
            font_size(10.0f).text_color(
                glm::vec4(
                    theme.text_muted.r,
                    theme.text_muted.g,
                    theme.text_muted.b,
                    0.80f
                )
            )
        );
      }
    };

    render_pipeline_row(
        "source-row",
        "Source",
        SceneSessionKind::Source,
        scene_panel::scene_source_save_state_label(lifecycle_status.source),
        scene_panel::scene_source_save_state_color(theme, lifecycle_status.source)
    );
    render_pipeline_row(
        "preview-row",
        "Preview",
        SceneSessionKind::Preview,
        scene_panel::scene_preview_state_label(lifecycle_status.preview),
        scene_panel::scene_preview_state_color(theme, lifecycle_status.preview)
    );
    render_pipeline_row(
        "runtime-row",
        "Runtime",
        SceneSessionKind::Runtime,
        scene_panel::scene_runtime_state_label(lifecycle_status.runtime),
        scene_panel::scene_runtime_state_color(theme, lifecycle_status.runtime)
    );
  }

  if (has_active_scene && active_scene != nullptr) {
    ASTRA_PROFILE_N("ScenePanel::derived_entities");

    content.view("sep-derived").style(scene_panel::separator_style(theme));

    auto derived_section = content.column("derived").style(fill_x().gap(6.0f));
    auto derived_header = derived_section.row("header").style(
        fill_x().items_center().justify_between()
    );
    derived_header.text("label", "Derived Entities")
        .style(font_size(10.0f).text_color(theme.text_muted));
    derived_header.text("summary", scene_panel::derived_entity_summary(*active_scene))
        .style(font_size(10.0f).text_color(
            glm::vec4(theme.text_muted.r, theme.text_muted.g, theme.text_muted.b, 0.7f)
        ));

    const auto derived_entities =
        scene_panel::gather_derived_entity_info(*active_scene, theme);
    if (auto derived_list = derived_section.scroll_view("list").style(
        fill_x()
            .max_height(px(420.0f))
            .min_height(px(0.0f))
            .padding(ui::UIEdges{
                .left = 0.0f,
                .top = 0.0f,
                .right = 4.0f,
                .bottom = 0.0f,
            })
            .gap(6.0f)
            .scroll_vertical()
            .scrollbar_auto()
            .scrollbar_thickness(8.0f)
            .scrollbar_track_color(glm::vec4(
                theme.panel_raised_background.r,
                theme.panel_raised_background.g,
                theme.panel_raised_background.b,
                0.92f
            ))
            .scrollbar_thumb_color(glm::vec4(
                theme.panel_border.r,
                theme.panel_border.g,
                theme.panel_border.b,
                0.96f
            ))
            .scrollbar_thumb_hovered_color(glm::vec4(
                theme.accent.r,
                theme.accent.g,
                theme.accent.b,
                0.78f
            ))
            .scrollbar_thumb_active_color(glm::vec4(
                theme.accent.r,
                theme.accent.g,
                theme.accent.b,
                0.86f
            ))
    )) {
      auto derived_content = derived_list.column("content").style(fill_x().gap(6.0f));
      for (size_t index = 0u; index < derived_entities.size(); ++index) {
        const auto &entity = derived_entities[index];
        auto entry_scope = derived_content.item_scope("derived-entry", index);
        auto entry = entry_scope.column("entry").style(fill_x().gap(2.0f));
        auto row =
            entry.row("row").style(fill_x().items_center().justify_between().gap(12.0f));
        row.text("name", entity.name)
            .style(font_size(11.0f).text_color(theme.text_primary));
        auto pill =
            row.view("pill").style(scene_panel::entity_pill_style(entity.state_color));
        pill.text("value", entity.state_text)
            .style(font_size(10.0f).text_color(glm::vec4(
                entity.state_color.r,
                entity.state_color.g,
                entity.state_color.b,
                0.8f
            )));
        entry.text("detail", entity.detail)
            .style(font_size(10.0f).text_color(glm::vec4(
                theme.text_muted.r,
                theme.text_muted.g,
                theme.text_muted.b,
                0.8f
            )));
      }
    }
  }

  if (scene_entry != nullptr) {
    ASTRA_PROFILE_N("ScenePanel::artifacts");

    content.view("sep-artifacts").style(scene_panel::separator_style(theme));

    auto artifacts_section = content.column("artifacts").style(fill_x().gap(5.0f));
    artifacts_section.text("header", "Artifacts")
        .style(font_size(10.0f).text_color(theme.text_muted));

    auto artifact_list = scene_panel::gather_artifact_info(*scene_entry);
    for (size_t index = 0u; index < artifact_list.size(); ++index) {
      const auto &artifact = artifact_list[index];
      if (artifact.path.empty()) {
        continue;
      }
      auto item_scope = artifacts_section.item_scope("artifact", index);
      auto artifact_row = item_scope.row("row").style(scene_panel::artifact_row_style());
      artifact_row.text("label", artifact.label)
          .style(font_size(11.0f).text_color(theme.text_muted));
      artifact_row.text("path", artifact.path)
          .style(font_size(11.0f).text_color(
              glm::vec4(theme.text_primary.r, theme.text_primary.g, theme.text_primary.b, 0.8f)
          ));
      artifact_row.spacer("spacer").style(grow(1.0f));
      if (!artifact.size.empty()) {
        artifact_row.text("size", artifact.size)
            .style(font_size(11.0f).text_color(
                glm::vec4(theme.text_muted.r, theme.text_muted.g, theme.text_muted.b, 0.6f)
            ));
      }
    }
  }

  if (has_active_scene && active_scene != nullptr) {
    ASTRA_PROFILE_N("ScenePanel::serialization");

    content.view("sep-serialization").style(scene_panel::separator_style(theme));

    auto serialization_section =
        content.column("serialization").style(fill_x().gap(5.0f));
    serialization_section.text("header", "Serialization")
        .style(font_size(10.0f).text_color(theme.text_muted));

    const auto serialization =
        scene_panel::gather_serialization_summary(*active_scene);
    const auto render_serialization_row =
        [&](const char *row_name, const char *label, const std::string &value) {
          auto row = serialization_section.row(row_name).style(
              fill_x().items_center().gap(6.0f)
          );
          row.text("label", label).style(
              width(px(64.0f)).shrink(0.0f).font_size(11.0f).text_color(theme.text_muted)
          );
          row.text("value", value).style(
              font_size(11.0f).text_color(
                  glm::vec4(
                      theme.text_primary.r,
                      theme.text_primary.g,
                      theme.text_primary.b,
                      0.85f
                  )
              )
          );
        };

    render_serialization_row("format-row", "Format", serialization.format);
    render_serialization_row("components-row", "Components", serialization.components);
    render_serialization_row("snapshots-row", "Snapshots", serialization.snapshots);
  }

  if (show_footer) {
    auto footer = root.column("footer").style(
        fill_x()
            .shrink(0.0f)
            .padding_xy(8.0f, 6.0f)
            .gap(6.0f)
            .background(theme.shell_background)
    );

    ASTRA_PROFILE_N("ScenePanel::actions");

    footer.view("sep-actions").style(scene_panel::separator_style(theme));

    auto actions_section = footer.column("actions").style(fill_x().gap(6.0f));
    actions_section.text("header", "Actions")
        .style(font_size(10.0f).text_color(theme.text_muted));

    const auto save_label = [&]() -> std::string {
      switch (active_scene_session_kind.value_or(SceneSessionKind::Source)) {
        case SceneSessionKind::Source:
          return "Save Source";
        case SceneSessionKind::Preview:
          return "Save Preview";
        case SceneSessionKind::Runtime:
          return "Save Runtime";
      }
      return "Save Source";
    }();

    auto save_button = actions_section.pressable("save-source")
                           .on_click([this]() { save_active_scene(); })
                           .style(scene_panel::action_button_primary_style(theme));
    save_button.text("label", save_label)
        .style(font_size(11.0f).text_color(theme.accent));

    auto promote_row = actions_section.row("promote-row").style(fill_x().gap(8.0f));

    auto promote_preview_button = promote_row.pressable("promote-preview")
                                      .enabled(
                                          active_scene_session_kind == SceneSessionKind::Source
                                      )
                                      .on_click([this]() {
                                        auto manager = SceneManager::get();
                                        if (manager == nullptr) return;
                                        auto scene_id = manager->get_active_scene_id();
                                        if (!scene_id.has_value()) return;
                                        manager->promote_source_to_preview(*scene_id);
                                        refresh(true);
                                      })
                                      .style(scene_panel::action_button_secondary_style(theme));
    promote_preview_button.text("label", "Promote to Preview")
        .style(font_size(11.0f).text_color(
            glm::vec4(theme.text_primary.r, theme.text_primary.g, theme.text_primary.b, 0.9f)
        ));

    auto promote_runtime_button = promote_row.pressable("promote-runtime")
                                      .enabled(
                                          active_scene_session_kind == SceneSessionKind::Preview
                                      )
                                      .on_click([this]() {
                                        auto manager = SceneManager::get();
                                        if (manager == nullptr) return;
                                        auto scene_id = manager->get_active_scene_id();
                                        if (!scene_id.has_value()) return;
                                        manager->promote_preview(*scene_id);
                                        refresh(true);
                                      })
                                      .style(scene_panel::action_button_secondary_style(theme));
    promote_runtime_button.text("label", "Promote to Runtime")
        .style(font_size(11.0f).text_color(
            glm::vec4(theme.text_primary.r, theme.text_primary.g, theme.text_primary.b, 0.9f)
        ));
    if (show_status_bar) {
      ASTRA_PROFILE_N("ScenePanel::status_bar");

      auto status_bar =
          footer.row("status-bar").style(scene_panel::status_bar_style(theme));
      status_bar.view("accent-border").style(
          width(px(3.0f)).fill_y().background(theme.accent)
      );

      auto status_content = status_bar.column("status-content").style(
          grow(1.0f).padding_xy(14.0f, 10.0f).gap(5.0f)
      );

      auto warning_text =
          scene_panel::status_bar_warning(lifecycle_status, active_scene);
      if (!warning_text.empty()) {
        status_content.text("warning", std::string("\xe2\x9a\xa0  ") + warning_text)
            .style(font_size(11.0f).text_color(glm::vec4(
                theme.accent.r,
                theme.accent.g,
                theme.accent.b,
                0.9f
            )));
      }

      auto hint_text = scene_panel::status_bar_hint(lifecycle_status);
      if (!hint_text.empty()) {
        status_content.text("hint", hint_text)
            .style(font_size(10.0f).text_color(glm::vec4(
                theme.text_primary.r,
                theme.text_primary.g,
                theme.text_primary.b,
                0.7f
            )));
      }

      status_content.view("target-sep").style(
          fill_x().height(px(1.0f)).background(glm::vec4(1.0f, 1.0f, 1.0f, 0.15f))
      );

      auto project = active_project();
      const char *target_label = "Source";
      if (project != nullptr) {
        target_label = scene_panel::startup_target_label(
            project->get_config().scenes.startup_target
        );
      }
      status_content.text("target", std::string("Startup target: ") + target_label)
          .style(font_size(10.0f).text_color(glm::vec4(
              theme.text_primary.r,
              theme.text_primary.g,
              theme.text_primary.b,
              0.5f
          )));
    }
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
            .scrollbar_auto()
            .scrollbar_thickness(8.0f)
            .scrollbar_track_color(glm::vec4(
                theme.panel_raised_background.r,
                theme.panel_raised_background.g,
                theme.panel_raised_background.b,
                0.92f
            ))
            .scrollbar_thumb_color(glm::vec4(
                theme.panel_border.r,
                theme.panel_border.g,
                theme.panel_border.b,
                0.96f
            ))
            .scrollbar_thumb_hovered_color(glm::vec4(
                theme.accent.r,
                theme.accent.g,
                theme.accent.b,
                0.78f
            ))
            .scrollbar_thumb_active_color(glm::vec4(
                theme.accent.r,
                theme.accent.g,
                theme.accent.b,
                0.86f
            ))
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
                             .style(scene_panel::action_button_primary_style(theme));
    promote_enter.text("label", "Promote + Enter")
        .style(font_size(13.0f).text_color(theme.text_primary));
    auto cancel = prompt_actions.pressable("cancel")
                      .on_click([this]() { close_runtime_prompt(); })
                      .style(scene_panel::action_button_secondary_style(theme));
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

  if (scene_manager != nullptr) {
    if (Scene *active_scene = scene_manager->get_active_scene();
        active_scene != nullptr) {
      hash = fnv1a64_append_value(hash, active_scene->world().revision());

      const auto &derived_state = active_scene->get_derived_state();
      hash = fnv1a64_append_value(hash, derived_state.overrides.size());
      for (const auto &override_record : derived_state.overrides) {
        hash = fnv1a64_append_string(override_record.key.generator_id, hash);
        hash = fnv1a64_append_string(override_record.key.stable_key, hash);
        hash = fnv1a64_append_value(hash, override_record.active);
        hash = fnv1a64_append_value(hash, override_record.name.has_value());
        if (override_record.name.has_value()) {
          hash = fnv1a64_append_string(*override_record.name, hash);
        }
        hash = fnv1a64_append_value(hash, override_record.removed_components.size());
        for (const auto &removed_component : override_record.removed_components) {
          hash = fnv1a64_append_string(removed_component, hash);
        }
        hash = fnv1a64_append_value(hash, override_record.components.size());
        for (const auto &component : override_record.components) {
          hash = fnv1a64_append_string(component.name, hash);
        }
      }

      hash = fnv1a64_append_value(hash, derived_state.suppressions.size());
      for (const auto &suppression : derived_state.suppressions) {
        hash = fnv1a64_append_string(suppression.key.generator_id, hash);
        hash = fnv1a64_append_string(suppression.key.stable_key, hash);
      }

      const auto &preview_info = active_scene->get_preview_build_info();
      hash = fnv1a64_append_value(hash, preview_info.has_value());
      if (preview_info.has_value()) {
        hash = fnv1a64_append_value(hash, preview_info->source_revision);
        hash = fnv1a64_append_string(preview_info->built_at_utc, hash);
      }

      const auto &runtime_info = active_scene->get_runtime_promotion_info();
      hash = fnv1a64_append_value(hash, runtime_info.has_value());
      if (runtime_info.has_value()) {
        hash = fnv1a64_append_value(
            hash, runtime_info->promoted_from_preview_revision
        );
        hash = fnv1a64_append_string(runtime_info->promoted_at_utc, hash);
      }
    }

    if (active_scene_id.has_value()) {
      if (const auto *entry = scene_manager->find_scene_entry(*active_scene_id);
          entry != nullptr) {
        hash = fnv1a64_append_string(entry->source_path, hash);
        hash = fnv1a64_append_string(entry->preview_path, hash);
        hash = fnv1a64_append_string(entry->runtime_path, hash);

        const auto artifact_info = scene_panel::gather_artifact_info(*entry);
        hash = fnv1a64_append_value(hash, artifact_info.size());
        for (const auto &artifact : artifact_info) {
          hash = fnv1a64_append_string(artifact.path, hash);
          hash = fnv1a64_append_string(artifact.size, hash);
        }
      }
    }
  }

  if (auto project = active_project(); project != nullptr) {
    hash = fnv1a64_append_value(
        hash, project->get_config().scenes.startup_target
    );
    hash = fnv1a64_append_value(
        hash, project->get_config().serialization.format
    );
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
