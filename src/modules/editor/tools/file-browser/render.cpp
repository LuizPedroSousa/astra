#include "tools/file-browser/file-browser-panel-controller.hpp"

#include "editor-theme.hpp"
#include "managers/resource-manager.hpp"
#include "resources/svg.hpp"
#include "resources/texture.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

constexpr float k_tree_row_indent = 14.0f;
constexpr float k_tree_toggle_size = 18.0f;
constexpr float k_tile_width = 120.0f;
constexpr float k_tile_height = 96.0f;
constexpr float k_tile_gap = 8.0f;

ResourceDescriptorID resolve_image(
    std::initializer_list<std::string_view> descriptor_ids
) {
  for (const std::string_view descriptor_id : descriptor_ids) {
    if (descriptor_id.empty()) {
      continue;
    }

    const ResourceDescriptorID candidate{descriptor_id};
    if (resource_manager()->get_by_descriptor_id<Texture2D>(candidate) != nullptr ||
        resource_manager()->get_by_descriptor_id<Svg>(candidate) != nullptr) {
      return candidate;
    }
  }

  return {};
}

ResourceDescriptorID directory_icon() {
  return resolve_image({"icons::directory", "icons::cube"});
}

ResourceDescriptorID file_icon() {
  return resolve_image({"icons::file", "icons::cube"});
}

ResourceDescriptorID chevron_icon(bool expanded) {
  return resolve_image(
      {expanded ? "icons::right_arrow_down" : "icons::right_arrow"}
  );
}

std::string extension_badge(std::string_view extension) {
  if (extension.empty()) {
    return "FILE";
  }

  std::string label(extension);
  if (!label.empty() && label.front() == '.') {
    label.erase(label.begin());
  }
  std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return label.empty() ? "FILE" : label;
}

ui::dsl::StyleBuilder input_style(
    const FileBrowserPanelTheme &theme,
    ResourceDescriptorID font_id,
    float font_size_value
) {
  return background(theme.card_background)
      .border(1.0f, theme.card_border)
      .radius(8.0f)
      .font_id(std::move(font_id))
      .font_size(std::max(13.0f, font_size_value * 0.78f))
      .text_color(theme.text_primary)
      .placeholder_text_color(theme.text_muted)
      .selection_color(theme.accent_soft)
      .caret_color(theme.accent);
}

ui::dsl::StyleBuilder toolbar_button_style(
    const FileBrowserPanelTheme &theme,
    bool active = false
) {
  return items_center()
      .justify_center()
      .padding_xy(10.0f, 7.0f)
      .radius(8.0f)
      .background(active ? theme.accent_soft : theme.card_background)
      .border(
          1.0f,
          active ? theme.accent : theme.card_border
      )
      .text_color(active ? theme.accent : theme.text_primary)
      .cursor_pointer()
      .hover(
          state()
              .background(active ? theme.accent_soft : theme.panel_background)
              .border(1.0f, active ? theme.accent : theme.separator)
      )
      .pressed(state().background(theme.panel_background))
      .focused(state().border(2.0f, theme.accent));
}

ui::dsl::StyleBuilder pane_style(const FileBrowserPanelTheme &theme) {
  return fill_y()
      .min_height(px(0.0f))
      .padding(8.0f)
      .gap(6.0f)
      .radius(8.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border)
      .overflow_hidden();
}

ui::dsl::StyleBuilder divider_style(
    const FileBrowserPanelTheme &theme,
    bool active
) {
  return width(px(8.0f))
      .fill_y()
      .radius(999.0f)
      .background(active ? theme.accent_soft : theme_alpha(theme.separator, 0.28f))
      .border(1.0f, active ? theme.accent : theme_alpha(theme.separator, 0.18f))
      .cursor_pointer()
      .hover(
          state()
              .background(theme_alpha(theme.accent, 0.10f))
              .border(1.0f, theme_alpha(theme.accent, 0.4f))
      )
      .pressed(
          state()
              .background(theme.accent_soft)
              .border(1.0f, theme.accent)
      );
}

} // namespace

