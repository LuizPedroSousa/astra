#pragma once

#include "panels/panel-controller.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralix::editor {

class FileBrowserPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 520.0f,
      .height = 340.0f,
  };

  struct DirectoryEntry {
    std::filesystem::path absolute_path;
    std::string path_key;
    std::string name;
    std::string extension;
    std::string size_label;
    std::string modified_label;
    bool is_directory = false;
    std::optional<std::uintmax_t> file_size;
    std::optional<std::filesystem::file_time_type> last_modified;
  };

  struct TreeVisibleRow {
    std::filesystem::path absolute_path;
    std::string path_key;
    std::string display_name;
    int depth = 0;
    bool is_expanded = false;
    bool is_selected = false;
    bool has_children = false;
    float height = 28.0f;
  };

  enum class ContentViewMode : uint8_t {
    Grid = 0,
    List,
  };

  enum class RootScope : uint8_t {
    Assets = 0,
    Files,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

private:
  struct DirectoryTreeNode {
    std::filesystem::path absolute_path;
    std::string path_key;
    std::string name;
    std::vector<DirectoryTreeNode> children;
  };

  void invalidate_runtime_handles();
  void resolve_root_path();
  void refresh(bool force = false);
  void refresh_now();
  void assign_current_directory(
      std::filesystem::path path,
      bool clear_selection = true
  );
  void ensure_current_directory_is_valid();
  void ensure_tree_expanded_for(const std::filesystem::path &path);
  void clear_selection_if_missing();

  std::filesystem::path sanitize_directory_path(
      const std::filesystem::path &path
  ) const;
  bool is_directory_under_root(const std::filesystem::path &path) const;
  bool is_tree_expanded(std::string_view key) const;
  bool tree_query_matches(std::string_view value) const;
  bool content_query_matches(std::string_view value) const;

  std::string path_key_for(const std::filesystem::path &path) const;
  std::filesystem::path path_for_key(std::string_view key) const;
  std::string display_name_for(const std::filesystem::path &path) const;

  DirectoryTreeNode scan_directory_tree() const;
  std::vector<DirectoryEntry> scan_directory_contents(
      const std::filesystem::path &directory
  );
  std::vector<TreeVisibleRow> build_tree_rows(const DirectoryTreeNode &root) const;
  bool append_tree_rows(
      std::vector<TreeVisibleRow> &rows,
      const DirectoryTreeNode &node,
      int depth
  ) const;

  void handle_tree_row_click(const TreeVisibleRow &row);
  void handle_content_click(const DirectoryEntry &entry);
  void activate_content_entry(const DirectoryEntry &entry);
  void set_view_mode(ContentViewMode mode);
  void set_root_scope(RootScope scope);
  void mark_render_dirty() { ++m_render_revision; }

  std::pair<std::string, std::string> content_empty_state() const;
  const DirectoryEntry *selected_entry() const;

  void render_toolbar(ui::im::Children &root);
  void render_directory_tree(ui::im::Children &parent);
  void render_directory_content(ui::im::Frame &ui, ui::im::Children &parent);
  void render_content_grid(ui::im::Frame &ui, ui::im::Children &parent);
  void render_content_list(ui::im::Children &parent);
  void render_breadcrumb_bar(ui::im::Children &root);
  void render_empty_content(ui::im::Children &parent) const;

  ui::im::Runtime *m_runtime = nullptr;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  std::filesystem::path m_root_path;
  std::string m_root_label = "Resources";
  std::filesystem::path m_current_directory;
  std::string m_current_directory_key = ".";

  std::unordered_map<std::string, bool> m_tree_expanded;
  std::vector<TreeVisibleRow> m_tree_rows;
  ui::im::WidgetId m_tree_widget = ui::im::k_invalid_widget_id;

  std::vector<DirectoryEntry> m_directory_contents;
  ContentViewMode m_view_mode = ContentViewMode::Grid;
  RootScope m_root_scope = RootScope::Assets;
  std::optional<std::string> m_selected_entry_key;
  ui::im::WidgetId m_content_widget = ui::im::k_invalid_widget_id;

  ui::im::WidgetId m_split_widget = ui::im::k_invalid_widget_id;
  float m_split_ratio = 0.26f;
  bool m_split_drag_active = false;
  float m_split_drag_origin_ratio = 0.26f;

  std::string m_search_query;
  bool m_directory_read_failed = false;
  bool m_reset_content_scroll = false;
  std::string m_assets_directory_key = ".";
  std::string m_files_directory_key = ".";

  std::optional<std::string> m_last_clicked_entry_key;
  double m_last_click_time = 0.0;
  double m_elapsed_time = 0.0;
  double m_filesystem_poll_elapsed = 0.0;
  uint64_t m_render_revision = 1u;
};

} // namespace astralix::editor