void FileBrowserPanelController::render(ui::im::Frame &ui) {
  const FileBrowserPanelTheme theme;

  auto root = ui.column("file-browser-root").style(
      fill().background(theme.shell_background).padding(12.0f).gap(10.0f)
  );

  render_toolbar(root);

  auto split = root.row("split").style(
      fill_x().flex(1.0f).min_height(px(0.0f)).gap(8.0f)
  );
  m_split_widget = split.widget_id();

  float split_width = kMinimumSize.width;
  if (m_runtime != nullptr && m_split_widget != ui::im::k_invalid_widget_id) {
    if (const auto bounds = m_runtime->layout_bounds(m_split_widget);
        bounds.has_value() && bounds->width > 0.0f) {
      split_width = bounds->width;
    }
  }

  const float min_tree_width = 180.0f;
  const float max_tree_width = std::max(min_tree_width, split_width - 220.0f);
  const float tree_width =
      std::clamp(split_width * m_split_ratio, min_tree_width, max_tree_width);

  auto tree_pane = split.column("tree-pane")
                       .style(pane_style(theme), width(px(tree_width)));
  render_directory_tree(tree_pane);

  split.pressable("divider")
      .style(divider_style(theme, m_split_drag_active))
      .on_pointer_event([this](const ui::UIPointerEvent &event) {
        switch (event.phase) {
          case ui::UIPointerEventPhase::DragStart:
            m_split_drag_active = true;
            m_split_drag_origin_ratio = m_split_ratio;
            mark_render_dirty();
            break;
          case ui::UIPointerEventPhase::DragUpdate: {
            if (m_runtime == nullptr || m_split_widget == ui::im::k_invalid_widget_id) {
              break;
            }
            const auto bounds = m_runtime->layout_bounds(m_split_widget);
            if (!bounds.has_value() || bounds->width <= 1.0f) {
              break;
            }

            const float next_ratio = std::clamp(
                m_split_drag_origin_ratio + (event.total_delta.x / bounds->width),
                0.18f,
                0.45f
            );
            if (std::fabs(next_ratio - m_split_ratio) > 0.001f) {
              m_split_ratio = next_ratio;
              mark_render_dirty();
            }
            break;
          }
          case ui::UIPointerEventPhase::DragEnd:
          case ui::UIPointerEventPhase::Release:
            if (m_split_drag_active) {
              m_split_drag_active = false;
              mark_render_dirty();
            }
            break;
          default:
            break;
        }
      });

  auto content_pane = split.column("content-pane")
                          .style(pane_style(theme), flex(1.0f), min_width(px(0.0f)));
  render_breadcrumb_bar(content_pane);
  render_directory_content(ui, content_pane);
}

void FileBrowserPanelController::render_toolbar(ui::im::Children &root) {
  const FileBrowserPanelTheme theme;

  auto toolbar = root.row("toolbar").style(fill_x().items_center().gap(8.0f));
  toolbar.text_input("search", m_search_query, "Search files and directories")
      .select_all_on_focus(true)
      .on_change([this](const std::string &value) {
        m_search_query = value;
        refresh(true);
      })
      .style(
          input_style(theme, m_default_font_id, m_default_font_size),
          width(px(360.0f)),
          min_width(px(200.0f)),
          max_width(px(420.0f))
      );

  toolbar.pressable("grid-button")
      .on_click([this]() { set_view_mode(ContentViewMode::Grid); })
      .style(toolbar_button_style(theme, m_view_mode == ContentViewMode::Grid))
      .text("label", "Grid")
      .style(font_size(12.0f));

  toolbar.pressable("list-button")
      .on_click([this]() { set_view_mode(ContentViewMode::List); })
      .style(toolbar_button_style(theme, m_view_mode == ContentViewMode::List))
      .text("label", "List")
      .style(font_size(12.0f));

  toolbar.spacer("toolbar-spacer").style(grow(1.0f));

  toolbar.pressable("refresh-button")
      .on_click([this]() { refresh_now(); })
      .style(toolbar_button_style(theme))
      .text("label", "Refresh")
      .style(font_size(12.0f));
}

void FileBrowserPanelController::render_directory_tree(ui::im::Children &parent) {
  const FileBrowserPanelTheme theme;

  if (m_root_path.empty()) {
    m_tree_widget = ui::im::k_invalid_widget_id;
    auto empty = parent.column("tree-empty").style(
        fill().justify_center().items_center().padding(16.0f).gap(6.0f)
    );
    empty.text("title", m_root_scope == RootScope::Files ? "No project tree"
                                                         : "No resource tree")
        .style(font_size(14.0f).text_color(theme.text_primary));
    empty.text(
            "body",
            m_root_scope == RootScope::Files
                ? "Open a project to browse its files."
                : "Open a project to browse its resources."
        )
        .style(font_size(12.0f).text_color(theme.text_muted));
    return;
  }

  auto scope_switch = parent.row("root-scope").style(fill_x().gap(8.0f));
  scope_switch.pressable("assets-button")
      .on_click([this]() { set_root_scope(RootScope::Assets); })
      .style(
          toolbar_button_style(theme, m_root_scope == RootScope::Assets),
          flex(1.0f)
      )
      .text("label", "Assets")
      .style(font_size(12.0f));

  scope_switch.pressable("files-button")
      .on_click([this]() { set_root_scope(RootScope::Files); })
      .style(
          toolbar_button_style(theme, m_root_scope == RootScope::Files),
          flex(1.0f)
      )
      .text("label", "Files")
      .style(font_size(12.0f));

  auto list = parent.virtual_list(
      "tree",
      m_tree_rows.size(),
      [this](size_t index) { return m_tree_rows[index].height; },
      [this, &theme](ui::im::Children &visible_scope, size_t start, size_t end) {
        for (size_t index = start; index <= end; ++index) {
          const TreeVisibleRow &row = m_tree_rows[index];
          auto row_scope = visible_scope.item_scope("tree-row", row.path_key);
          auto shell = row_scope.row("shell").style(
              fill_x()
                  .items_center()
                  .gap(2.0f)
                  .min_height(px(row.height))
                  .padding(ui::UIEdges{
                      .left = 6.0f + static_cast<float>(row.depth) * k_tree_row_indent,
                      .top = 3.0f,
                      .right = 6.0f,
                      .bottom = 3.0f,
                  })
                  .radius(8.0f)
                  .background(
                      row.is_selected ? theme.row_selected_background
                                      : theme_alpha(theme.panel_background, 0.0f)
                  )
                  .border(
                      row.is_selected ? 1.0f : 0.0f,
                      row.is_selected ? theme.row_selected_border
                                      : theme_alpha(theme.card_border, 0.0f)
                  )
          );

          if (row.has_children) {
            auto toggle = shell.pressable("toggle")
                              .on_click([this, key = row.path_key]() {
                                m_tree_expanded[key] = !is_tree_expanded(key);
                                refresh(true);
                              })
                              .style(
                                  items_center()
                                      .justify_center()
                                      .width(px(k_tree_toggle_size))
                                      .height(px(k_tree_toggle_size))
                                      .shrink(0.0f)
                                      .radius(6.0f)
                                      .background(theme_alpha(theme.panel_background, 0.0f))
                                      .border(1.0f, theme_alpha(theme.card_border, 0.0f))
                                      .cursor_pointer()
                                      .hover(
                                          state()
                                              .background(theme_alpha(theme.panel_background, 0.56f))
                                              .border(1.0f, theme_alpha(theme.card_border, 0.3f))
                                      )
                              );
            toggle.image("icon", chevron_icon(row.is_expanded))
                .style(
                    width(px(12.0f))
                        .height(px(12.0f))
                        .tint(theme.text_muted)
                );
          } else {
            shell.spacer("toggle-gap")
                .style(width(px(k_tree_toggle_size)).height(px(k_tree_toggle_size)).shrink(0.0f));
          }

          auto select = shell.pressable("select")
                            .on_click([this, row]() { handle_tree_row_click(row); })
                            .style(
                                ui::dsl::styles::row()
                                    .fill_x()
                                    .items_center()
                                    .justify_start()
                                    .gap(8.0f)
                                    .padding_xy(8.0f, 6.0f)
                                    .radius(8.0f)
                                    .background(theme_alpha(theme.panel_background, 0.0f))
                                    .border(1.0f, theme_alpha(theme.card_border, 0.0f))
                                    .cursor_pointer()
                                    .hover(
                                        state()
                                            .background(theme_alpha(theme.panel_background, 0.68f))
                                            .border(1.0f, theme_alpha(theme.card_border, 0.2f))
                                    )
                                    .pressed(
                                        state().background(theme_alpha(theme.panel_background, 0.8f))
                                    )
                            );

          auto icon_shell = select.view("icon-shell").style(
              width(px(18.0f))
                  .height(px(18.0f))
                  .shrink(0.0f)
                  .items_center()
                  .justify_center()
                  .radius(6.0f)
                  .background(theme_alpha(theme.panel_background, 0.5f))
          );
          icon_shell.image("icon", directory_icon())
              .style(
                  width(px(14.0f))
                      .height(px(14.0f))
                      .tint(row.is_selected ? theme.accent : theme.text_muted)
              );

          select.text("label", row.display_name)
              .style(
                  font_size(12.5f)
                      .text_color(row.is_selected ? theme.text_primary
                                                  : theme.text_muted)
              );
        }
      }
  );

  m_tree_widget = list.widget_id();
  list.style(
      fill().min_height(px(0.0f)).scroll_vertical().scrollbar_auto()
  );
}

void FileBrowserPanelController::render_directory_content(
    ui::im::Frame &ui,
    ui::im::Children &parent
) {
  if (m_directory_contents.empty()) {
    m_content_widget = ui::im::k_invalid_widget_id;
    render_empty_content(parent);
    return;
  }

  if (m_view_mode == ContentViewMode::Grid) {
    render_content_grid(ui, parent);
  } else {
    render_content_list(parent);
  }

  if (m_reset_content_scroll &&
      m_content_widget != ui::im::k_invalid_widget_id) {
    ui.set_scroll_offset(m_content_widget, glm::vec2(0.0f));
    m_reset_content_scroll = false;
  }
}

void FileBrowserPanelController::render_content_grid(
    ui::im::Frame &,
    ui::im::Children &parent
) {
  const FileBrowserPanelTheme theme;

  auto scroll = parent.scroll_view("content-grid").style(
      fill().min_height(px(0.0f)).overflow_hidden().scroll_vertical()
  );
  m_content_widget = scroll.widget_id();

  float viewport_width = 420.0f;
  if (m_runtime != nullptr && m_content_widget != ui::im::k_invalid_widget_id) {
    const auto state = m_runtime->virtual_list_state(m_content_widget);
    if (state.viewport_width > 0.0f) {
      viewport_width = state.viewport_width;
    }
  }

  const float usable_width = std::max(1.0f, viewport_width - 4.0f);
  const size_t columns = std::max<size_t>(
      1u,
      static_cast<size_t>((usable_width + k_tile_gap) / (k_tile_width + k_tile_gap))
  );

  auto rows = scroll.column("rows").style(fill_x().gap(k_tile_gap).padding(2.0f));
  for (size_t row_start = 0; row_start < m_directory_contents.size();
       row_start += columns) {
    auto row = rows.row("grid-row").style(fill_x().gap(k_tile_gap));
    const size_t row_end =
        std::min(m_directory_contents.size(), row_start + columns);
    for (size_t index = row_start; index < row_end; ++index) {
      const DirectoryEntry &entry = m_directory_contents[index];
      const bool selected =
          m_selected_entry_key.has_value() &&
          *m_selected_entry_key == entry.path_key;

      auto tile_scope = row.item_scope("content-entry", entry.path_key);
      auto tile = tile_scope.pressable("tile")
                      .on_click([this, entry]() { handle_content_click(entry); })
                      .style(
                          column()
                              .width(px(k_tile_width))
                              .min_height(px(k_tile_height))
                              .padding(10.0f)
                              .gap(8.0f)
                              .items_center()
                              .justify_center()
                              .radius(8.0f)
                              .background(
                                  selected ? theme.row_selected_background
                                           : theme.tile_background
                              )
                              .border(
                                  1.0f,
                                  selected ? theme.row_selected_border
                                           : theme.tile_border
                              )
                              .cursor_pointer()
                              .hover(
                                  state()
                                      .background(theme_alpha(theme.accent, 0.12f))
                                      .border(1.0f, theme_alpha(theme.accent, 0.32f))
                              )
                              .pressed(
                                  state().background(theme_alpha(theme.accent, 0.18f))
                              )
                      );

      auto icon_shell = tile.view("icon-shell").style(
          width(px(38.0f))
              .height(px(38.0f))
              .items_center()
              .justify_center()
              .radius(8.0f)
              .background(theme_alpha(theme.panel_background, 0.62f))
      );
      icon_shell.image("icon", entry.is_directory ? directory_icon() : file_icon())
          .style(
              width(px(26.0f))
                  .height(px(26.0f))
                  .tint(entry.is_directory ? theme.accent : theme.text_muted)
          );

      tile.text("name", entry.name)
          .style(font_size(12.0f).text_color(theme.text_primary));
      tile.text(
              "meta",
              entry.is_directory ? "Folder" : extension_badge(entry.extension)
          )
          .style(font_size(10.5f).text_color(theme.text_muted));
    }
  }
}

void FileBrowserPanelController::render_content_list(ui::im::Children &parent) {
  const FileBrowserPanelTheme theme;

  auto list = parent.virtual_list(
      "content-list",
      m_directory_contents.size(),
      [](size_t) { return 32.0f; },
      [this, &theme](ui::im::Children &visible_scope, size_t start, size_t end) {
        for (size_t index = start; index <= end; ++index) {
          const DirectoryEntry &entry = m_directory_contents[index];
          const bool selected =
              m_selected_entry_key.has_value() &&
              *m_selected_entry_key == entry.path_key;

          auto row_scope = visible_scope.item_scope("content-entry", entry.path_key);
          auto item = row_scope.pressable("row")
                          .on_click([this, entry]() { handle_content_click(entry); })
                          .style(
                              row()
                                  .fill_x()
                                  .items_center()
                                  .justify_between()
                                  .gap(12.0f)
                                  .min_height(px(32.0f))
                                  .padding_xy(10.0f, 6.0f)
                                  .radius(8.0f)
                                  .background(
                                      selected ? theme.row_selected_background
                                               : theme_alpha(theme.panel_background, 0.0f)
                                  )
                                  .border(
                                      1.0f,
                                      selected ? theme.row_selected_border
                                               : theme_alpha(theme.card_border, 0.0f)
                                  )
                                  .cursor_pointer()
                                  .hover(
                                      state()
                                          .background(theme_alpha(theme.panel_background, 0.56f))
                                          .border(1.0f, theme_alpha(theme.card_border, 0.18f))
                                  )
                                  .pressed(
                                      state().background(theme_alpha(theme.panel_background, 0.72f))
                                  )
                          );

          auto main = item.row("main").style(
              items_center().gap(10.0f).grow(1.0f).min_width(px(0.0f))
          );
          auto icon_shell = main.view("icon-shell").style(
              width(px(22.0f))
                  .height(px(22.0f))
                  .shrink(0.0f)
                  .items_center()
                  .justify_center()
                  .radius(7.0f)
                  .background(theme_alpha(theme.panel_background, 0.58f))
          );
          icon_shell.image("icon", entry.is_directory ? directory_icon() : file_icon())
              .style(
                  width(px(16.0f))
                      .height(px(16.0f))
                      .tint(entry.is_directory ? theme.accent : theme.text_muted)
              );

          main.text("name", entry.name)
              .style(font_size(12.5f).text_color(theme.text_primary));

          auto meta = item.row("meta").style(items_center().gap(16.0f).shrink(0.0f));
          meta.text("size", entry.is_directory ? "" : entry.size_label)
              .style(font_size(11.0f).text_color(theme.text_muted));
          meta.text("modified", entry.modified_label)
              .style(font_size(11.0f).text_color(theme.text_muted));
        }
      }
  );

  m_content_widget = list.widget_id();
  list.style(fill().min_height(px(0.0f)).scroll_vertical().scrollbar_auto());
}

void FileBrowserPanelController::render_breadcrumb_bar(ui::im::Children &root) {
  const FileBrowserPanelTheme theme;

  auto bar = root.row("breadcrumb").style(
      fill_x()
          .items_center()
          .gap(6.0f)
          .padding_xy(10.0f, 8.0f)
          .radius(8.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
  );

  if (m_root_path.empty()) {
    bar.text(
            "empty",
            m_root_scope == RootScope::Files ? "No project directory"
                                             : "No resource directory"
        )
        .style(font_size(12.0f).text_color(theme.text_muted));
    return;
  }

  std::filesystem::path cursor = m_root_path;
  size_t segment_index = 0u;
  const auto add_segment = [&](std::string label, const std::filesystem::path &path) {
    if (segment_index > 0u) {
      bar.text(std::string("sep-") + std::to_string(segment_index), ">")
          .style(font_size(12.0f).text_color(theme.text_muted));
    }

    bar.pressable(std::string("segment-") + std::to_string(segment_index))
        .on_click([this, path]() {
          assign_current_directory(path);
          refresh(true);
        })
        .style(
            row()
                .items_center()
                .justify_center()
                .padding_xy(6.0f, 4.0f)
                .radius(8.0f)
                .background(theme_alpha(theme.panel_background, 0.0f))
                .border(1.0f, theme_alpha(theme.card_border, 0.0f))
                .cursor_pointer()
                .hover(
                    state()
                        .background(theme_alpha(theme.panel_background, 0.6f))
                        .border(1.0f, theme_alpha(theme.card_border, 0.2f))
                )
        )
        .text("label", std::move(label))
        .style(font_size(12.0f).text_color(theme.text_muted));

    ++segment_index;
  };

  add_segment(m_root_label, m_root_path);

  const auto relative =
      m_current_directory.lexically_normal().lexically_relative(m_root_path.lexically_normal());
  if (relative.empty() || relative == ".") {
    return;
  }

  for (const auto &part : relative) {
    cursor /= part;
    add_segment(part.string(), cursor);
  }
}

void FileBrowserPanelController::render_empty_content(
    ui::im::Children &parent
) const {
  const FileBrowserPanelTheme theme;
  const auto [title, body] = content_empty_state();

  auto empty = parent.column("empty-content").style(
      fill()
          .justify_center()
          .items_center()
          .padding(24.0f)
          .gap(8.0f)
          .radius(8.0f)
          .background(theme_alpha(theme.panel_background, 0.42f))
          .border(1.0f, theme_alpha(theme.panel_border, 0.6f))
  );
  empty.text("title", title)
      .style(font_size(16.0f).text_color(theme.text_primary));
  empty.text("body", body)
      .style(font_size(12.0f).text_color(theme.text_muted));
}

} // namespace astralix::editor
